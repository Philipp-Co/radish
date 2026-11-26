#ifndef __RAD_VIEW_RENDERING_ISO_TILE_IMAGES_H__
#define __RAD_VIEW_RENDERING_ISO_TILE_IMAGES_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <radish/model/tile.h>
#include <radish/view/rendering/iso_decor.h>
#include <radish/view/rendering/iso_object.h>

///
/// Die Bilder der Felder (RAD_IsoObject_t.image und die Dekorationen,
/// RAD_IsoDecor_t), einmal geladen und von allen Feldern eines Typs geteilt.
///
/// **Welcher Typ welche Bilder hat, sagt seine tile.json**
/// (view/rendering/iso_tile_sprite.h, client/schema/tile.schema.json) unter
/// assets/tiles/<typ>/ -- nicht der Code. Die Dateien kommen aus der Datenbank
/// des Backends (api/domain/tiles/); das Frontend legt sie vor main() ins
/// Dateisystem des Moduls (client/publish_assets.cmake). Ein Typ ohne tile.json
/// wird als Wuerfel in seiner Farbe gezeichnet (view/rendering/iso_tile/).
///
/// Eine Textur haengt am Renderer, RAD_IsoTileApplyType hat aber keinen:
/// deshalb laedt main() alle Bilder nach dem Renderer vorab, und
/// RAD_IsoTileApplyType holt sie sich mit RAD_IsoTileImagesApply.
///
/// **Alles in festen Plaetzen**: je Typ eine Beschreibung und hoechstens
/// 1 + RAD_ISO_DECOR_PER_TILE Texturen, ein statischer Lesepuffer.
///

///
/// Laedt die Bilder aller Typen fuer "renderer". false, wenn eine tile.json
/// ungueltig ist oder ein Bild darin fehlt -- der Grund steht auf der Konsole;
/// die uebrigen sind trotzdem geladen. Ein Typ ohne tile.json ist kein Fehler.
///
bool RAD_IsoTileImagesLoad(SDL_Renderer *renderer);

///
/// Gibt alle Bilder frei. Vor dem Renderer: die Texturen haengen an ihm.
///
void RAD_IsoTileImagesFree(void);

///
/// Setzt das Bild des Feldes "object" (RAD_IsoObject_t.image) und fuellt
/// "decor" mit den Dekorationen des Typs "type", ab dem ersten Platz. Was der
/// Typ nicht hat oder was nicht geladen ist, bleibt NULL -- ohne Bild der
/// Wuerfel, ohne Dekoration nichts. Die Dekorationen sind danach noch nicht auf
/// das Feld gesetzt (RAD_IsoDecorPlace).
///
void RAD_IsoTileImagesApply(RAD_ClientTileType_t type, RAD_IsoObject_t *object,
                            RAD_IsoDecor_t decor[RAD_ISO_DECOR_PER_TILE]);

#endif
