#include <radish/game/game.h>

#include <radish/server/interface/message.h>
#include <radish/server/control/execute.h>
#include <radish/server/control/loader.h>
#include <radish/server/control/events/tiles.h>

#include <zucchini/api/api.h>
#include <zucchini/ipc/ring_buffer.h>

#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

///
/// Der Server ist die Gegenseite des Clients: er haelt den Spielzustand und
/// haengt ueber die Zucchini-Api daran, was von draussen hereinkommt.
///
/// Zucchini nimmt die UDP-Pakete an; dieses Programm ist aus dessen Sicht ein
/// lokaler Client, der ueber zwei Ringpuffer im Shared Memory angebunden ist
/// (siehe zucchini/api/api.h). Es gibt also zwei Prozesse: zucchini_server und
/// diesen hier.
///
///     Browser ──► Backend ──UDP──► zucchini_server ──Ringpuffer──► server
///
/// Der Name unten muss zu dem passen, unter dem die Zucchini-Instanz laeuft --
/// aus ihm leiten beide Seiten die Namen der Ringpuffer und der Wakeup-FIFO ab.
/// "zucchini" ist der Default von zucchini_server (ZUC_SERVER_NAME in dessen
/// main.c); ein anderer laesst sich als erstes Argument uebergeben.
///
///     server [zucchini-name] [spielstand.json]
///
/// Das zweite Argument ist ein Spielstand im Speicherformat (siehe
/// radish/game/serialization). Ohne ihn faengt der Server mit einem leeren Spiel
/// an; mit einem, der sich nicht lesen laesst, faengt er gar nicht erst an.
///
/// Die Welt, mit der das Spiel beginnt, kommt aus der Umgebung:
///
///     RADISH_WORLD_PATH=assets/worlds/default.json server
///
/// Eine Weltdefinition (game/schema/world.schema.json), geladen vor einem
/// Spielstand. Ohne sie ist die Welt ein Raster aus Grund in der groessten Form;
/// mit einer, die sich nicht lesen laesst, faengt der Server nicht an -- wie beim
/// Spielstand.
///
#define RAD_SERVER_DEFAULT_INTERFACE_NAME "zucchini"

/// Die Umgebungsvariable mit dem Pfad der Weltdefinition (siehe main).
#define RAD_SERVER_WORLD_PATH_VARIABLE "RADISH_WORLD_PATH"

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
/// Wie lange eine Nachricht auf einen freien Platz im Sendepuffer wartet
/// (send_message): so viele Versuche im Abstand von RAD_SERVER_SEND_RETRY_MS.
///
/// Der Puffer hat ZUC_RING_BUFFER_SLOT_COUNT Plaetze, und zucchini_server leert
/// ihn in seiner Schleife, die spaetestens alle 5 ms durchlaeuft. Wer mehr
/// Nachrichten auf einmal schickt als Plaetze frei sind -- eine Discover-Antwort
/// ist eine Nachricht je Feld --, muss also warten und darf nicht verwerfen. Die
/// Grenze ist grosszuegig bemessen, damit sie nur greift, wenn zucchini_server
/// gar nicht mehr abholt: dann soll der Server nicht haengenbleiben.
///
#define RAD_SERVER_SEND_RETRY_MS 1
#define RAD_SERVER_SEND_RETRY_LIMIT 100


///
/// Wird aus dem Signal-Handler gesetzt und in der Schleife gelesen; deshalb
/// volatile sig_atomic_t und nicht bool.
///
static volatile sig_atomic_t terminate = 0;


