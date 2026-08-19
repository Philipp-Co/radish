#include <radish/game/game.h>

#include <radish/server/interface/command.h>
#include <radish/server/control/execute.h>
#include <radish/server/control/loader.h>

#include <zucchini/api/api.h>
#include <zucchini/ipc/ring_buffer.h>

#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>

///
/// Der Server ist die Gegenseite des Clients: er haelt den Spielzustand und
/// haengt ueber die Zucchini-Api daran, was von draussen hereinkommt.
///
/// Zucchini nimmt die UDP-Pakete an; dieses Programm ist aus dessen Sicht ein
/// lokaler Client, der ueber zwei Ringpuffer im Shared Memory angebunden ist
/// (siehe zucchini/api/api.h). Es gibt also zwei Prozesse: zucchini_server und
/// diesen hier.
///
///     Browser ──► Relay ──UDP──► zucchini_server ──Ringpuffer──► server
///
/// Der Name unten muss zu dem passen, unter dem die Zucchini-Instanz laeuft --
/// aus ihm leiten beide Seiten die Namen der Ringpuffer und der Wakeup-FIFO ab.
/// "zucchini" ist der Default von zucchini_server (ZUC_SERVER_NAME in dessen
/// main.c); ein anderer laesst sich als erstes Argument uebergeben.
///
///     server [zucchini-name] [spielstand.json]
///
/// Das zweite Argument ist ein Spielstand im Speicherformat (siehe
/// radish/serialization). Ohne ihn faengt der Server mit einem leeren Spiel an;
/// mit einem, der sich nicht lesen laesst, faengt er gar nicht erst an.
///
#define RAD_SERVER_DEFAULT_INTERFACE_NAME "zucchini"

///
/// Obergrenze eines Ringpuffer-Slots und damit einer Nachricht. Kommt aus
/// zucchini, statt hier als 1024 zu stehen -- laenger kann ohnehin nichts
/// ankommen.
///
#define RAD_SERVER_MESSAGE_SIZE ZUC_RING_BUFFER_SLOT_SIZE

///
/// Wie lange auf die Wakeup-FIFO gewartet wird, bevor die Schleife ohnehin
/// einmal durchlaeuft. Der Ausstieg per Signal wird dadurch spaetestens nach
/// dieser Zeit wirksam.
///
#define RAD_SERVER_WAIT_TIMEOUT_MS 1000


///
/// Wird aus dem Signal-Handler gesetzt und in der Schleife gelesen; deshalb
/// volatile sig_atomic_t und nicht bool.
///
static volatile sig_atomic_t terminate = 0;


static void handle_signal(int signum)
{
    (void)signum;
    terminate = 1;
}

