#ifndef __RAD_RENDERING_ISO_TILE_IMAGES_H__
#define __RAD_RENDERING_ISO_TILE_IMAGES_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>

///
/// Die Bilder der Felder (RAD_IsoObject_t.image), einmal geladen und von allen
/// Feldern geteilt. Eine Textur haengt am Renderer, RAD_IsoTileApplyType hat
/// aber keinen: deshalb laedt main() sie nach dem Renderer vorab, und die Typen
/// (rendering/iso_tile/) holen sie sich mit RAD_IsoTileImage.
///
/// Die Dateien liegen unter assets/tiles/ und werden ins Wasm eingebettet
/// (client/CMakeLists.txt, --embed-file).
///
typedef enum
{
    RAD_ISO_TILE_IMAGE_GRASS = 0,
    RAD_ISO_TILE_IMAGES
} RAD_IsoTileImage_t;

///
/// Laedt alle Bilder fuer "renderer". false, wenn eines fehlt; die uebrigen sind
/// trotzdem geladen, und ein Feld ohne sein Bild wird als Wuerfel gezeichnet.
///
bool RAD_IsoTileImagesLoad(SDL_Renderer *renderer);

///
/// Gibt alle Bilder frei. Vor dem Renderer: die Texturen haengen an ihm.
///
void RAD_IsoTileImagesFree(void);

///
/// Das Bild "image", oder NULL, wenn es nicht geladen ist (vor
/// RAD_IsoTileImagesLoad, nach einem Fehler, nach RAD_IsoTileImagesFree, oder
/// "image" ausserhalb der Aufzaehlung).
///
SDL_Texture* RAD_IsoTileImage(RAD_IsoTileImage_t image);

#endif
