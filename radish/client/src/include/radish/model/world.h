#ifndef __RAD_MODEL_WORLD_H__
#define __RAD_MODEL_WORLD_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_types.h>
#include <radish/rendering/iso_definitions.h>

///
/// Was der Client von der Welt weiss -- und nur das.
///
/// **Ein Abbild, keine Simulation.** Gefuellt wird es aus dem, was der Server
/// schickt: den Feldern einer Discover-Antwort, den Tile-Ereignissen und den
/// Zuegen, die er bestaetigt hat. Regeln wendet es keine an; ob ein Zug geht,
/// entscheidet der Server. Der User-Input liest hier nach, was auf einem Feld
/// liegt, um daraus eine Anfrage zu bauen.
///
/// Das Raster ist so gross wie das der Darstellung (RAD_ISO_MAP_SIZE). Ein Feld
/// ausserhalb davon wird verworfen, genau wie in der Iso-Map.
///
typedef struct
{
    RAD_NetTile_t tiles[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];

    ///
    /// Ob der Server dieses Feld schon geschickt hat. Ein unbekanntes Feld ist
    /// keines -- RAD_ClientWorldTileAt liefert dafuer false.
    ///
    bool known[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];

    uint32_t width;
    uint32_t height;
} RAD_ClientWorld_t;

///
/// Bringt "world" in den Grundzustand: kein Feld bekannt, Groesse null.
///
void RAD_ClientWorldInit(RAD_ClientWorld_t *world);

void RAD_ClientWorldSetSize(RAD_ClientWorld_t *world, uint32_t width, uint32_t height);

///
/// Uebernimmt ein Feld, wie es der Server schickt -- neu oder geaendert, das
/// Feld ersetzt, was hier vorher stand.
///
void RAD_ClientWorldApplyTile(RAD_ClientWorld_t *world, const RAD_NetTile_t *tile);

void RAD_ClientWorldRemoveTile(RAD_ClientWorld_t *world, uint32_t x, uint32_t y);

///
/// Das Feld (x, y), oder false, wenn es ausserhalb liegt oder noch nicht bekannt
/// ist. "output" wird nur bei true beschrieben.
///
bool RAD_ClientWorldTileAt(const RAD_ClientWorld_t *world, int32_t x, int32_t y, RAD_NetTile_t *output);

///
/// Setzt die Figur "entity" vom Start des Weges auf sein Ende. Fuer einen Zug,
/// den der Server bestaetigt hat -- er schickt die Aenderung der Felder bislang
/// nicht als Tile-Ereignis mit, also traegt der Client sie selbst nach.
///
/// Steht die Figur nicht auf steps_to[0], geschieht nichts.
///
void RAD_ClientWorldMoveEntity(RAD_ClientWorld_t *world, RAD_NetEntityId_t entity, const RAD_NetPath_t *path);

#endif
