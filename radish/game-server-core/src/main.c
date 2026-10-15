#include <radish/game/game.h>

#include <radish/server/interface/message.h>
#include <radish/server/interface/game_report.h>
#include <radish/server/control/execute.h>
#include <radish/server/control/loader.h>
#include <radish/server/control/message_timing.h>
#include <radish/game/control/start_game.h>
#include <radish/server/control/events/tiles.h>

#include <zucchini/api/api.h>
#include <zucchini/ipc/message.h>

#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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
/// Einen Default gibt es nicht, auch zucchini_server verlangt den Namen
/// (-n). Im Container reicht game-instance-entrypoint.sh beiden denselben
/// durch.
///
///     server <zucchini-name>
///
/// Die Welt, mit der das Spiel beginnt, kommt aus der Umgebung:
///
///     RADISH_WORLD_PATH=assets/worlds/default.json server
///
/// Eine Weltdefinition (game/schema/world.schema.json). Ohne sie ist die Welt ein
/// Raster aus Grund in der groessten Form; mit einer, die sich nicht lesen laesst,
/// faengt der Server gar nicht erst an.
///
/// Wer spielt, legt das Backend beim Start eines Spiels als Datei ab, und der
/// Server erfaehrt davon per Signal:
///
///     RADISH_GAME_START_PATH=/run/radish/game-instance-1/spielstart.json
///     RADISH_GAME_PID_PATH=/run/radish/game-instance-1/radish_server.pid
///
/// Das Django des Game-Servers (radish/game-server/instances/views.py) schreibt
/// die Spielstart-Datei (game/schema/spielstart.schema.json) bzw. loescht sie beim
/// Abbruch und schickt danach SIGUSR1 an die PID aus der PID-Datei. Die schreibt
/// dieser Server selbst, und zwar erst, nachdem der Handler fuer SIGUSR1 steht:
/// ohne ihn beendet das Signal den Prozess, und eine PID, die vorher schon in der
/// Datei stuende, liesse genau das zu. Beim Beenden loescht er sie wieder.
///
/// Auf das Signal liest er die Datei neu (game_start_changed): ist sie da, wird
/// sie geladen, fehlt sie, ist kein Spiel mehr angesetzt. Eine Rueckmeldung an
/// Django gibt es nicht -- was ankam, steht nur im Log. Fehlen die beiden
/// Variablen (meist auf dem Host), gibt es weder PID-Datei noch Spielstart.
///
/// Das Ende eines Spiels meldet der Server dem Backend ueber ein Programm, das
/// er dafuer startet (interface/game_report.h):
///
///     RADISH_GAME_FINISHED_COMMAND=/usr/local/bin/register_instance.py
///
/// Fehlt die Variable, wird nicht gemeldet.
///
/// Die Umgebungsvariable mit dem Pfad der Weltdefinition (siehe main).
#define RAD_SERVER_WORLD_PATH_VARIABLE "RADISH_WORLD_PATH"

/// Die Umgebungsvariablen mit den Pfaden der Spielstart- und der PID-Datei (oben).
#define RAD_SERVER_GAME_START_PATH_VARIABLE "RADISH_GAME_START_PATH"
#define RAD_SERVER_PID_PATH_VARIABLE "RADISH_GAME_PID_PATH"

///
/// Das Programm, das das Ende eines Spiels an das Backend meldet
/// (interface/game_report.h) -- im Container docker/game-server/
/// register_instance.py. Fehlt sie (meist auf dem Host), wird nicht gemeldet.
///
#define RAD_SERVER_GAME_FINISHED_COMMAND_VARIABLE "RADISH_GAME_FINISHED_COMMAND"

///
/// Obergrenze einer Nachricht: ein Ringpuffer-Slot abzueglich des Kopfes, den
/// zucchini selbst davorsetzt (Ziel bzw. Absender, zucchini/ipc/message.h).
/// Kommt aus zucchini, statt hier als Zahl zu stehen -- laenger kann ohnehin
/// nichts ankommen und nichts hinausgehen.
///
#define RAD_SERVER_MESSAGE_SIZE ZUC_MESSAGE_MAX_PAYLOAD

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

///
/// Gesetzt von SIGUSR1: die Spielstart-Datei hat sich geaendert. Die Schleife setzt
/// es zurueck, bevor sie liest -- ein Signal waehrend des Lesens fuehrt so zu einem
/// weiteren Durchlauf und geht nicht verloren.
///
static volatile sig_atomic_t game_start_changed = 0;


static void log_unit_path(const RAD_Path_t *path);
static void send_game_finished(RAD_Control_t control, ZUC_Api_t api, uint64_t target);
static void report_game_finished(RAD_Control_t control, RAD_UserId_t winner, const char *command);

///
/// Was der Zugwechsel braucht (on_turn_changed): die Api, um zu senden, und die
/// Steuerung, um alle Einheiten neu zu schicken. Zeiger auf die Variablen in
/// main und nicht ihre Werte -- abonniert wird, bevor es beide gibt; solange
/// eine NULL ist, geht das Entsprechende nicht hinaus.
///
/// Dazu fuer das Spielende (on_game_finished) das Programm, das es ans Backend
/// meldet, NULL fuer keins (RAD_SERVER_GAME_FINISHED_COMMAND_VARIABLE).
///
typedef struct
{
    const ZUC_Api_t *api;
    const RAD_Control_t *control;
    const char *game_finished_command;
} RAD_ServerTurnTarget_t;

