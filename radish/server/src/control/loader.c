#include <radish/server/control/loader.h>

#include "loader/save_file.h"

#include <radish/game/control/events/event_manager.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

///
/// Zwei Schritte: erst entsteht ein leeres Spiel, dann wird -- wenn ein Pfad
/// dasteht -- ein Spielstand hineingelesen. Umgekehrt geht es nicht, denn
/// RAD_DeserializeGameFromJson fuellt ein vorhandenes Spiel und uebernimmt daraus
/// alles, was nicht im Spielstand steht (die Ereignisverwaltung, den Absender,
/// die Sequenznummer).
///
/// Der Event-Manager liegt auf dem Heap und nicht in dieser Datei: RAD_Game_t
/// haelt nur einen Zeiger auf ihn, er muss also mindestens so lange leben wie das
/// Spiel. Eine statische Variable hier taete das auch, gaebe es aber nur einmal
/// -- so haengt an jedem Spiel sein eigener, und das Modul hat keinen Zustand.
/// Abgebaut wird er in RAD_ControlDestroyGame, das ihn sich aus dem Spiel holt.
///
/// Diese Datei loggt, anders als die uebrigen Module des Servers. Sie ist die
/// einzige, die eine Datei anfasst -- was dabei schiefgeht, ist eine Frage des
/// Betriebs und gehoert ins Log, und nur hier stehen beide Ergebnisse (Datei und
/// Inhalt) nebeneinander.
///

static RAD_Game_t* RAD_ControlCreateEmptyGame(void);


RAD_Game_t* RAD_ControlCreateGame(const char *save_path)
{
    RAD_Game_t *game = RAD_ControlCreateEmptyGame();
    if(game == NULL)
    {
        printf("Spiel nicht angelegt -- kein Speicher.\n");
        return NULL;
    }

    if(save_path == NULL)
    {
        return game;
    }

    RAD_SerializeResult_t reason = RAD_SERIALIZE_OK;
    const RAD_SaveFileResult_t result = RAD_ReadGameFromSaveFile(save_path, game, &reason);

    if(result == RAD_SAVE_FILE_ERROR_CONTENT)
    {
        printf("Spielstand '%s' abgelehnt: %s\n", save_path, RAD_SerializeResultText(reason));
        RAD_ControlDestroyGame(&game);
        return NULL;
    }

    if(result != RAD_SAVE_FILE_OK)
    {
        printf("Spielstand '%s' nicht geladen: %s\n", save_path, RAD_SaveFileResultText(result));
        RAD_ControlDestroyGame(&game);
        return NULL;
    }

    printf("Spielstand '%s' geladen.\n", save_path);

    return game;
}

void RAD_ControlDestroyGame(RAD_Game_t **game)
{
    if(*game == NULL)
    {
        return;
    }

    // Erst merken, dann abbauen: nach RAD_DestroyGame ist das Spiel weg, und mit
    // ihm der einzige Zeiger auf seinen Event-Manager.
    RAD_EventManager_t *event_manager = (*game)->event_manager;

    RAD_DestroyGame(game);

    RAD_DestroyEventManager(event_manager);
    free(event_manager);
}

///
/// Das leere Spiel, in das geladen wird -- und das Ergebnis, wenn es nichts zu
/// laden gibt.
///
static RAD_Game_t* RAD_ControlCreateEmptyGame(void)
{
    RAD_EventManager_t *event_manager = malloc(sizeof(RAD_EventManager_t));
    if(event_manager == NULL)
    {
        return NULL;
    }

    *event_manager = RAD_CreateEventManager();

    // RAD_USER_NONE: der Server sitzt an keinem Client. Kommandos, die er selbst
    // erzeugt, haetten keinen Absender -- wer mitspielt, fuehrt die Steuerung,
    // und der Absender eines eingehenden Kommandos steht in dessen Kopf.
    RAD_Game_t *game = RAD_CreateGame(event_manager, RAD_USER_NONE);
    if(game == NULL)
    {
        RAD_DestroyEventManager(event_manager);
        free(event_manager);
        return NULL;
    }

    return game;
}