///
/// Schreibt ein gelesenes Kommando ins Log, eine Zeile, mit den Feldern seiner
/// Art. Die Namen stehen hier und nicht im Codec: dort gibt es Wire-Nummern, weil
/// die auf die Strecke gehen -- ein Name zum Lesen ist etwas anderes und wird
/// ausser hier von niemandem gebraucht.
///
static void log_command(const RAD_Command_t *command)
{
    // Sequenznummer und Absender zusammen: die Nummer allein benennt kein
    // Kommando, sie zaehlt je Benutzer (siehe command.h).
    printf("<- #%" PRIu64 " von 0x%" PRIx64 " ",
           command->header.sequence,
           command->header.user);

    switch(command->header.type)
    {
        case RAD_COMMAND_TYPE_SPAWN_ENTITY:
            printf("spawn_entity  typ=%d auf (%d,%d) z=%d\n",
                   (int)command->command.spawn_entity.entity_type,
                   command->command.spawn_entity.x,
                   command->command.spawn_entity.y,
                   command->command.spawn_entity.z);
            break;

        case RAD_COMMAND_TYPE_MOVE_ENTITY:
            printf("move_entity   id=%d von (%d,%d) nach (%d,%d)\n",
                   command->command.move_entity.entity,
                   command->command.move_entity.from_x,
                   command->command.move_entity.from_y,
                   command->command.move_entity.to_x,
                   command->command.move_entity.to_y);
            break;

        case RAD_COMMAND_TYPE_REMOVE_ENTITY:
            printf("remove_entity id=%d\n", command->command.remove_entity.entity);
            break;

        case RAD_COMMAND_TYPE_CREATE_TILE:
            printf("create_tile   typ=%d auf (%d,%d) z=%d\n",
                   (int)command->command.create_tile.tile_type,
                   command->command.create_tile.x,
                   command->command.create_tile.y,
                   command->command.create_tile.z);
            break;

        case RAD_COMMAND_TYPE_REMOVE_TILE:
            printf("remove_tile   auf (%d,%d)\n",
                   command->command.remove_tile.x,
                   command->command.remove_tile.y);
            break;

        // Ohne Nutzlast: was zu sagen war, steht schon im Kopf.
        case RAD_COMMAND_TYPE_END_TURN:
            printf("end_turn\n");
            break;

        case RAD_COMMAND_TYPE_SHOOT:
            printf("shoot         id=%d auf (%d,%d) mit Waffe %u\n",
                   command->command.shoot.entity,
                   command->command.shoot.x,
                   command->command.shoot.y,
                   (unsigned)command->command.shoot.weapon);
            break;

        case RAD_COMMAND_TYPE_USE:
            printf("use           id=%d auf (%d,%d)\n",
                   command->command.use.entity,
                   command->command.use.x,
                   command->command.use.y);
            break;

        // Unerreichbar: ein Kommando ohne Art kommt aus dem Codec nicht heraus.
        case RAD_COMMAND_TYPE_NONE:
        default:
            printf("ohne Art\n");
            break;
    }
}

///
/// Nimmt eine Nachricht vom Client entgegen: lesen, loggen, ausfuehren lassen,
/// antworten.
///
/// "data" ist die reine Nutzlast -- das 8-Byte-Codefeld, das der Client vor jedes
/// Paket setzt, wertet zucchini_server selbst aus (Whitelist) und schneidet es ab,
/// bevor es in den Ringpuffer geht.
///
/// Auf eine Nachricht, die kein Kommando ist, geht nichts zurueck. Eine Antwort
/// traegt Art und Sequenznummer ihres Kommandos -- beides gibt es nicht, wenn sich
/// die Nachricht nicht lesen liess, und eine Antwort mit erfundenem Kopf waere
/// schlimmer als keine: der Absender wuerde sie einem fremden Kommando zuordnen.
/// Sie wird deshalb nur geloggt und fallen gelassen. Was ein Absender stattdessen
/// erfahren sollte, ist eine Frage des Protokolls und noch offen (siehe
/// response.h).
///
/// Was dazwischen liegt, macht diese Datei nicht selbst: aus den Bytes wird ein
/// Kommando in interface/, entschieden und beantwortet wird es in control/. Hier
/// bleibt die Verkettung der Schritte und das Log -- die Ausgabe ist Sache des
/// Programms, nicht der Module.
///
static void handle_message(RAD_Control_t control, ZUC_Api_t api, const uint8_t *data, uint16_t size)
{
    RAD_Command_t command;
    const RAD_CommandCodecResult_t result = RAD_ParseCommandFromMessage(data, size, &command);

    if(result != RAD_COMMAND_CODEC_OK)
    {
        printf("<- %u Bytes verworfen: %s\n", size, RAD_CommandCodecResultText(result));
        return;
    }

    log_command(&command);

    // Wer sendet, spielt mit -- und das entscheidet sich hier, nicht in control/:
    // einen Beitritt gibt es im Protokoll nicht. Es gibt kein Kommando dafuer, und
    // eine getrennte Verbindung meldet Zucchini dem Server auch nicht --
    // ZUC_ApiReceive bringt Bytes und sonst nichts. Solange das so bleibt, ist das
    // erste Kommando eines Benutzers sein Beitritt, und gehen tut niemand mehr;
    // RAD_ControlRemoveUser hat keinen Aufrufer. Sobald das Protokoll ein Join und
    // ein Leave kennt, stehen die beiden Aufrufe an dessen Stelle -- an dieser
    // hier, denn hier kommen die Nachrichten an.
    const RAD_ControlResult_t joined = RAD_ControlAddUser(control, command.header.user);
    if(joined != RAD_CONTROL_OK)
    {
        printf("   nicht aufgenommen: %s\n", RAD_ControlResultText(joined));
    }

    // Die Antwort kommt fertig aus control/ zurueck, "value" eingeschlossen: was
    // der Server ueber das Kommando zu sagen hat, weiss nur die Stelle, die es
    // geprueft und ausgefuehrt hat.
    const RAD_CommandResponse_t response = RAD_ControlExecuteCommand(control, &command);

    printf("   %s (%d Mitspieler)\n",
           RAD_ControlResultText((RAD_ControlResult_t)response.value),
           RAD_ControlNumberOfPlayers(control));

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;

    if(!RAD_SerializeCommandResponseToMessage(&response, message, (uint16_t)sizeof(message), &message_size))
    {
        printf("Antwort passt nicht in %u Bytes\n", (unsigned)sizeof(message));
        return;
    }

    if(0 != ZUC_ApiSend(api, message, message_size))
    {
        printf("Antwort nicht abgeschickt -- Ringpuffer voll?\n");
        return;
    }

    printf("-> #%" PRIu64 " beantwortet, %u Bytes\n", response.header.sequence, message_size);
}