static void handle_signal(int signum)
{
    (void)signum;
    terminate = 1;
}

static void handle_game_start_signal(int signum)
{
    (void)signum;
    game_start_changed = 1;
}

///
/// Liefert den Wert der Umgebungsvariable, oder NULL, wenn sie fehlt oder leer ist
/// -- leer zaehlt wie nicht gesetzt, wie bei RADISH_WORLD_PATH.
///
static const char* path_from_environment(const char *variable)
{
    const char *path = getenv(variable);
    if((path != NULL) && (path[0] == '\0'))
    {
        return NULL;
    }
    return path;
}

///
/// Schreibt die eigene PID nach "path", damit Django weiss, wohin SIGUSR1 geht.
/// false, wenn das nicht klappt -- dann erreicht den Server kein Spielstart.
///
static bool write_pid_file(const char *path)
{
    FILE *file = fopen(path, "w");
    if(file == NULL)
    {
        return false;
    }
    const bool written = (fprintf(file, "%ld\n", (long)getpid()) > 0);
    const bool closed = (fclose(file) == 0);
    return written && closed;
}

///
/// Liest die Spielstart-Datei nach dem Signal und richtet das Spiel danach ein:
/// beide Spieler spielen mit, ihre Armeen stehen in ihrer Reserve. Beides macht
/// das Spielmodul (RAD_StartGameFromFile, radish/game/control/start_game.h); hier
/// steht nur, was ins Log geht. Wer danach dran ist, meldet das Spiel selbst
/// (on_turn_changed).
///
/// Vorher setzt die Hauptschleife das Spiel zurueck (RAD_ControlResetGame):
/// ein neuer Start trifft immer auf ein leeres Spiel, gleich was vorher war --
/// ein beendetes, ein abgebrochenes oder eines, dem nach dem Ende noch jemand
/// mit einem Kommando beigetreten ist. Das Ende eines Spiels und ein Abbruch im
/// Backend (die Datei ist weg) setzen dagegen nichts zurueck: das alte Spiel
/// bleibt mit Sieger und Punkten stehen, bis das naechste kommt.
///
/// Ist die Datei ungueltig, bleibt es beim leeren Spiel -- das Backend wollte
/// ein neues, das alte gilt nicht mehr --, und das Log sagt, warum.
///
static void load_game_start(RAD_Control_t control, const char *path)
{
    RAD_GameResult_t game_result = RAD_GAME_OK;
    const RAD_GameStartResult_t result = RAD_ControlStartGameFromFile(control, path, &game_result);
    if(result == RAD_GAME_START_ERROR_REJECTED)
    {
        printf("Spielstart: nicht eingerichtet -- %s.\n", RAD_GameResultText(game_result));
        return;
    }
    if(result != RAD_GAME_START_OK)
    {
        printf("Spielstart: %s nicht geladen -- %s.\n", path, RAD_GameStartResultText(result));
        return;
    }

    // Was daraus wurde, aus dem Spiel gelesen und nicht aus der Datei: so steht im
    // Log, was wirklich eingerichtet ist.
    printf("Spielstart eingerichtet:\n");
    for(int32_t i=0;i < RAD_ControlNumberOfPlayers(control); ++i)
    {
        const RAD_UserId_t user = RAD_ControlPlayerAt(control, i);
        char identifier[RAD_GAME_START_IDENTIFIER_LENGTH + 1];
        RAD_IdentifierFromUserId(user, identifier);
        printf("  %s (Id 0x%" PRIx64 "): %" PRId32 " Einheiten in der Reserve\n",
               identifier,
               user,
               RAD_ControlNumberOfUserUnits(control, user));
    }
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
        case RAD_COMMAND_TYPE_DEPLOY_UNIT:
            printf("deploy_unit   id=%d auf (%d,%d)\n",
                   command->command.deploy_unit.unit,
                   command->command.deploy_unit.x,
                   command->command.deploy_unit.y);
            break;

        // Der Weg und nicht sein Ziel: wo die Figur aufsetzt, steht erst am Ende --
        // was dazwischen liegt, entscheidet, ob der Zug ueberhaupt so geht.
        case RAD_COMMAND_TYPE_MOVE_UNIT:
            printf("move_entity   id=%d ",
                   command->command.move_unit.unit);
            log_unit_path(&command->command.move_unit.path);
            break;

        case RAD_COMMAND_TYPE_REMOVE_UNIT:
            printf("remove_entity id=%d\n", command->command.remove_unit.unit);
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

        case RAD_COMMAND_TYPE_ATTACK:
            printf("attack        id=%d auf (%d,%d)\n",
                   command->command.attack.unit,
                   command->command.attack.x,
                   command->command.attack.y);
            break;

        case RAD_COMMAND_TYPE_USE:
            printf("use           id=%d auf (%d,%d)\n",
                   command->command.use.unit,
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
/// stehen genullt (move_unit.h); sie mitzuloggen hiesse, sechzehnmal (0,0) in die
/// Zeile zu schreiben, wo drei Schritte gemeint sind.
///
/// Ein Pfad mit weniger als zwei Feldern kommt aus dem Codec nicht heraus. Er wird
/// hier trotzdem abgefangen, weil diese Zeile sonst "von" ohne ein Feld dahinter
/// schriebe -- und ein Log soll auch dann lesbar bleiben, wenn die Annahme faellt.
///
static void log_unit_path(const RAD_Path_t *path)
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
/// ZUC_ApiSendTo unterscheidet beides nicht, deshalb wird die Groesse vorher
/// geprueft.
///
/// "target" ist der Zucchini-Code des Empfaengers, ZUC_MESSAGE_BROADCAST fuer
/// jeden Client. **An einen geht, was nur der Anfragende braucht** -- die Antwort
/// auf sein Kommando oder seine Discover-Anfrage; der Code ist der, den
/// ZUC_ApiReceive als Absender geliefert hat. **An alle geht, was sich am Spiel
/// geaendert hat** -- die Ereignisse, der Zugwechsel, das Ende. Ob ein Code noch
/// erreichbar ist, weiss erst zucchini_server; eine Nachricht an einen
/// unbekannten verwirft es mit einer Zeile im eigenen Log, hier gilt sie als
/// abgeschickt.
///
static bool send_message(ZUC_Api_t api, uint64_t target, const uint8_t *message, uint16_t message_size)
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
        if(0 == ZUC_ApiSendTo(api, target, message, message_size))
        {
            return true;
        }
        nanosleep(&pause, NULL);
    }

    return false;
}

