#ifndef __RAD_VIEW_RENDERING_ISO_TILE_H__
#define __RAD_VIEW_RENDERING_ISO_TILE_H__

#include <radish/view/rendering/iso_object.h>
#include <radish/view/rendering/iso_decor.h>
#include <radish/model/tile.h>

///
/// Wie ein Feld je nach Typ (model/tile.h) aussieht. Die Farbe der oberen
/// Flaeche (RAD_IsoObject_t.background_color) steht im Code, je Typ eine Datei in
/// view/rendering/iso_tile/; das Bild statt des Wuerfels (RAD_IsoObject_t.image)
/// und die Dekorationen kommen aus der tile.json des Typs
/// (view/rendering/iso_tile_images.h). Siegfeld und Fokus gehen beim Zeichnen der
/// Flaeche weiter vor (RAD_RenderIsoObject).
///

///
/// Richtet "object" nach dem Typ seines Feldes ein. Setzt alles, was vom Typ
/// abhaengt -- ein Feld, das den Typ wechselt, behaelt nichts vom alten. Ein Typ
/// ausserhalb der Aufzaehlung sieht aus wie RAD_CLIENT_TILE_TYPE_UNKNOWN.
/// "object" darf nicht NULL sein.
///
///
/// Richtet das Feld "object" nach seinem Typ ein und fuellt "decor" mit dem, was
/// darauf steht -- RAD_ISO_DECOR_PER_TILE Plaetze, die ungenutzten mit "image"
/// auf NULL. Die Dekorationen sind danach auf dem Feld gesetzt
/// (RAD_IsoDecorPlace).
///
void RAD_IsoTileApplyType(RAD_IsoObject_t *object, RAD_IsoDecor_t *decor, RAD_ClientTileType_t type);

#endif