int main(int argc, char **argv)
{
    // Zeilenweise, damit die Ausgabe auch in einer Pipe oder im Container
    // mitlaeuft statt blockweise anzukommen.
    setvbuf(stdout, NULL, _IOLBF, 0);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    const char *interface_name = (argc > 1) ? argv[1] : RAD_SERVER_DEFAULT_INTERFACE_NAME;

    // Zweites Argument: der Spielstand, der geladen werden soll. Ohne ihn faengt
    // der Server mit einem leeren Spiel an.
    const char *save_path = (argc > 2) ? argv[2] : NULL;

    // Woher das Spiel kommt, entscheidet main nicht: es holt es aus dem Loader
    // und reicht es an die Steuerung weiter. Warum keines zustande kam, steht
    // dann schon im Log -- nur der Loader kennt den Grund.
    RAD_Game_t *game = RAD_ControlCreateGame(save_path);
    if(game == NULL)
    {
        printf("Kein Spiel -- Abbruch.\n");
        return 1;
    }

    printf("Spiel geladen: %dx%d, %d Entitaeten, konsistent: %s\n",
           RAD_WORLD_WIDTH, RAD_WORLD_HEIGHT, game->world.number_of_entities,
           RAD_WorldIsConsistent(&game->world) ? "ja" : "nein");

    // Die Steuerung bekommt das Spiel geliehen und wird deshalb vor ihm abgebaut.
    // Wer mitspielt, steht im Spiel selbst; geaendert wird es aber nur ueber die
    // Steuerung -- RAD_ControlAddUser und RAD_ControlBindUserEntity.
    RAD_Control_t control = RAD_CreateControl(game);
    if(control == NULL)
    {
        printf("Steuerung nicht angelegt -- kein Speicher.\n");
        RAD_ControlDestroyGame(&game);
        return 1;
    }

    ZUC_Api_t api = ZUC_CreateApi(interface_name);
    if(api == NULL)
    {
        printf("Zucchini-Api '%s' nicht angelegt.\n", interface_name);
        RAD_DestroyControl(&control);
        RAD_ControlDestroyGame(&game);
        return 1;
    }

    printf("An Zucchini-Instanz '%s' angebunden. Beenden mit SIGINT/SIGTERM.\n", interface_name);

    while(!terminate)
    {
        uint8_t message[RAD_SERVER_MESSAGE_SIZE];
        uint16_t size = 0;

        // Erst den Ringpuffer leerraeumen, dann warten: ein Wakeup steht fuer
        // "es liegt etwas an", nicht fuer "genau eine Nachricht".
        while(0 == ZUC_ApiReceive(api, message, &size))
        {
            handle_message(control, api, message, size);
        }

        ZUC_ApiWait(api, RAD_SERVER_WAIT_TIMEOUT_MS);
    }

    printf("\nEnde.\n");

    ZUC_DestroyApi(&api);
    RAD_DestroyControl(&control);
    RAD_ControlDestroyGame(&game);

    return 0;
}