static void send_packed(ZUC_Api_t api, uint64_t target, RAD_NetCodecResult_t result,
                        const uint8_t *message, uint16_t message_size, const char *label)
{
    if(result != RAD_NET_CODEC_OK)
    {
        printf("%s nicht gepackt: %s\n", label, RAD_NetCodecResultText(result));
        return;
    }

    if(!send_message(api, target, message, message_size))
    {
        printf("%s nicht abgeschickt -- Ringpuffer voll?\n", label);
        return;
    }

    // Nur ob an alle oder an einen, nicht an wen: der Code ist das Geheimnis, mit
    // dem das Backend Nachrichten durch zucchini_server bringt (message.h), und
    // gehoert nicht ins Log.
    printf("-> %s%s, %u Bytes\n", label, (target == ZUC_MESSAGE_BROADCAST) ? "" : " (an Anfragenden)", message_size);
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
static void send_discover_tiles(RAD_Control_t control, ZUC_Api_t api, uint64_t target, const RAD_DiscoverRequest_t *request)
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

            if(!send_message(api, target, message, message_size))
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
/// Wer gerade dran ist (NetCurrentPlayerEvent), an den Anfragenden -- als Teil
/// der Antwort auf eine Discover-Anfrage. Ein Zugwechsel geht ueber
/// on_turn_changed an alle hinaus.
///
static void send_current_player(RAD_Control_t control, ZUC_Api_t api, uint64_t target)
{
    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;

    const RAD_UserId_t current = RAD_ControlCurrentUser(control);
    const RAD_NetCodecResult_t result =
        RAD_SerializeCurrentPlayerEventToMessage(current, message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, target, result, message, message_size, "current_player");
}

///
/// Die laufende Runde und nach wie vielen gewertet wird -- mit jedem Zugwechsel
/// an alle und auf eine Discover-Anfrage an den Anfragenden.
///
static void send_round(RAD_Control_t control, ZUC_Api_t api, uint64_t target)
{
    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;

    const RAD_NetCodecResult_t result =
        RAD_SerializeRoundEventToMessage((uint32_t)RAD_ControlRound(control),
                                         (uint32_t)RAD_ControlNumberOfRounds(control),
                                         message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, target, result, message, message_size, "round");
}

///
/// Die Antwort auf eine Discover-Anfrage, je eine Nachricht: wer dran ist, die
/// Runde, wie das Spiel ausgegangen ist (nur, wenn es vorbei ist), wer
/// mitspielt, wie gross die Welt ist, wo jeder Mitspieler aufstellen darf -- und
/// danach die Felder des angefragten
/// Ausschnitts (send_discover_tiles). Die Groesse kommt vor den Feldern, damit ein
/// Client sie einordnen kann.
///
/// Sie gehen nur an den, der gefragt hat ("target"): es sind Zustaende, keine
/// Aenderungen, und jeder Client fragt sie selbst an, sobald er verbunden ist.
/// Bei einer ganzen Welt ist das eine Nachricht je Feld -- an alle verschickt,
/// liefe jeder Beitritt einmal durch jeden Client.
///
static void send_discover_events(RAD_Control_t control, ZUC_Api_t api, uint64_t target, const RAD_DiscoverRequest_t *request)
{
    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    RAD_NetCodecResult_t result;

    send_current_player(control, api, target);
    send_round(control, api, target);

    // Ist es schon vorbei, steht das gleich hinter der Runde -- dieselbe
    // Nachricht wie beim Ende selbst (on_game_finished).
    if(RAD_ControlIsFinished(control))
    {
        send_game_finished(control, api, target);
    }

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
    send_packed(api, target, result, message, message_size, "players");

    result = RAD_SerializeWorldSizeEventToMessage((uint32_t)RAD_ControlWorldWidth(control),
                                                  (uint32_t)RAD_ControlWorldHeight(control),
                                                  message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, target, result, message, message_size, "world_size");

    // Nach der Groesse: die Zone an der Ostkante haengt an der Breite.
    for(int32_t i = 0; i < number_of_players; ++i)
    {
        int32_t min_x = 0, min_y = 0, max_x = 0, max_y = 0;
        if(!RAD_ControlDeployZone(control, players[i], &min_x, &min_y, &max_x, &max_y))
        {
            continue;
        }
        result = RAD_SerializeDeployZoneEventToMessage(players[i],
                                                       (uint32_t)min_x, (uint32_t)min_y,
                                                       (uint32_t)max_x, (uint32_t)max_y,
                                                       message, (uint16_t)sizeof(message), &message_size);
        send_packed(api, target, result, message, message_size, "deploy_zone");
    }

    send_discover_tiles(control, api, target, request);
}

///
/// Die Antwort auf eine Reserve-Anfrage, an den Anfragenden: jede Einheit, die
/// noch in der Reserve steht, als eigene Nachricht, in der Reihenfolge ihrer Ids
/// -- beide Spieler, der Client zeigt auch die fremde Reserve
/// (protobuf/discover.proto). Steht keine in der Reserve, geht nichts hinaus.
///
static void send_reserve_units(RAD_Control_t control, ZUC_Api_t api, uint64_t target)
{
    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    int32_t sent = 0;

    for(int32_t i = 0; i < RAD_ControlNumberOfUnits(control); ++i)
    {
        RAD_Unit_t unit;
        if(!RAD_ControlUnitAt(control, i, &unit) || (unit.state != RAD_UNIT_STATE_RESERVE))
        {
            continue;
        }

        uint16_t message_size = 0;
        const RAD_NetCodecResult_t result =
            RAD_SerializeReserveUnitEventToMessage(&unit, message, (uint16_t)sizeof(message), &message_size);
        send_packed(api, target, result, message, message_size, "reserve_unit");
        sent++;
    }

    printf("-> Reserve: %" PRId32 " Einheiten\n", sent);
}

///
/// Jede Einheit, die einem Spieler gehoert, als eigene Nachricht, in der
/// Reihenfolge ihrer Ids -- gleich ob sie in der Reserve oder auf dem Feld steht
/// (protobuf/discover.proto). Gehoert keine einem Spieler, geht nichts hinaus.
/// Als Antwort auf eine Einheiten-Anfrage an den Anfragenden, nach einem
/// Zugwechsel an alle (on_turn_changed).
///
static void send_units(RAD_Control_t control, ZUC_Api_t api, uint64_t target)
{
    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    int32_t sent = 0;

    for(int32_t i = 0; i < RAD_ControlNumberOfUnits(control); ++i)
    {
        RAD_Unit_t unit;
        if(!RAD_ControlUnitAt(control, i, &unit) || (unit.owner == RAD_USER_NONE))
        {
            continue;
        }

        uint16_t message_size = 0;
        const RAD_NetCodecResult_t result =
            RAD_SerializeUnitEventToMessage(&unit, message, (uint16_t)sizeof(message), &message_size);
        send_packed(api, target, result, message, message_size, "unit");
        sent++;
    }

    printf("-> Einheiten: %" PRId32 "\n", sent);
}

///
/// Eine einzelne Einheit mit ihrem jetzigen Stand an jeden Client
/// (NetUnitEvent) -- nach einem Zug oder Angriff, damit die Clients sehen, was
/// sie in diesem Zug schon getan hat (NetUnit.moved, attacked). Gibt es sie
/// nicht, geht nichts hinaus.
///
static void send_unit(RAD_Control_t control, ZUC_Api_t api, RAD_UnitId_t id)
{
    for(int32_t i = 0; i < RAD_ControlNumberOfUnits(control); ++i)
    {
        RAD_Unit_t unit;
        if(!RAD_ControlUnitAt(control, i, &unit) || (unit.id != id))
        {
            continue;
        }

        uint8_t message[RAD_SERVER_MESSAGE_SIZE];
        uint16_t message_size = 0;
        const RAD_NetCodecResult_t result =
            RAD_SerializeUnitEventToMessage(&unit, message, (uint16_t)sizeof(message), &message_size);
        send_packed(api, ZUC_MESSAGE_BROADCAST, result, message, message_size, "unit");
        return;
    }
}

///
/// Die Einheit, die auf (x, y) steht, mit ihrem jetzigen Stand an jeden Client
/// (NetUnitEvent) -- nach einem Angriff fuer die getroffene, damit die Clients
/// sehen, was ihr an Mitgliedern und Lebenspunkten bleibt. Steht dort keine
/// (mehr), geht nichts hinaus: eine zerstoerte hat das Spiel schon gemeldet.
///
static void send_unit_at(RAD_Control_t control, ZUC_Api_t api, int32_t x, int32_t y)
{
    for(int32_t i = 0; i < RAD_ControlNumberOfUnits(control); ++i)
    {
        RAD_Unit_t unit;
        if(RAD_ControlUnitAt(control, i, &unit) && (unit.state == RAD_UNIT_STATE_DEPLOYED)
           && (unit.x == x) && (unit.y == y))
        {
            send_unit(control, api, unit.id);
            return;
        }
    }
}

///
/// Die Einheiten-Ereignisse des Spiels (RAD_EventsUnitChangedCallback_t).
/// "user_argument" ist ein Zeiger auf die Zucchini-Api von main -- auf die
/// Variable und nicht ihren Wert, denn abonniert wird schon beim Anlegen des
/// Spiels, und die Api entsteht erst danach. Solange sie NULL ist, geht nichts
/// hinaus; beim Laden der Welt wird ohnehin keine Figur aufgestellt.
///
/// Gemeldet wird synchron aus RAD_ControlExecuteCommand heraus: das Ereignis geht
/// also vor der Antwort auf das Kommando hinaus, und wie alles an jeden Client.
///
static void on_unit_deployed(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    const ZUC_Api_t api = *(const ZUC_Api_t *)user_argument;
    if(api == NULL)
    {
        printf("Einheit %d auf (%d,%d) -- noch keine Verbindung, nicht gemeldet\n",
               (int)unit->id, (int)x, (int)y);
        return;
    }

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    const RAD_NetCodecResult_t result =
        RAD_SerializeUnitDeployedEventToMessage(unit, x, y, message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, ZUC_MESSAGE_BROADCAST, result, message, message_size, "unit_deployed");
}

///
/// Eine Einheit ist zerstoert und vom Feld (RAD_OnUnitDestroyed_t): als
/// NetUnitDestroyedEvent an jeden Client -- "user_argument" wie bei
/// on_unit_deployed. Im Kampf geschieht das mitten im Angriff, also vor dessen
/// Meldung (on_unit_attacked).
///
static void on_unit_destroyed(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    const ZUC_Api_t api = *(const ZUC_Api_t *)user_argument;
    if(api == NULL)
    {
        printf("Einheit %d auf (%d,%d) zerstoert -- noch keine Verbindung, nicht gemeldet\n",
               (int)unit->id, (int)x, (int)y);
        return;
    }

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    const RAD_NetCodecResult_t packed =
        RAD_SerializeUnitDestroyedEventToMessage(unit->id, x, y, message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, ZUC_MESSAGE_BROADCAST, packed, message, message_size, "unit_destroyed");
}

///
/// Ein Zug (RAD_OnUnitMoved_t): der Weg, den die Einheit wirklich gegangen ist,
/// als NetUnitMovedEvent an jeden Client -- "user_argument" wie bei
/// on_unit_deployed. Nur wenn sie wenigstens ein Feld weit kam: ein Zug, der
/// nicht losging, meldet das Spiel mit leerem Weg, und ein abgelehnter ohne
/// Einheit (NULL).
///
/// Gemeldet wird synchron aus RAD_ControlExecuteCommand heraus, also vor dem
/// neuen Stand der Einheit (send_unit) und vor der Antwort auf das Kommando.
///
static void on_unit_moved(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    if((unit == NULL) || (path == NULL) || (path->number_of_steps < 2))
    {
        printf("Zug ohne Bewegung (%d) -- nicht gemeldet\n", (int)result);
        return;
    }

    const ZUC_Api_t api = *(const ZUC_Api_t *)user_argument;
    if(api == NULL)
    {
        printf("Einheit %d gezogen -- noch keine Verbindung, nicht gemeldet\n", (int)unit->id);
        return;
    }

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    const RAD_NetCodecResult_t packed =
        RAD_SerializeUnitMovedEventToMessage(unit->id, path, message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, ZUC_MESSAGE_BROADCAST, packed, message, message_size, "unit_moved");
}

///
/// Ein Angriff (RAD_OnUnitAttacked_t): wer, wohin und was er bewirkt hat, als
/// NetUnitAttackedEvent an jeden Client -- "user_argument" wie bei
/// on_unit_deployed. Das Spiel meldet nur angenommene Angriffe, und erst nach dem
/// Kampf.
///
/// Gemeldet wird synchron aus RAD_ControlExecuteCommand heraus, also vor dem
/// neuen Stand der beiden Einheiten (send_unit, send_unit_at) und vor der
/// Antwort auf das Kommando.
///
static void on_unit_attacked(void *user_argument, const RAD_AttackResult_t *result)
{
    printf("Einheit %d greift (%d,%d) an: %d Schuesse, %d Treffer, %d durch, %d Schaden, %d gefallen%s\n",
           (int)result->attacker, (int)result->x, (int)result->y,
           (int)result->shots, (int)result->hits, (int)result->penetrations,
           (int)result->wounds, (int)result->members_killed,
           result->target_destroyed ? ", Ziel zerstoert" : "");

    const ZUC_Api_t api = *(const ZUC_Api_t *)user_argument;
    if(api == NULL)
    {
        printf("   noch keine Verbindung, nicht gemeldet\n");
        return;
    }

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    const RAD_NetCodecResult_t packed =
        RAD_SerializeUnitAttackedEventToMessage(result, message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, ZUC_MESSAGE_BROADCAST, packed, message, message_size, "unit_attacked");
}

///
/// Der Zugwechsel des Spiels (RAD_OnTurnChanged_t): wer jetzt dran ist, an jeden
/// Client, und danach alle Einheiten neu (send_units) -- mit dem Zug hat das
/// Spiel vergessen, was sie getan haben (NetUnit.deployed, moved, attacked).
/// "user_argument" ist ein RAD_ServerTurnTarget_t; solange die Api NULL ist,
/// geht nichts hinaus, solange die Steuerung NULL ist, keine Einheiten.
///
/// Gemeldet wird synchron aus dem Spiel heraus -- beim Abgeben, beim Beitritt
/// mit dem ersten Kommando und beim Spielstart. Beim Abgeben und Beitreten geht
/// das Ereignis damit vor der Antwort auf das Kommando hinaus.
///
///
/// Das Ende des Spiels (RAD_OnGameFinished_t): erst Sieger, Grund und die Punkte
/// jedes Mitspielers an jeden Client (send_game_finished), dann an das Backend
/// (report_game_finished) -- die Clients sollen das Ergebnis haben, bevor das
/// Backend aufraeumt. Ans Backend auch ohne Verbindung zu den Clients: das Spiel
/// ist vorbei, ob es jemand sieht oder nicht.
///
/// Danach nimmt die Steuerung kein Kommando mehr an
/// (RAD_CONTROL_ERROR_GAME_FINISHED). Zurueckgesetzt wird erst vor dem naechsten
/// Start (RAD_ControlResetGame).
///
static void on_game_finished(void *user_argument, RAD_UserId_t winner, RAD_GameEndReason_t reason)
{
    const RAD_ServerTurnTarget_t *target = (const RAD_ServerTurnTarget_t *)user_argument;
    printf("Spiel vorbei: Sieger 0x%" PRIx64 ", Grund %d\n", winner, (int)reason);

    const ZUC_Api_t api = *target->api;
    const RAD_Control_t control = *target->control;
    if(control == NULL)
    {
        printf("   keine Steuerung, nicht gemeldet\n");
        return;
    }

    if(api != NULL)
    {
        send_game_finished(control, api, ZUC_MESSAGE_BROADCAST);
    }
    else
    {
        printf("   noch keine Verbindung, nicht an die Clients gemeldet\n");
    }

    report_game_finished(control, winner, target->game_finished_command);
}

///
/// Das Spielende an das Backend (interface/game_report.h): wer gewonnen und
/// wer verloren hat, ueber ihre Kennungen, oder "draw". Das Programm laeuft
/// neben dem Server weiter; was daraus wurde, holt die Hauptschleife ab
/// (RAD_ReportCollectFinished).
///
static void report_game_finished(RAD_Control_t control, RAD_UserId_t winner, const char *command)
{
    if(command == NULL)
    {
        printf("   %s nicht gesetzt -- Spielende nicht ans Backend gemeldet\n",
               RAD_SERVER_GAME_FINISHED_COMMAND_VARIABLE);
        return;
    }

    const int32_t number_of_players = RAD_ControlNumberOfPlayers(control);
    RAD_UserId_t players[(number_of_players > 0) ? number_of_players : 1];
    for(int32_t i = 0; i < number_of_players; ++i)
    {
        players[i] = RAD_ControlPlayerAt(control, i);
    }

    RAD_GameReport_t report;
    if(!RAD_GameReportFromResult(winner, players, number_of_players, &report))
    {
        printf("   Spielende nicht ans Backend gemeldet: keine zwei Mitspieler mit Kennung\n");
        return;
    }
    RAD_ReportGameFinished(command, &report);
}

///
/// Wie das Spiel ausgegangen ist (NetGameFinishedEvent): Sieger, Grund und die
/// Punkte jedes Mitspielers -- an jeden Client in dem Moment, in dem es endet
/// (on_game_finished), und danach an den Anfragenden mit jeder Antwort auf eine
/// Discover-Anfrage (send_discover_events), damit es auch erfaehrt, wer erst
/// spaeter verbindet. Nur fuer ein Spiel, das vorbei ist.
///
static void send_game_finished(RAD_Control_t control, ZUC_Api_t api, uint64_t target)
{
    const RAD_UserId_t winner = RAD_ControlWinner(control);
    const RAD_GameEndReason_t reason = RAD_ControlEndReason(control);

    const int32_t number_of_players = RAD_ControlNumberOfPlayers(control);
    RAD_UserId_t players[(number_of_players > 0) ? number_of_players : 1];
    int32_t scores[(number_of_players > 0) ? number_of_players : 1];
    for(int32_t i = 0; i < number_of_players; ++i)
    {
        players[i] = RAD_ControlPlayerAt(control, i);
        scores[i] = RAD_ControlScore(control, players[i]);
        printf("   0x%" PRIx64 ": %d Punkte\n", players[i], (int)scores[i]);
    }

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    const RAD_NetCodecResult_t result =
        RAD_SerializeGameFinishedEventToMessage(winner, reason, players, scores, (size_t)number_of_players,
                                                message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, target, result, message, message_size, "finished");
}

static void on_turn_changed(void *user_argument, RAD_UserId_t current_user)
{
    const RAD_ServerTurnTarget_t *target = (const RAD_ServerTurnTarget_t *)user_argument;
    const ZUC_Api_t api = *target->api;
    if(api == NULL)
    {
        printf("Zugwechsel auf 0x%" PRIx64 " -- noch keine Verbindung, nicht gemeldet\n", current_user);
        return;
    }

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;
    const RAD_NetCodecResult_t result =
        RAD_SerializeCurrentPlayerEventToMessage(current_user, message, (uint16_t)sizeof(message), &message_size);
    send_packed(api, ZUC_MESSAGE_BROADCAST, result, message, message_size, "current_player");

    const RAD_Control_t control = *target->control;
    if(control != NULL)
    {
        send_round(control, api, ZUC_MESSAGE_BROADCAST);
        send_units(control, api, ZUC_MESSAGE_BROADCAST);
    }
}

///
/// "source" ist der Zucchini-Code, von dem die Nachricht kam (ZUC_ApiReceive) --
/// dorthin geht die Antwort, auf eine Discover-Anfrage wie auf ein Kommando. Wer
/// der Absender im Spiel ist, sagt er nicht: das steht in den ersten 8 Byte der
/// Nachricht (RAD_ParseSenderFromMessage).
///
static void handle_message(RAD_Control_t control, ZUC_Api_t api, uint64_t source, const uint8_t *raw, uint16_t raw_size)
{
    // Zuerst der Absender: die ersten 8 Byte setzt das Backend, nicht der Client
    // (RAD_ParseSenderFromMessage). Was danach kommt, ist die NetUserRequest.
    RAD_UserId_t sender = RAD_USER_NONE;
    const uint8_t *data = NULL;
    uint16_t size = 0;
    const RAD_NetCodecResult_t sender_result = RAD_ParseSenderFromMessage(raw, raw_size, &sender, &data, &size);
    if(sender_result != RAD_NET_CODEC_OK)
    {
        printf("<- %u Bytes verworfen: %s\n", raw_size, RAD_NetCodecResultText(sender_result));
        return;
    }

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
        send_discover_events(control, api, source, &discover);
        return;
    }

    // Dann als Reserve-Anfrage, nach demselben Muster: ist sie es nicht, geht die
    // Nachricht weiter zum Kommando.
    if(RAD_ParseDiscoverReserveFromMessage(data, size) == RAD_NET_CODEC_OK)
    {
        printf("<- discover_reserve\n");
        send_reserve_units(control, api, source);
        return;
    }

    // Dann als Einheiten-Anfrage, nach demselben Muster.
    if(RAD_ParseDiscoverUnitsFromMessage(data, size) == RAD_NET_CODEC_OK)
    {
        printf("<- discover_units\n");
        send_units(control, api, source);
        return;
    }

    RAD_Command_t command;
    const RAD_NetCodecResult_t parse_result = RAD_ParseCommandFromMessage(data, size, &command);

    if(parse_result != RAD_NET_CODEC_OK)
    {
        printf("<- %u Bytes verworfen: %s\n", size, RAD_NetCodecResultText(parse_result));
        return;
    }

    // Der Absender ist der aus dem Kopf der Nachricht, gleich was im Kommando steht:
    // ein user_id dort hat der Client geschrieben (message.h).
    command.header.user = sender;

    log_command(&command);

    // Wer sendet, spielt mit -- und das entscheidet sich hier, nicht in control/:
    // einen Beitritt gibt es im Protokoll nicht. Es gibt kein Kommando dafuer, und
    // eine getrennte Verbindung meldet Zucchini dem Server auch nicht --
    // ZUC_ApiReceive bringt die Bytes und den Code des Absenders, aber kein
    // Kommen und Gehen. Solange das so bleibt, ist das
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

    // Hat eine Einheit gezogen oder angegriffen, geht ihr neuer Stand vor der
    // Antwort hinaus, wie beim Aufstellen (on_unit_deployed): die Clients sehen
    // daran, was sie in diesem Zug noch darf. Nach einem Angriff dazu die
    // getroffene, mit dem, was ihr geblieben ist.
    if(success && (command.header.type == RAD_COMMAND_TYPE_MOVE_UNIT))
    {
        send_unit(control, api, command.command.move_unit.unit);
    }
    else if(success && (command.header.type == RAD_COMMAND_TYPE_ATTACK))
    {
        send_unit(control, api, command.command.attack.unit);
        send_unit_at(control, api, command.command.attack.x, command.command.attack.y);
    }

    uint8_t message[RAD_SERVER_MESSAGE_SIZE];
    uint16_t message_size = 0;

    const RAD_NetCodecResult_t serialize_result = RAD_SerializeCommandResponseToMessage(
        &response, success, description, message, (uint16_t)sizeof(message), &message_size);

    if(serialize_result != RAD_NET_CODEC_OK)
    {
        printf("Antwort nicht gepackt: %s\n", RAD_NetCodecResultText(serialize_result));
        return;
    }

    // Nur an den Absender: die Antwort traegt seine Sequenznummer, ein anderer
    // Client koennte mit ihr nichts anfangen. Was sich am Spiel geaendert hat,
    // ist vorher schon als Ereignis an alle gegangen.
    if(!send_message(api, source, message, message_size))
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
    signal(SIGUSR1, handle_game_start_signal);

    if((argc != 2) || (argv[1][0] == '\0'))
    {
        printf("Aufruf: %s <zucchini-name>\n", argv[0]);
        return 1;
    }
    const char *interface_name = argv[1];

    // Die Welt, mit der das Spiel beginnt, aus der Umgebung und nicht als zweites
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
    //
    // Die Api steht schon hier, angelegt wird sie erst unten: die Einheiten-
    // Ereignisse bekommen einen Zeiger auf diese Variable (on_unit_deployed).
    // Ebenso die Steuerung fuer den Zugwechsel (on_turn_changed).
    ZUC_Api_t api = NULL;
    RAD_Control_t control = NULL;
    const char *game_finished_command = path_from_environment(RAD_SERVER_GAME_FINISHED_COMMAND_VARIABLE);
    const RAD_ServerTurnTarget_t turn_target = {
        .api = &api,
        .control = &control,
        .game_finished_command = game_finished_command
    };

    RAD_EventCallbacks_t event_callbacks = {
        .tile_changed = {
            .added = RAD_OnTileAddedToGame,
            .removed = RAD_OnTileRemovedFromGame,
            .changed = RAD_OnTileStateChanged
        },
        .unit_changed = {
            .user_argument = &api,
            .spawned = on_unit_deployed,
            .destroyed = on_unit_destroyed,
            .moved = on_unit_moved,
            .attacked = on_unit_attacked
        },
        .turn_changed = {
            .user_argument = (void *)&turn_target,
            .changed = on_turn_changed
        },
        .game_changed = {
            .user_argument = (void *)&turn_target,
            .finished = on_game_finished
        }
    };
    RAD_ControlGame_t loaded = RAD_ControlCreateGame(world_path, & event_callbacks);
    if(loaded.game == NULL)
    {
        printf("Kein Spiel -- Abbruch.\n");
        return 1;
    }

    // Die Steuerung bekommt das Spiel geliehen und wird deshalb vor ihm abgebaut.
    // Wer mitspielt, steht im Spiel selbst; geaendert wird es aber nur ueber die
    // Steuerung -- RAD_ControlAddUser und RAD_ControlBindUserUnit.
    control = RAD_CreateControl(loaded.game);
    if(control == NULL)
    {
        printf("Steuerung nicht angelegt -- kein Speicher.\n");
        RAD_ControlDestroyGame(&loaded);
        return 1;
    }

    api = ZUC_CreateApi(interface_name);
    if(api == NULL)
    {
        printf("Zucchini-Api '%s' nicht angelegt.\n", interface_name);
        RAD_DestroyControl(&control);
        RAD_ControlDestroyGame(&loaded);
        return 1;
    }

    // Die PID-Datei erst jetzt: lange nach dem Handler fuer SIGUSR1 (siehe oben),
    // und erst wenn der Server wirklich laeuft -- bei jedem Abbruch davor bliebe
    // sonst eine PID liegen, hinter der kein Server mehr steht.
    const char *game_start_path = path_from_environment(RAD_SERVER_GAME_START_PATH_VARIABLE);
    const char *pid_path = path_from_environment(RAD_SERVER_PID_PATH_VARIABLE);
    if(pid_path != NULL && !write_pid_file(pid_path))
    {
        printf("PID-Datei %s nicht geschrieben -- Spielstarts erreichen diesen Server nicht.\n", pid_path);
        pid_path = NULL;
    }

    printf("An Zucchini-Instanz '%s' angebunden. Beenden mit SIGINT/SIGTERM.\n", interface_name);

    // Ungleich 0 nur, wenn der Server ohne Spiel dasteht (RAD_ControlResetGame) --
    // supervisord startet ihn dann neu.
    int exit_code = 0;

    // Wie lange eine Nachricht dauert, seit dem Start (control/message_timing.h).
    RAD_MessageTiming_t timing;
    RAD_MessageTimingInitialize(&timing);

    while(!terminate)
    {
        uint8_t message[RAD_SERVER_MESSAGE_SIZE];
        uint16_t size = 0;
        uint64_t source = 0;
        const uint64_t measured_before = timing.count;

        // Erst den Ringpuffer leerraeumen, dann warten: ein Wakeup steht fuer
        // "es liegt etwas an", nicht fuer "genau eine Nachricht".
        while(0 == ZUC_ApiReceive(api, &source, message, &size))
        {
            const uint64_t start = RAD_MessageTimingNow();
            handle_message(control, api, source, message, size);
            RAD_MessageTimingAdd(&timing, RAD_MessageTimingNow() - start);
        }

        // Meldungen des Spielendes, die inzwischen fertig sind, ins Log.
        RAD_ReportCollectFinished();

        // Ein neuer Spielstart: erst zuruecksetzen, dann einrichten. Ohne Datei
        // ist kein Spiel angesetzt (Abbruch im Backend) -- dann bleibt das Spiel,
        // wie es ist, auch ein beendetes (load_game_start).
        if(game_start_changed)
        {
            game_start_changed = 0;
            if(game_start_path != NULL && access(game_start_path, F_OK) == 0)
            {
                if(!RAD_ControlResetGame(&loaded, &control, world_path, &event_callbacks))
                {
                    printf("Ohne Spiel geht es nicht weiter -- Abbruch.\n");
                    exit_code = 1;
                    break;
                }
                load_game_start(control, game_start_path);
            }
            else if(game_start_path != NULL)
            {
                printf("Spielstart: keine Datei unter %s -- kein Spiel angesetzt.\n", game_start_path);
            }
        }

        // Die Werte seit dem Start, aber nur, wenn in diesem Durchlauf eine
        // Nachricht dazukam: sonst stuende ohne Verkehr jede Sekunde dieselbe Zeile
        // im Log.
        if(timing.count != measured_before)
        {
            printf("Bearbeitung seit Start: %" PRIu64 " Nachrichten, max %.3f ms, avg %.3f ms\n",
                   timing.count,
                   (double)timing.max_ns / 1e6,
                   (double)RAD_MessageTimingAverage(&timing) / 1e6);
        }

        // Ein Signal unterbricht das Warten (poll in ZUC_ApiWait) -- ein Spielstart
        // kommt also gleich an und nicht erst nach Ablauf der Wartezeit.
        ZUC_ApiWait(api, RAD_SERVER_WAIT_TIMEOUT_MS);
    }

    printf("\nEnde.\n");

    if(pid_path != NULL)
    {
        unlink(pid_path);
    }

    ZUC_DestroyApi(&api);
    RAD_DestroyControl(&control);
    RAD_ControlDestroyGame(&loaded);

    return exit_code;
}
