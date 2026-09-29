#ifndef __RAD_MAP_H__
#define __RAD_MAP_H__

#include <SDL2/SDL_render.h>
#include <radish/rendering/iso_object.h>
#include <radish/rendering/iso_definitions.h>
#include <radish/rendering/camera.h>
#include <radish/io/net_types.h>


typedef struct
{
    RAD_IsoObject_t *iso_object;
} RAD_Object_t;

typedef struct
{
    RAD_IsoObject_t data [3][RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];    
   
    int32_t number_of_iso_objects; 
    RAD_IsoObject_t *iso_objects[RAD_ISO_MAP_SIZE * RAD_ISO_MAP_SIZE * 3];

    RAD_Camera_t camera;

} RAD_IsoMap_t;


RAD_IsoMap_t* RAD_CreateIsoMap(void);
RAD_IsoObject_t* RAD_IsoObjectAtScreenCoordinates(RAD_IsoMap_t *map, int32_t screen_x, int32_t srceen_y);
void RAD_RenderIsoMap(SDL_Renderer *renderer, RAD_IsoMap_t *map);
RAD_IsoObject_t* RAD_MapAddIsoObject(RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t layer);

///
/// Zeichnet ein Feld, wie es der Server schickt -- neu oder geaendert. Das
/// Iso-Objekt wird beim ersten Mal angelegt und danach nur aktualisiert; die
/// Figur darauf folgt "entity_id". Ausserhalb des Rasters wird verworfen.
///
void RAD_IsoMapApplyTile(RAD_IsoMap_t *map, const RAD_NetTile_t *tile);

void RAD_IsoMapRemoveTile(RAD_IsoMap_t *map, uint32_t x, uint32_t y);

///
/// Haengt die Figur "entity" vom Start des Weges auf sein Ende um -- fuer einen
/// Zug, den der Server bestaetigt hat.
///
void RAD_IsoMapMoveEntity(RAD_IsoMap_t *map, RAD_NetEntityId_t entity, const RAD_NetPath_t *path);

void RAD_ToFlatCoordinates(RAD_IsoMap_t *map, const int32_t screen_x, const int32_t screen_y, int32_t *x, int32_t *y);

///
/// Das Rechteck in Pixeln, das die ganze Karte beim Zeichnen belegt -- in
/// Kamerakoordinaten, also so, wie es bei camera == (0, 0) auf dem Bildschirm
/// laege. "max" ist der erste Pixel dahinter.
///
/// Gerechnet ueber das volle Raster (RAD_ISO_MAP_SIZE) und alle Ebenen, nicht
/// ueber die Felder, die der Server schon geschickt hat: so bleibt es fest und
/// springt nicht, waehrend eine Discover-Antwort eintrifft.
///
void RAD_IsoMapScreenBounds(const RAD_IsoMap_t *map, int32_t *min_x, int32_t *min_y, int32_t *max_x, int32_t *max_y);

///
/// Der Ausschnitt des Rasters, den ein Bildschirm von "screen_width" mal
/// "screen_height" Pixeln bei der jetzigen Kamera zeigt, als Rechteck in Feldern:
/// linke obere Ecke (x, y), Breite "w", Hoehe "h".
///
/// Isometrisch ist das Sichtbare eine Raute im Raster. Heraus kommt das Rechteck
/// um die vier Bildschirmecken -- es enthaelt damit auch Felder, die gerade nicht
/// zu sehen sind, aber keines, das zu sehen ist, fehlt. Begrenzt auf das Raster;
/// liegt es ganz ausserhalb, sind "w" und "h" null.
///
void RAD_IsoMapVisibleArea(RAD_IsoMap_t *map, int32_t screen_width, int32_t screen_height,
                           int32_t *x, int32_t *y, int32_t *w, int32_t *h);

#endif
