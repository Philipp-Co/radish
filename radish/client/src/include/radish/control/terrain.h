#ifndef __RAD_CONTROL_TERRAIN_H__
#define __RAD_CONTROL_TERRAIN_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/model/tile.h>

///
/// Was das Terrain eines Feldes (model/tile.h) fuer das Spiel bedeutet: wie es
/// heisst, um wie viele Felder es die Bewegungsreichweite aendert (MV), wie
/// viel Deckung es gibt (CV) und ob es zu betreten ist.
///
/// **Die Werte kommen vom Server**, mit jedem Feld (RAD_ClientTile_t.terrain);
/// gepflegt werden sie im Backend und gehen mit dem Spielstart ins Spiel. Der
/// Client fuehrt keine eigenen -- nur den Namen zum Anzeigen, der haengt am Typ
/// des Feldes. Ein Typ, den der Client nicht kennt, heisst "Unbekannt".
///
/// **Aus MV folgt, was es kostet, ein Feld zu betreten**
/// (RAD_ControlTerrainMovementCost) -- nach derselben Regel wie im Spiel
/// (RAD_TileMovementCost), damit der Client den Weg einer ziehenden Einheit so
/// gegen ihre Reichweite prueft, wie der Server es tut
/// (view/context_menu/context_menu_view.h).
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
/// die Reichweite um eins senkt, kostet also zwei. Ob es ueberhaupt zu betreten
/// ist, sagt RAD_ControlTerrainImpassable. "tile" darf nicht NULL sein.
///
int32_t RAD_ControlTerrainMovementCost(const RAD_ClientTile_t *tile);

///
/// Die Deckung, die "tile" gibt. "tile" darf nicht NULL sein.
///
int32_t RAD_ControlTerrainCover(const RAD_ClientTile_t *tile);

///
/// Ob eine Einheit "tile" nicht betreten darf: es ist unbegehbar
/// (RAD_ClientTerrain_t.impassable) oder hat kein Gelaende -- dieselben zwei
/// Gruende, aus denen der Server einen Weg ablehnt. "tile" darf nicht NULL sein.
///
bool RAD_ControlTerrainImpassable(const RAD_ClientTile_t *tile);

#endif
