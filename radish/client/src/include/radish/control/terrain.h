#ifndef __RAD_CONTROL_TERRAIN_H__
#define __RAD_CONTROL_TERRAIN_H__

#include <stdint.h>
#include <radish/model/tile.h>

///
/// Was das Terrain eines Feldes (model/tile.h) fuer das Spiel bedeutet: wie es
/// heisst, um wie viele Felder es die Bewegungsreichweite aendert (MV) und wie
/// viel Deckung es gibt (CV).
///
/// **Alles haengt nur am Typ des Feldes** und steht in einer Tabelle in
/// control/terrain.c. Der Server schickt es nicht, und das Spiel kennt noch
/// keine Werte; vorerst sind alle 0. Ein Typ, den die Tabelle nicht kennt,
/// heisst "Unbekannt" und hat ebenfalls 0.
///
/// **Aus MV folgt, was es kostet, ein Feld zu betreten**
/// (RAD_ControlTerrainMovementCost) -- der Client prueft damit den Weg einer
/// ziehenden Einheit gegen ihre Reichweite (view/context_menu/context_menu_view.h).
///

///
/// Der Name des Terrains von "tile" zum Anzeigen, z.B. "Wasser". Ein
/// statischer Text: nicht freigeben. "tile" darf nicht NULL sein.
///
const char* RAD_ControlTerrainName(const RAD_ClientTile_t *tile);

///
/// Um wie viele Felder "tile" die Bewegungsreichweite aendert, negativ wie
/// positiv. "tile" darf nicht NULL sein.
///
int32_t RAD_ControlTerrainMovementModifier(const RAD_ClientTile_t *tile);

///
/// Was es kostet, "tile" zu betreten, in Feldern der Bewegungsreichweite:
/// 1 - MV (RAD_ControlTerrainMovementModifier), mindestens 1 -- ein Feld, das
/// die Reichweite um eins senkt, kostet also zwei. "tile" darf nicht NULL
/// sein.
///
int32_t RAD_ControlTerrainMovementCost(const RAD_ClientTile_t *tile);

///
/// Die Deckung, die "tile" gibt. "tile" darf nicht NULL sein.
///
int32_t RAD_ControlTerrainCover(const RAD_ClientTile_t *tile);

#endif
