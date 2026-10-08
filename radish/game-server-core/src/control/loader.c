#include <radish/server/control/loader.h>

#include <radish/game/control/events/event_manager.h>

#include <stdio.h>
#include <stddef.h>

///
/// Zwei Schritte: erst entsteht ein leeres Spiel, dann wird -- wenn ein Pfad
/// dasteht -- eine Weltdefinition hineingelesen. Umgekehrt geht es nicht, denn
/// RAD_LoadWorldFromFile fuellt ein vorhandenes Spiel; die Ereignisverwaltung,
/// den Absender und die Sequenznummer bringt RAD_CreateGame mit.
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
/// einzige, die eine Datei des Spiels anfasst -- ob die Welt zustande kam, ist eine
/// Frage des Betriebs und gehoert ins Log. Angefasst wird sie mit einem
/// Funktionsaufruf: die Datei selbst, ihr Format und ihr Inhalt sind Sache des
/// Spielmoduls, und was davon schiefgegangen ist, steht in einem Wert.
///

static RAD_ControlGame_t RAD_ControlCreateEmptyGame(RAD_EventCallbacks_t *callbacks);


RAD_ControlGame_t RAD_ControlCreateGame(const char *world_path, RAD_EventCallbacks_t *callbacks)
{
    RAD_ControlGame_t created = RAD_ControlCreateEmptyGame(callbacks);
    if(created.game == NULL)
    {
        printf("Spiel nicht angelegt -- kein Speicher.\n");
        return created;
    }

    if(world_path == NULL)
    {
        return created;
    }

    // Ein Ergebnis fuer Datei und Inhalt: ob die Datei fehlte oder ihr Inhalt
    // abgelehnt wurde, steht in demselben Wert, und fuer den Betrieb ist es
    // dieselbe Nachricht -- geladen wurde nichts, und warum, sagt der Text.
    const RAD_GameLoadResult_t result = RAD_LoadWorldFromFile(created.game, world_path);
    if(result != RAD_GAME_LOAD_OK)
    {
        printf("Welt '%s' nicht geladen: %s\n", world_path, RAD_GameLoadResultText(result));

        // Baut beides ab und setzt beide Zeiger auf NULL -- damit ist "created"
        // genau das, was der Aufrufer als "kein Spiel" liest, und muss nicht eigens
        // dafuer zusammengesetzt werden.
        RAD_ControlDestroyGame(&created);
        return created;
    }

    printf("Welt '%s' geladen, %d x %d Felder.\n",
        world_path, RAD_GameWorldWidth(created.game), RAD_GameWorldHeight(created.game));

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
static RAD_ControlGame_t RAD_ControlCreateEmptyGame(RAD_EventCallbacks_t *callbacks)
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

    // Abonniert wird vor RAD_CreateGame: das Spiel baut seine Welt schon beim
    // Anlegen auf und meldet dabei jedes Feld. Wer erst danach abonniert, hat den
    // Aufbau verpasst -- und kennt nur die Felder, die sich spaeter aendern.
    RAD_EventManagerSubscribeToTileEvents(created.event_manager, callbacks->tile_changed);
    RAD_EventManagerSubscribeToUnitEvents(created.event_manager, callbacks->unit_changed);
    RAD_EventManagerSubscribeToTurnEvents(created.event_manager, callbacks->turn_changed);

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
