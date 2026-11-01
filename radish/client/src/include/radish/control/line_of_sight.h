#ifndef __RAD_CONTROL_LINE_OF_SIGHT_H__
#define __RAD_CONTROL_LINE_OF_SIGHT_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/model/world.h>

///
/// Die Sichtlinie beim Angriff, nachgerechnet im Client: dieselbe Regel wie
/// RAD_GameHasLineOfSight im Spiel (radish/game/src/control/attack_unit.c), damit
/// die Zielwahl nur markiert, was der Server annimmt
/// (view/context_menu/context_menu_view.c). Wer die eine Seite aendert, aendert
/// die andere mit -- wie bei den Wegkosten (control/terrain.h).
///
/// **Die Regel:** die Linie laeuft von der Mitte des einen Feldes zur Mitte des
/// anderen. Jedes Feld, durch das sie geht, ausser den beiden Enden, versperrt
/// die Sicht, wenn sein Gelaende sichtblockierend ist
/// (RAD_ClientTerrain_t.blocks_sight) oder eine Einheit darauf steht. Geht sie
/// genau durch eine Ecke, ist sie dort nur versperrt, wenn beide Felder daneben
/// es sind. Nachbarn sehen sich immer, ein Feld sich selbst auch.
///

///
/// Ob das Feld (x, y) die Sicht versperrt -- die Frage, die RAD_ControlLineOfSight
/// an jedes Feld auf der Linie stellt.
///
typedef bool (*RAD_ControlBlocksSight_t)(void *user_argument, int32_t x, int32_t y);

///
/// Die Regel ohne Welt: laeuft die Linie von (from_x, from_y) nach (to_x, to_y)
/// und fragt "blocks_sight" nach jedem Feld dazwischen. Getrennt von der Welt,
/// damit sie sich gegen das Spiel vergleichen laesst. "blocks_sight" darf nicht
/// NULL sein.
///
bool RAD_ControlLineOfSight(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y,
                            RAD_ControlBlocksSight_t blocks_sight, void *user_argument);

///
/// Sieht (from_x, from_y) das Feld (to_x, to_y) in "world"? Ein Feld, das der
/// Client nicht kennt (RAD_ClientWorldTileAt liefert NULL), versperrt nichts --
/// entscheiden wird ohnehin der Server. false fuer eine NULL-Welt.
///
bool RAD_ControlHasLineOfSight(const RAD_ClientWorld_t *world, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y);

#endif
