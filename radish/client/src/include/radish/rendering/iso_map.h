#ifndef __RAD_MAP_H__
#define __RAD_MAP_H__

#include <SDL2/SDL_render.h>
#include <radish/rendering/iso_object.h>
#include <radish/rendering/iso_definitions.h>
#include <radish/rendering/camera.h>
#include <radish/io/net_types.h>
#include <radish/model/tile.h>


typedef struct
{
    RAD_IsoObject_t *iso_object;
} RAD_Object_t;

///
/// Wie viele Figuren gleichzeitig ausserhalb eines Feldes stehen koennen
/// (RAD_IsoMapAddEntity).
///
#define RAD_ISO_MAP_ENTITIES_MAX 8

typedef enum
{
    RAD_ISO_RENDERABLE_OBJECT = 0,
    RAD_ISO_RENDERABLE_ENTITY
} RAD_IsoRenderableType_t;

///
/// Ein Eintrag der Zeichenliste (RAD_IsoMap_t.iso_objects): ein Feld oder eine
/// Figur, die nicht auf einem Feld steht. "type" sagt, welches.
///
typedef struct
{
    RAD_IsoRenderableType_t type;
    union
    {
        /// Gehoert der Map ("data").
        RAD_IsoObject_t *object;

        /// Gehoert dem, der sie eingetragen hat (RAD_IsoMapAddEntity).
        RAD_IsoEntity_t *entity;
    };
} RAD_IsoRenderable_t;

typedef struct
{
    RAD_IsoObject_t data [3][RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];

    ///
    /// Was RAD_RenderIsoMap zeichnet, in dieser Reihenfolge: nach Ebene und
    /// Tiefe sortiert (hinten zuerst). Felder und Figuren ausserhalb eines
    /// Feldes; "number_of_entities" davon sind Figuren.
    ///
    int32_t number_of_iso_objects;
    int32_t number_of_entities;
    RAD_IsoRenderable_t iso_objects[RAD_ISO_MAP_SIZE * RAD_ISO_MAP_SIZE * 3 + RAD_ISO_MAP_ENTITIES_MAX];

    RAD_Camera_t camera;

} RAD_IsoMap_t;


RAD_IsoMap_t* RAD_CreateIsoMap(void);

///
/// Gibt die Map frei samt den Figuren auf ihren Feldern, in allen Ebenen und
/// Tiefen, und nullt den Zeiger des Aufrufers. Figuren, die noch in der
/// Zeichenliste stehen (RAD_IsoMapAddEntity), bleiben beim Aufrufer -- er
/// nimmt sie vorher heraus.
///
void RAD_DestroyIsoMap(RAD_IsoMap_t **map);
RAD_IsoObject_t* RAD_IsoObjectAtScreenCoordinates(RAD_IsoMap_t *map, int32_t screen_x, int32_t srceen_y);
void RAD_RenderIsoMap(SDL_Renderer *renderer, RAD_IsoMap_t *map);
RAD_IsoObject_t* RAD_MapAddIsoObject(RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t layer);

///
/// Zeichnet ein Feld, wie es im Model steht (model/tile.h) -- neu oder
/// geaendert. Das Iso-Objekt wird beim ersten Mal angelegt und danach nur
/// aktualisiert. Wie es aussieht, richtet sich bei jedem Aufruf nach seinem Typ
/// (RAD_IsoTileApplyType); die Figur in RAD_ISO_UNIT_DEPTH folgt der Einheit des Feldes,
/// Hinter- und Vordergrund bleiben unberuehrt, und ein Siegfeld
/// wird als solches gezeichnet (RAD_IsoObject_t.victory). Ausserhalb des
/// Rasters wird verworfen.
///
/// **Alle Felder liegen auf Ebene 0.** Das Model kennt keine Hoehe, die Karte
/// ist also flach.
///
void RAD_IsoMapApplyTile(RAD_IsoMap_t *map, const RAD_ClientTile_t *tile);

///
/// Nimmt das Feld (x, y) aus der Zeichenliste, in allen Ebenen, und gibt seine
/// Figuren in allen Tiefen frei. Ausserhalb des Rasters wird verworfen.
///
void RAD_IsoMapRemoveTile(RAD_IsoMap_t *map, uint32_t x, uint32_t y);

///
/// Traegt eine Figur ein, die nicht auf einem Feld steht -- etwa eine in
/// Bewegung. Gezeichnet wird sie bei (entity->x, entity->y) auf Ebene
/// entity->layer, eingereiht nach ihrer Tiefe wie die Felder. Steht sie genau so
/// tief wie ein Feld, liegt sie darauf.
///
/// **"entity" bleibt beim Aufrufer** und muss gueltig bleiben, bis
/// RAD_IsoMapRemoveEntity. Er darf ihre Position jederzeit aendern; die Map
/// sortiert vor jedem Zeichnen neu, solange Figuren eingetragen sind.
///
/// false, wenn schon RAD_ISO_MAP_ENTITIES_MAX eingetragen sind oder diese schon.
///
bool RAD_IsoMapAddEntity(RAD_IsoMap_t *map, RAD_IsoEntity_t *entity);

///
/// Nimmt die Figur aus der Zeichenliste; sie selbst bleibt beim Aufrufer. Eine
/// nicht eingetragene bleibt still.
///
void RAD_IsoMapRemoveEntity(RAD_IsoMap_t *map, const RAD_IsoEntity_t *entity);

void RAD_ToFlatCoordinates(RAD_IsoMap_t *map, const int32_t screen_x, const int32_t screen_y, int32_t *x, int32_t *y);

///
/// Der Mittelpunkt der Raute von Feld (x, y) auf dem Bildschirm, wie
/// RAD_RenderIsoMap sie gerade zeichnet -- also samt Kamera. Ebene 0, wie alle
/// Felder (RAD_IsoMapApplyTile). false, wenn das Feld ausserhalb des Rasters
/// liegt oder noch nicht gezeichnet wird; dann bleiben "screen_x" und
/// "screen_y" unberuehrt.
///
bool RAD_IsoMapTileScreenCenter(const RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t *screen_x, int32_t *screen_y);

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
