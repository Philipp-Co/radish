#include <radish/server/control/loader.h>

#include <radish/game/control/events/event_manager.h>

#include <stdio.h>
#include <stddef.h>

///
/// Zwei Schritte: erst entsteht ein leeres Spiel, dann wird -- wenn ein Pfad
/// dasteht -- ein Spielstand hineingelesen. Umgekehrt geht es nicht, denn
/// RAD_LoadGameFromFile fuellt ein vorhandenes Spiel und uebernimmt daraus alles,
/// was nicht im Spielstand steht (die Ereignisverwaltung, den Absender, die
/// Sequenznummer).
///
/// Der Event-Manager liegt auf dem Heap und nicht in dieser Datei: RAD_Game_t
/// haelt nur einen Zeiger auf ihn, er muss also mindestens so lange leben wie das
/// Spiel. Eine statische Variable hier taete das auch, gaebe es aber nur einmal
/// -- so haengt an jedem Spiel sein eigener, und das Modul hat keinen Zustand.
///
/// Abgebaut wird er in RAD_ControlDestroyGame, und zwar aus dem Paar, das
/// RAD_ControlCreateGame zurueckgegeben hat (RAD_ControlGame_t). Vorher holte sich
/// diese Datei den Zeiger aus game->event_manager -- das ging, solange die Struktur
/// des Spiels offenlag, und geht seit ihrer Kapselung nicht mehr. Der Weg ueber das
/// Paar ist ohnehin der richtige: wer etwas anlegt, weiss, was er angelegt hat, und
/// muss es nicht bei dem nachfragen, dem er es gegeben hat.
///
/// Der Manager wird hier auch nicht mehr selbst mit malloc besorgt.
/// RAD_CreateEventManager legt ihn inzwischen an und gibt einen Zeiger zurueck,
/// RAD_DestroyEventManager nimmt ihn zurueck und gibt ihn frei -- diese Datei
/// stellt keinen Speicher mehr fuer fremde Strukturen, und sie koennte es auch
/// nicht: wie gross ein Event-Manager ist, steht nicht mehr in seinem Header.
///
/// Diese Datei loggt, anders als die uebrigen Module des Servers. Sie ist die
/// einzige, die einen Spielstand anfasst -- ob einer zustande kam, ist eine Frage
/// des Betriebs und gehoert ins Log. Angefasst wird er inzwischen mit einem
/// Funktionsaufruf: die Datei selbst, ihr Format und ihr Inhalt sind Sache des
/// Spielmoduls, und was davon schiefgegangen ist, steht in einem Wert.
///

static RAD_ControlGame_t RAD_ControlCreateEmptyGame(void);


RAD_ControlGame_t RAD_ControlCreateGame(const char *save_path, RAD_EventCallbacks_t *callbacks)
{
    RAD_ControlGame_t created = RAD_ControlCreateEmptyGame();
    if(created.game == NULL)
    {
        printf("Spiel nicht angelegt -- kein Speicher.\n");
        return created;
    }

    if(save_path == NULL)
    {
        return created;
    }

    RAD_EventManagerSubscribeToTileEvents(created.event_manager, callbacks->tile_changed);

    // Ein Ergebnis fuer Datei und Inhalt: ob die Datei fehlte oder ihr Inhalt
    // abgelehnt wurde, steht in demselben Wert, und fuer den Betrieb ist es
    // dieselbe Nachricht -- geladen wurde nichts, und warum, sagt der Text.
    const RAD_GameSaveResult_t result = RAD_LoadGameFromFile(created.game, save_path);
    if(result != RAD_GAME_SAVE_OK)
    {
        printf("Spielstand '%s' nicht geladen: %s\n", save_path, RAD_GameSaveResultText(result));

        // Baut beides ab und setzt beide Zeiger auf NULL -- damit ist "created"
        // genau das, was der Aufrufer als "kein Spiel" liest, und muss nicht eigens
        // dafuer zusammengesetzt werden.
        RAD_ControlDestroyGame(&created);
        return created;
    }

    printf("Spielstand '%s' geladen.\n", save_path);

    return created;
}

void RAD_ControlDestroyGame(RAD_ControlGame_t *game)
{
    // Ohne Pruefung auf NULL, und das ist keine Nachlaessigkeit: beide Funktionen
    // geben einen Zeiger frei und setzen ihn auf NULL, und beide vertragen einen,
    // der schon NULL ist -- free(NULL) ist erlaubt. Ein halb entstandenes Paar,
    // ein zweimal abgebautes und ein nie benutztes laufen damit ueber denselben
    // Weg, ohne dass hier drei Faelle stehen muessten.
    //
    // Erst das Spiel, dann sein Manager: das Spiel haelt einen Zeiger auf ihn.
    RAD_DestroyGame(&game->game);
    RAD_DestroyEventManager(&game->event_manager);
}

///
/// Das leere Spiel, in das geladen wird -- und das Ergebnis, wenn es nichts zu
/// laden gibt.
///
/// Bei einem Fehlschlag kommt ein Paar aus zwei NULL heraus und nichts bleibt
/// stehen: der Manager wird wieder abgebaut, wenn das Spiel an ihm nicht zustande
/// kommt. Nur eines von beiden waere ein Leck, das erst beim Beenden auffiele.
///
static RAD_ControlGame_t RAD_ControlCreateEmptyGame(void)
{
    RAD_ControlGame_t created = {
        .game = NULL,
        .event_manager = NULL
    };

    created.event_manager = RAD_CreateEventManager();
    if(created.event_manager == NULL)
    {
        return created;
    }

    // RAD_USER_NONE: der Server sitzt an keinem Client. Kommandos, die er selbst
    // erzeugt, haetten keinen Absender -- wer mitspielt, fuehrt die Steuerung,
    // und der Absender eines eingehenden Kommandos steht in dessen Kopf.
    created.game = RAD_CreateGame(created.event_manager, RAD_USER_NONE);
    if(created.game == NULL)
    {
        RAD_DestroyEventManager(&created.event_manager);
        return created;
    }

    return created;
}
