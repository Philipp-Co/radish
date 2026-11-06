#ifndef __RAD_CONTROL_VISIBILITY_H__
#define __RAD_CONTROL_VISIBILITY_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/model/world.h>

///
/// Die eigene Sicht, nachgerechnet im Client: dieselbe Regel wie
/// RAD_GameIsTileVisible im Spiel (radish/game/src/control/visibility.c), damit
/// der Nebel dort liegt, wo der Server nichts zeigt. Wer die eine Seite aendert,
/// aendert die andere mit -- wie bei Sichtlinie und Wegkosten
/// (control/line_of_sight.h, control/terrain.h).
///
/// **Die Regel:** ein Feld ist sichtbar, wenn eine eigene Einheit auf dem Feld
/// es sieht: es ist hoechstens RAD_ClientUnit_t.sight_range Felder entfernt,
/// waagerecht plus senkrecht, und die Sichtlinie ist frei
/// (RAD_ControlHasLineOfSight -- Gelaende und Einheiten versperren). Ihr eigenes
/// Feld sieht jede.
///
/// **Nur eine Anzeige.** Was der Spieler sieht, entscheidet der Server; der Client
/// zeigt nur, wo. Eine fremde Einheit, die er nicht kennt, kann auf dem Server
/// die Sicht versperren -- dann zeigt der Client ein Feld hell, auf dem trotzdem
/// nichts erscheint.
///

///
/// Sieht "player" das Feld (x, y) in "world"? false fuer eine NULL-Welt, eine
/// Stelle ausserhalb der Welt oder RAD_CLIENT_PLAYER_NONE.
///
bool RAD_ControlTileVisible(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t player, int32_t x, int32_t y);

///
/// Wo eine Einheit fuer die Sicht steht, wenn es nicht ihr Feld in der Welt ist:
/// waehrend eines Zuges steht sie dort schon auf dem Ziel, auf dem Bildschirm
/// aber noch unterwegs (view/iso_map_view.c). Gerechnet wird dann von hier aus.
///
typedef struct
{
    RAD_ClientUnitId_t unit;
    int32_t x;
    int32_t y;
} RAD_ControlUnitPosition_t;

///
/// Alles, was "player" sieht, auf einmal: visible[y][x] fuer jedes Feld der
/// Welt, false dahinter. Gerechnet von den eigenen Einheiten aus und nicht je
/// Feld -- so wird nur geprueft, was in ihrer Sichtweite liegt, und das laesst
/// sich jedes Bild neu tun.
///
/// Fuer RAD_CLIENT_PLAYER_NONE -- der Client kennt seine Spieler-Id noch nicht --
/// ist alles sichtbar: ohne zu wissen, wer man ist, gibt es keinen Nebel.
///
/// **"positions"**, "number_of_positions" Eintraege, darf NULL sein: fuer jede
/// Einheit darin wird von dieser Stelle aus gerechnet statt von ihrem Feld --
/// so wandert die Sicht mit einer Einheit, die gerade faehrt. Die Sichtlinie
/// laeuft dabei durch die Welt, wie sie ist; steht die Einheit dort schon auf dem
/// Ziel, versperrt sie, was dahinter liegt, wie jede andere.
///
void RAD_ControlVisibility(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t player,
                           const RAD_ControlUnitPosition_t *positions, size_t number_of_positions,
                           bool visible[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE]);

#endif
