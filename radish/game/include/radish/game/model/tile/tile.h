#ifndef __RAD_TILE_H__
#define __RAD_TILE_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/model/model.h>

///
/// Ein Feld des Rasters.
///
/// Oeffentlich wie die Entitaet, und aus demselben Grund: ein Tile ist etwas, das
/// aus dem Spiel herauskommt. RAD_OnTileAddedToGame_t und seine beiden Nachbarn
/// (event_manager.h) reichen ein const RAD_Tile_t* an ihre Abonnenten, und wer
/// darauf hin zeichnen soll, muss x, y und z lesen koennen. Die Welt, in der das
/// Tile steht, bleibt privat -- ein Abonnent erfaehrt von einem Feld, nicht vom
/// Raster.
///
/// Lesen heisst hier nicht schreiben: die Zeiger nach draussen sind const, und
/// wer ein Tile aendern will, geht ueber die Welt, die RAD_Tile_t.entity und
/// RAD_Entity_t.x/y synchron haelt (world.h).
///
/// **Der Name steht hier und nicht in model.h.** Dort stehen nur die drei
/// Strukturen, die unvollstaendig bleiben -- ein Name ohne Inhalt braucht eine
/// Datei, in der er allein stehen kann. Tile hat beides an einer Stelle, also
/// steht beides hier: wer RAD_Tile_t nennt, bindet diese Datei ein.
///
/// **Und mit dem Namen der Zugriff.** Unten stehen die zwei Funktionen, mit denen
/// ein Aufrufer von aussen an die Tiles kommt. Sie heissen RAD_Game*, weil sie ein
/// Spiel nehmen -- aber sie gehoeren zum Tile, und deshalb stehen sie hier und
/// nicht in game.h: wer Tiles lesen will, braucht genau eine Datei dafuer, und wer
/// game.h liest, wird nicht mit dem Zubehoer jedes einzelnen Typs behangen.
///
typedef struct RAD_Tile RAD_Tile_t;

struct RAD_Tile
{
    ///
    /// Position in der Welt. Redundant zur Array-Position, aber noetig, sobald
    /// ein einzelnes RAD_Tile_t* weitergereicht wird -- genauso haelt es
    /// RAD_IsoObject_t auf der Rendering-Seite.
    ///
    int32_t x;
    int32_t y;
    int32_t z;

    RAD_TileType_t type;

    ///
    /// Entitaet auf diesem Tile, RAD_ENTITY_NONE wenn frei. Pro Tile kann es zu
    /// jedem Zeitpunkt hoechstens eine geben.
    ///
    RAD_EntityId_t entity;
};

///
/// Die Tiles eines Spiels zum Nachlesen: erst zaehlen, dann einzeln holen. Wer
/// alle sehen will, laeuft von 0 bis unter die Anzahl. Dasselbe Muster wie bei den
/// Figuren (entity.h) und in turn.h.
///
/// **Warum das die einzige Lesart von aussen ist.** RAD_World_t ist nach aussen
/// nur ein Name (model.h), ein Aufrufer kommt also nicht an das Raster selbst.
/// Diese zwei Funktionen sind der Ersatz -- und RAD_GameTileAt gibt eine *Kopie*
/// heraus, keinen Zeiger in die Welt. Daran haengt die Kapselung:
///
///   - Ein Zeiger liesse sich beschreiben, und damit waere die Welt an den Regeln
///     vorbei zu aendern. Eine Kopie nicht.
///   - Ein Zeiger baumelt, sobald die Welt sich weiterdreht. Eine Kopie ist ein
///     Stand und bleibt gueltig -- sie wird eben alt, was ehrlicher ist.
///
/// Wer aendern will, geht ueber die Kommandos; wer den laufenden Zustand braucht,
/// fragt neu. Die Ereignisse (event_manager.h) bleiben der andere Weg heraus: sie
/// melden von selbst, statt gefragt zu werden.
///

///
/// Wie viele Tiles es gibt. Das Raster ist vollstaendig besetzt -- ein Feld ohne
/// Inhalt ist RAD_TILE_TYPE_VOID und zaehlt mit --, die Anzahl ist also
/// unveraenderlich und zugleich der einzige Weg, von aussen die Groesse der Welt
/// zu erfahren: RAD_WORLD_WIDTH und RAD_WORLD_HEIGHT liegen privat
/// (game_definitions.h). 0 fuer ein NULL-Spiel.
///
int32_t RAD_GameNumberOfTiles(const RAD_Game_t *game);

///
/// Holt das Tile an dieser Stelle. Gezaehlt wird zeilenweise, Zeile 0 zuerst:
/// Index = y * Breite + x, dieselbe Reihenfolge, in der der World-Serializer die
/// Tiles ablegt. Wer x und y braucht, liest sie aus dem Tile selbst -- es traegt
/// sie (oben).
///
/// Liefert false, wenn "game" oder "output" NULL ist oder der Index ausserhalb
/// [0, RAD_GameNumberOfTiles) liegt. **"output" bleibt dann unangetastet** -- es
/// wird nichts halb hineingeschrieben, damit ein nicht geprueftes false keinen
/// halben Stand hinterlaesst.
///
bool RAD_GameTileAt(const RAD_Game_t *game, int32_t index, RAD_Tile_t *output);

#endif
