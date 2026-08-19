#ifndef __RAD_MODEL_GAME_H__
#define __RAD_MODEL_GAME_H__

#include <radish/game/model/model.h>
#include <radish/game/model/world/world.h>
#include <radish/game/model/turn/turn.h>
#include <radish/game/user.h>
#include <radish/game/control/events/event_manager.h>
#include <radish/game/control/command/command.h>

///
/// Der Spielzustand -- was hinter dem Namen aus game.h steht.
///
/// Er liegt unter model/, weil ein Spiel Zustand ist: eine Welt, ein Zug und wer
/// davor sitzt. Die Schnittstelle dazu ist die andere Datei, die game.h heisst
/// (im oeffentlichen Baum); dort ist RAD_Game_t nur ein Name. Wer diese hier
/// einbindet, uebersetzt innerhalb von radish_game -- von aussen gibt es keinen
/// Suchpfad dorthin.
///
/// Die Trennung ist der Grund, aus dem ein Client nicht mehr in der Welt
/// schreiben kann, ohne die Regeln zu fragen: er hat die Struktur nicht.
///

struct RAD_CommandListItem
{
    RAD_Command_t command;
    struct RAD_CommandListItem *next;
};

typedef struct
{
    struct RAD_CommandListItem *head;
} RAD_CommandList_t;

///
/// Oberste Abstraktion. Ein Spiel besteht aus genau einer Welt und denen, die an
/// ihr spielen; beides liegt per Wert im Spiel, damit die Ownership eindeutig ist
/// und es nur eine Allokation gibt.
///
struct RAD_Game
{
    RAD_World_t world;

    ///
    /// Wer mitspielt, in welcher Reihenfolge und wer davon dran ist (turn.h). Das
    /// ist keine Beschreibung der Welt, sondern die Gegenseite dazu: die Welt
    /// sagt, was auf dem Feld steht, der Zug sagt, wer es dahin gestellt hat und
    /// wer als naechster darf.
    ///
    RAD_Turn_t turn;

    RAD_EventManager_t *event_manager;
    RAD_CommandList_t executed_commands;

    uint32_t current_sequence_number;

    ///
    /// Wer an diesem Spiel sitzt. Jedes hier erzeugte Kommando traegt ihn als
    /// Absender im Kopf -- die Fabriken in game.h holen ihn von hier, damit ihn
    /// nicht jede Aufrufstelle mitschleppen muss.
    ///
    /// Im Client die Uuid des Benutzers, der spielt. Im Server RAD_USER_NONE: er
    /// haelt den Zustand fuer alle und ist selbst niemand. Der Absender eines
    /// eingehenden Kommandos steht in dessen Kopf -- nicht hier.
    ///
    /// Nicht zu verwechseln mit der Reihe darueber: die sagt, wer mitspielt,
    /// dieses Feld sagt, wer an diesem einen Programm sitzt. Im Client steht die
    /// eigene Uuid deshalb zweimal da -- einmal als Absender jedes erzeugten
    /// Kommandos, einmal als einer von mehreren Mitspielern.
    ///
    RAD_UserId_t local_user;
};

///
/// Tiles setzen und wegnehmen -- die Schreibseite des Rasters am Spiel.
///
/// **Sie stehen hier und nicht in include/radish/game/game.h**, und darin liegt
/// der Unterschied, den diese Datei ausmacht: sie schreiben in der Welt. Wer das
/// darf, uebersetzt innerhalb von radish_game -- von aussen gibt es keinen
/// Suchpfad hierher, und deshalb kann niemand das Gelaende aendern, ohne die Regeln
/// zu fragen. Von draussen fuehrt der Weg ueber ein Kommando
/// (control/command/create_tile.h und remove_tile.h); dass es noch niemand
/// verdrahtet hat, aendert an der Grenze nichts.
///
/// Beide reichen an die Welt weiter und halten selbst keine Regel: die Uebergaenge,
/// die Ereignisse und der Grund, warum eine Figur ihr Gelaende haelt, stehen in
/// world.h. Was hier dazukommt, ist die Pruefung auf ein Spiel, das es nicht gibt
/// -- ein NULL-Spiel ist false und kein Absturz, wie auf der Leseseite (tile.h).
///
bool RAD_GameAddTile(RAD_Game_t *game, int32_t x, int32_t y, int32_t z, RAD_TileType_t type);
bool RAD_GameRemoveTile(RAD_Game_t *game, int32_t x, int32_t y);

#endif
