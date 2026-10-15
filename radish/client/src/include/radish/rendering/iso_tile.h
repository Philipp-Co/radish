#ifndef __RAD_RENDERING_ISO_TILE_H__
#define __RAD_RENDERING_ISO_TILE_H__

#include <radish/rendering/iso_object.h>
#include <radish/model/tile.h>

///
/// Wie ein Feld je nach Typ (model/tile.h) aussieht. Jeder Typ hat seine eigene
/// Funktion in rendering/iso_tile/, eine Datei je Typ; hier steht nur der
/// Verteiler.
///
/// Am Typ haengen die Farbe der oberen Flaeche (RAD_IsoObject_t.background_color),
/// das Bild statt des Wuerfels (RAD_IsoObject_t.image, rendering/iso_tile_images.h)
/// und die Dekorationen hinter und vor der Einheit (RAD_IsoObject_t.background,
/// .foreground). Die Dekorationen gehoeren damit dem Typ: jede Funktion setzt
/// beide, auch auf RAD_ISO_DECORATION_NONE. Siegfeld und Fokus gehen beim Zeichnen
/// der Flaeche weiter vor (RAD_RenderIsoObject).
///

///
/// Richtet "object" nach dem Typ seines Feldes ein. Setzt alles, was vom Typ
/// abhaengt -- ein Feld, das den Typ wechselt, behaelt nichts vom alten. Ein Typ
/// ausserhalb der Aufzaehlung sieht aus wie RAD_CLIENT_TILE_TYPE_UNKNOWN.
/// "object" darf nicht NULL sein.
///
void RAD_IsoTileApplyType(RAD_IsoObject_t *object, RAD_ClientTileType_t type);

#endif