static void log_entity_path(const RAD_EntityPath_t *path);

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

        // Der Weg und nicht sein Ziel: wo die Figur aufsetzt, steht erst am Ende --
        // was dazwischen liegt, entscheidet, ob der Zug ueberhaupt so geht.
        case RAD_COMMAND_TYPE_MOVE_ENTITY:
            printf("move_entity   id=%d ",
                   command->command.move_entity.entity);
            log_entity_path(&command->command.move_entity.path);
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
/// Die Felder eines Weges, in ihrer Reihenfolge, und schliesst die Zeile ab.
///
/// Ein eigener Helfer, weil ein Pfad eine Schleife braucht und printf keine hat.
/// Die Felder stehen absolut da, so wie im Kommando (model/path/path.h), und das
/// erste ist das, auf dem die Figur schon steht -- deshalb "von" und dann der Rest.
///
/// **Geloggt werden Schritte, gezaehlt sind Felder.** number_of_steps zaehlt seit
/// dem Startfeld Felder; ein Weg aus n Feldern ist n-1 Schritte weit. Hier steht
/// die Zahl, die jemand nachzaehlen wuerde, wenn er auf die Klammern sieht.
///
/// Nur die belegten Plaetze. Hinter number_of_steps fahren die ungenutzten mit und
/// stehen genullt (move_entity.h); sie mitzuloggen hiesse, sechzehnmal (0,0) in die
/// Zeile zu schreiben, wo drei Schritte gemeint sind.
///
/// Ein Pfad mit weniger als zwei Feldern kommt aus dem Codec nicht heraus. Er wird
/// hier trotzdem abgefangen, weil diese Zeile sonst "von" ohne ein Feld dahinter
/// schriebe -- und ein Log soll auch dann lesbar bleiben, wenn die Annahme faellt.
///
static void log_entity_path(const RAD_EntityPath_t *path)
{
    if(path->number_of_steps < 2)
    {
        printf("ohne Weg (%d Feld%s)\n",
               (int)path->number_of_steps,
               (path->number_of_steps == 1) ? "" : "er");
        return;
    }

    const int32_t steps = (int32_t)path->number_of_steps - 1;

    printf("von (%d,%d) in %d Schritt%s nach",
           (int)path->steps_to[0].x, (int)path->steps_to[0].y,
           steps,
           (steps == 1) ? "" : "en");

    for(int32_t i = 1; i < (int32_t)path->number_of_steps; ++i)
    {
        printf(" (%d,%d)", (int)path->steps_to[i].x, (int)path->steps_to[i].y);
    }

    printf("\n");
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
///
/// Schickt eine fertig gepackte Nachricht ab und loggt, was es war. "label" ist
/// nur fuers Log. Geht beim Packen etwas schief, steht der Grund im Log, und es
/// geht nichts hinaus.
///
///
/// Legt eine Nachricht in den Sendepuffer und wartet, solange er voll ist
/// (RAD_SERVER_SEND_RETRY_LIMIT). false erst, wenn er auch danach voll ist --
/// oder die Nachricht fuer einen Slot zu gross ist, was kein Warten behebt;
/// ZUC_ApiSend unterscheidet beides nicht, deshalb wird die Groesse vorher
/// geprueft.
///
static bool send_message(ZUC_Api_t api, const uint8_t *message, uint16_t message_size)
{
    if(message_size > RAD_SERVER_MESSAGE_SIZE)
    {
        return false;
    }

    const struct timespec pause = {
        .tv_sec = 0,
        .tv_nsec = RAD_SERVER_SEND_RETRY_MS * 1000L * 1000L
    };

    for(int32_t attempt = 0; attempt <= RAD_SERVER_SEND_RETRY_LIMIT; ++attempt)
    {
        if(0 == ZUC_ApiSend(api, message, message_size))
        {
            return true;
        }
        nanosleep(&pause, NULL);
    }

    return false;
}

static void send_packed(ZUC_Api_t api, RAD_NetCodecResult_t result,
                        const uint8_t *message, uint16_t message_size, const char *label)
{
    if(result != RAD_NET_CODEC_OK)
    {
        printf("%s nicht gepackt: %s\n", label, RAD_NetCodecResultText(result));
        return;
    }

    if(!send_message(api, message, message_size))
    {
        printf("%s nicht abgeschickt -- Ringpuffer voll?\n", label);
        return;
    }

    printf("-> %s, %u Bytes\n", label, message_size);
}

///
/// Die Felder des angefragten Ausschnitts, eine Nachricht je Feld
/// (NetTilesEvent mit einem Eintrag).
///
/// **Eine je Feld, nicht eine je Zeile oder fuer alle.** Wie viele Felder in
/// einen Slot passen, haengt an der Groesse eines Feldes auf der Strecke und an
/// der Breite der Welt -- beides aendert sich. Ein Feld je Nachricht passt, solange
/// ein einzelnes Feld passt, und das Warten bei vollem Puffer uebernimmt
/// send_message.
///
/// **Bleibt der Puffer voll, endet die Antwort.** send_message hat dann schon
/// RAD_SERVER_SEND_RETRY_LIMIT Versuche lang gewartet, und zucchini_server holt
/// offenbar nichts mehr ab. Jedes weitere Feld wartete genauso lange und kaeme
/// genauso wenig an -- bei einer ganzen Welt stuende der Server sekundenlang.
/// Die uebrigen Felder zaehlen als nicht verschickt.
///
/// Der Ausschnitt wird auf die Welt zugeschnitten: was ausserhalb liegt, gibt es
/// nicht und kommt nicht mit, ein Ausschnitt ganz ausserhalb ergibt keine Felder.
/// Gerechnet wird in 64 Bit, weil x + w in uint32 ueberlaufen kann -- ein
/// Ausschnitt ist, was ein Client geschickt hat. Die Felder selbst liest
/// RAD_ControlTileAt, das die Grenzen noch einmal prueft.
///
static void send_discover_tiles(RAD_Control_t control, ZUC_Api_t api, const RAD_DiscoverRequest_t *request)
{
    const uint64_t width = (uint64_t)RAD_ControlWorldWidth(control);
    const uint64_t height = (uint64_t)RAD_ControlWorldHeight(control);

    const uint64_t x_begin = (request->x < width) ? request->x : width;
    const uint64_t y_begin = (request->y < height) ? request->y : height;
    const uint64_t x_end_requested = (uint64_t)request->x + (uint64_t)request->w;
    const uint64_t y_end_requested = (uint64_t)request->y + (uint64_t)request->h;
    const uint64_t x_end = (x_end_requested < width) ? x_end_requested : width;
    const uint64_t y_end = (y_end_requested < height) ? y_end_requested : height;

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    int32_t sent = 0;
    int32_t failed = 0;
    bool stalled = false;

    for(uint64_t y = y_begin; (y < y_end) && !stalled; ++y)
    {
        for(uint64_t x = x_begin; (x < x_end) && !stalled; ++x)
        {
            RAD_Tile_t tile;
            if(!RAD_ControlTileAt(control, (int32_t)x, (int32_t)y, &tile))
            {
                failed++;
                continue;
            }

            const RAD_NetCodecResult_t result =
                RAD_SerializeTilesEventToMessage(&tile, 1, message, (uint16_t)sizeof(message), &message_size);
            if(result != RAD_NET_CODEC_OK)
            {
                printf("Feld (%" PRIu64 ",%" PRIu64 ") nicht gepackt: %s\n", x, y, RAD_NetCodecResultText(result));
                failed++;
                continue;
            }

            if(!send_message(api, message, message_size))
            {
                printf("Feld (%" PRIu64 ",%" PRIu64 ") nicht abgeschickt -- Ringpuffer voll, Rest der Antwort entfaellt\n", x, y);
                stalled = true;
                continue;
            }

            sent++;
        }
    }

    // Was nach dem Abbruch nicht mehr versucht wurde, zaehlt mit -- das Feld, an
    // dem es scheiterte, eingeschlossen.
    if(stalled)
    {
        failed = (int32_t)((x_end - x_begin) * (y_end - y_begin)) - sent;
    }

    // Eine Zeile fuer alle Felder statt einer je Nachricht: bei einer ganzen Welt
    // waeren es sonst so viele Zeilen wie Felder.
    printf("-> tiles, %d Felder (x %" PRIu64 "..%" PRIu64 ", y %" PRIu64 "..%" PRIu64 ")",
           sent, x_begin, x_end, y_begin, y_end);
    if(failed > 0)
    {
        printf(", %d nicht verschickt", failed);
    }
    printf("\n");
}

///
/// Die Antwort auf eine Discover-Anfrage, je eine Nachricht: wer dran ist, wer
/// mitspielt, wie gross die Welt ist -- und danach die Felder des angefragten
/// Ausschnitts (send_discover_tiles). Die Groesse kommt vor den Feldern, damit ein
/// Client sie einordnen kann.
///
/// Sie gehen, wie alles, was der Server schickt, an jeden Client (zucchini
/// verteilt an die ganze Whitelist) und nicht nur an den, der gefragt hat. Das
/// schadet nicht: es sind Zustaende, keine Aenderungen, und jeder bekommt
/// dieselben.
///
static void send_discover_events(RAD_Control_t control, ZUC_Api_t api, const RAD_DiscoverRequest_t *request)
{
    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    RAD_NetCodecResult_t result;

    const RAD_UserId_t current = RAD_ControlCurrentUser(control);
    result = RAD_SerializeCurrentPlayerEventToMessage(current, message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, result, message, message_size, "current_player");

    // Ohne feste Obergrenze von hier aus: wie viele mitspielen koennen, steht
    // privat im Spiel. Mindestens ein Platz, weil ein Array der Laenge null
    // kein gueltiges C ist -- mitgeschickt werden nur "number_of_players".
    const int32_t number_of_players = RAD_ControlNumberOfPlayers(control);
    RAD_UserId_t players[(number_of_players > 0) ? number_of_players : 1];
    for(int32_t i = 0; i < number_of_players; ++i)
    {
        players[i] = RAD_ControlPlayerAt(control, i);
    }
    result = RAD_SerializePlayersEventToMessage(players, (size_t)number_of_players,
                                                message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, result, message, message_size, "players");

    result = RAD_SerializeWorldSizeEventToMessage((uint32_t)RAD_ControlWorldWidth(control),
                                                  (uint32_t)RAD_ControlWorldHeight(control),
                                                  message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, result, message, message_size, "world_size");

    send_discover_tiles(control, api, request);
}

static void handle_message(RAD_Control_t control, ZUC_Api_t api, const uint8_t *data, uint16_t size)
{
    // Erst als Discover-Anfrage lesen, dann als Kommando: eine Nachricht, die
    // keine Discover-Anfrage ist, liefert UNEXPECTED_MESSAGE und geht den
    // bisherigen Weg. Beantwortet wird sie mit drei Spiel-Ereignissen und den
    // Feldern des Ausschnitts (send_discover_events).
    RAD_DiscoverRequest_t discover;
    const RAD_NetCodecResult_t discover_result = RAD_ParseDiscoverFromMessage(data, size, &discover);
    if(discover_result == RAD_NET_CODEC_OK)
    {
        printf("<- discover      (%" PRIu32 ",%" PRIu32 ") %" PRIu32 "x%" PRIu32 "\n",
               discover.x, discover.y, discover.w, discover.h);
        send_discover_events(control, api, &discover);
        return;
    }

    RAD_Command_t command;
    const RAD_NetCodecResult_t parse_result = RAD_ParseCommandFromMessage(data, size, &command);

    if(parse_result != RAD_NET_CODEC_OK)
    {
        printf("<- %u Bytes verworfen: %s\n", size, RAD_NetCodecResultText(parse_result));
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

    // "success"/"description" statt "value": interface/message.h kennt
    // RAD_ControlResult_t nicht (die Schnittstelle deutet kein Kommando), also
    // wird hier gedeutet, nicht dort -- dieselbe Stelle, die "value" bisher
    // schon fuers Log in Text uebersetzt hat.
    const RAD_ControlResult_t control_result = (RAD_ControlResult_t)response.value;
    const bool success = (control_result == RAD_CONTROL_OK);
    const char *description = RAD_ControlResultText(control_result);

    printf("   %s (%d Mitspieler)\n", description, RAD_ControlNumberOfPlayers(control));

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;

    const RAD_NetCodecResult_t serialize_result = RAD_SerializeCommandResponseToMessage(
        &response, success, description, message, (uint16_t)sizeof(message), &message_size);

    if(serialize_result != RAD_NET_CODEC_OK)
    {
        printf("Antwort nicht gepackt: %s\n", RAD_NetCodecResultText(serialize_result));
        return;
    }

    if(!send_message(api, message, message_size))
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

    // Die Welt, mit der das Spiel beginnt, aus der Umgebung und nicht als drittes
    // Argument: sie ist eine Einstellung des Betriebs und keine des einzelnen
    // Starts. Das Image setzt sie auf die Standardwelt, die es mitbringt
    // (docker/game-server/Dockerfile); auf dem Host fehlt sie meist, und dann
    // bleibt es beim Raster aus Grund. Leer zaehlt wie nicht gesetzt.
    const char *world_path = getenv(RAD_SERVER_WORLD_PATH_VARIABLE);
    if((world_path != NULL) && (world_path[0] == '\0'))
    {
        world_path = NULL;
    }

    // Woher das Spiel kommt, entscheidet main nicht: es holt es aus dem Loader
    // und reicht es an die Steuerung weiter. Warum keines zustande kam, steht
    // dann schon im Log -- nur der Loader kennt den Grund.
    //
    // Zwei Zeiger und nicht einer: an einem Spiel haengt sein Event-Manager, und
    // beide gehen zusammen weg. Was das heisst, weiss der Loader
    // (RAD_ControlGame_t) -- main traegt das Paar weiter und gibt es zurueck.
    RAD_EventCallbacks_t event_callbacks = {
        .tile_changed = {
            .added = RAD_OnTileAddedToGame,
            .removed = RAD_OnTileRemovedFromGame,
            .changed = RAD_OnTileStateChanged 
        }
    };
    RAD_ControlGame_t loaded = RAD_ControlCreateGame(world_path, save_path, & event_callbacks);
    if(loaded.game == NULL)
    {
        printf("Kein Spiel -- Abbruch.\n");
        return 1;
    }

    // Die Steuerung bekommt das Spiel geliehen und wird deshalb vor ihm abgebaut.
    // Wer mitspielt, steht im Spiel selbst; geaendert wird es aber nur ueber die
    // Steuerung -- RAD_ControlAddUser und RAD_ControlBindUserEntity.
    RAD_Control_t control = RAD_CreateControl(loaded.game);
    if(control == NULL)
    {
        printf("Steuerung nicht angelegt -- kein Speicher.\n");
        RAD_ControlDestroyGame(&loaded);
        return 1;
    }

    ZUC_Api_t api = ZUC_CreateApi(interface_name);
    if(api == NULL)
    {
        printf("Zucchini-Api '%s' nicht angelegt.\n", interface_name);
        RAD_DestroyControl(&control);
        RAD_ControlDestroyGame(&loaded);
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
    RAD_ControlDestroyGame(&loaded);

    return 0;
}
