#ifndef __RAD_MAP_H__
#define __RAD_MAP_H__

#include <SDL2/SDL_render.h>
#include <radish/view/rendering/entity.h>
#include <radish/view/rendering/iso_decor.h>
#include <radish/view/rendering/iso_effect.h>
#include <radish/view/rendering/iso_object.h>
#include <radish/view/rendering/iso_definitions.h>
#include <radish/view/rendering/camera.h>
#include <radish/io/net_types.h>
#include <radish/model/definitions.h>
#include <radish/model/tile.h>


typedef struct
{
    RAD_IsoObject_t *iso_object;
} RAD_Object_t;

///
/// Wie viele Figuren die Map hoechstens haelt -- so viele Einheiten, wie der
/// Client kennen kann (RAD_CLIENT_UNIT_REPOSITORY_UNITS_MAX, model/unit_repository.h).
///
#define RAD_ISO_MAP_ENTITIES_MAX 64

///
/// Wie viele Effekte gleichzeitig laufen oder warten koennen (RAD_IsoMapStartEffect).
///
#define RAD_ISO_MAP_EFFECTS_MAX 8

///
/// Die Ebene eines Eintrags der Zeichenliste (RAD_IsoRenderable_t.layer),
/// von unten nach oben. Sortiert wird nach (y - x) + layer * RAD_ISO_LAYER_STEP
/// (view/rendering/iso_map.c): eine Ebene hoeher zaehlt wie ein Stueck weiter vorn.
///
#define RAD_ISO_LAYER_TILE   0
#define RAD_ISO_LAYER_DECOR  1
#define RAD_ISO_LAYER_ENTITY 2
#define RAD_ISO_LAYER_EFFECT 3

///
/// Wie weit eine Ebene einen Eintrag nach vorn rueckt, in Reihen (y - x).
/// Drei Viertel: zwei Ebenen (Feld -> Figur) sind mehr als eine Reihe, so
/// liegt der Boden, ueber den eine Figur faehrt, immer unter ihr. Und bei
/// ganzzahliger Lage hat jede der vier Ebenen einen eigenen Nachkommaanteil
/// (.0, .75, .5, .25) -- Eintraege verschiedener Ebenen sind nie gleich.
///
#define RAD_ISO_LAYER_STEP 0.75f

///
/// Was ein Eintrag der Zeichenliste ist. Fuer die Reihenfolge zaehlt er nicht,
/// nur Lage und Ebene (RAD_IsoRenderable_t.layer).
///
typedef enum
{
    RAD_ISO_RENDERABLE_OBJECT = 0,
    RAD_ISO_RENDERABLE_DECOR,
    RAD_ISO_RENDERABLE_ENTITY,
    RAD_ISO_RENDERABLE_EFFECT
} RAD_IsoRenderableType_t;

///
/// Ein Eintrag der Zeichenliste (RAD_IsoMap_t.iso_objects): ein Feld, eine
/// Dekoration, eine Figur oder ein Effekt. "type" sagt, welches.
///
typedef struct
{
    RAD_IsoRenderableType_t type;

    /// Die Ebene (RAD_ISO_LAYER_*), gesetzt beim Eintragen.
    int32_t layer;

    union
    {
        /// Gehoert der Map ("data").
        RAD_IsoObject_t *object;

        /// Gehoert der Map ("decor").
        RAD_IsoDecor_t *decor;

        /// Gehoert der Map ("entities").
        RAD_IsoEntity_t *entity;

        /// Gehoert der Map ("effects").
        RAD_IsoEffect_t *effect;
    };
} RAD_IsoRenderable_t;

typedef struct
{
    RAD_IsoObject_t data[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];

    ///
    /// Was auf den Feldern steht (view/rendering/iso_decor.h), je Feld bis zu
    /// RAD_ISO_DECOR_PER_TILE. Ohne Bild steht eine Dekoration nicht in der
    /// Zeichenliste. Gesetzt mit dem Feld (RAD_IsoMapApplyTile).
    ///
    RAD_IsoDecor_t decor[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE][RAD_ISO_DECOR_PER_TILE];

    ///
    /// Die Figuren, eine je Einheit, gefunden ueber ihre Id
    /// (RAD_IsoMapEntityOf) -- nicht ueber das Feld, auf dem sie stehen: eine
    /// Figur in Bewegung steht auf keinem. Wo sie ist, sagt sie selbst
    /// (RAD_IsoEntity_t.x, .y). Sie gehoeren der Map und stehen in der
    /// Zeichenliste; die Felder ("data") wissen nichts von ihnen.
    ///
    RAD_IsoEntity_t *entities[RAD_ISO_MAP_ENTITIES_MAX];
    int32_t number_of_entities;

    ///
    /// Die Effekte (view/rendering/iso_effect.h), feste Plaetze: "active" sagt,
    /// welche belegt sind. Sie gehoeren der Map und wandern nie, damit die
    /// Zeichenliste auf sie zeigen kann. Darin steht ein Effekt nur, solange er
    /// laeuft -- ein wartender wird nicht gezeichnet.
    ///
    RAD_IsoEffect_t effects[RAD_ISO_MAP_EFFECTS_MAX];

    ///
    /// Was RAD_RenderIsoMap zeichnet, in dieser Reihenfolge: nach Lage und
    /// Ebene sortiert (hinten zuerst). Felder, Dekorationen, Figuren und die
    /// Effekte, die gerade laufen.
    ///
    int32_t number_of_iso_objects;
    RAD_IsoRenderable_t iso_objects[RAD_ISO_MAP_SIZE * RAD_ISO_MAP_SIZE
                                    + RAD_ISO_MAP_SIZE * RAD_ISO_MAP_SIZE * RAD_ISO_DECOR_PER_TILE
                                    + RAD_ISO_MAP_ENTITIES_MAX + RAD_ISO_MAP_EFFECTS_MAX];

    RAD_Camera_t camera;

} RAD_IsoMap_t;


RAD_IsoMap_t* RAD_CreateIsoMap(void);

///
/// Gibt die Map frei samt allen ihren Figuren ("entities") und nullt den Zeiger
/// des Aufrufers.
///
void RAD_DestroyIsoMap(RAD_IsoMap_t **map);
RAD_IsoObject_t* RAD_IsoObjectAtScreenCoordinates(RAD_IsoMap_t *map, int32_t screen_x, int32_t srceen_y);
void RAD_RenderIsoMap(SDL_Renderer *renderer, RAD_IsoMap_t *map);
RAD_IsoObject_t* RAD_MapAddIsoObject(RAD_IsoMap_t *map, int32_t x, int32_t y);

///
/// Zeichnet ein Feld, wie es im Model steht (model/tile.h) -- neu oder
/// geaendert. Das Iso-Objekt wird beim ersten Mal angelegt und danach nur
/// aktualisiert. Wie es aussieht, richtet sich bei jedem Aufruf nach seinem Typ
/// (RAD_IsoTileApplyType), samt dem, was darauf steht (view/rendering/iso_decor.h);
/// die Figur auf dem Feld (RAD_IsoMapEntityAt) folgt der
/// Einheit des Feldes, ihr Bild deren Bild-Schluessel (RAD_IsoUnitSpriteOf), ihr Ring
/// "player" -- die Stelle ihres Besitzers unter den Mitspielern,
/// RAD_ISO_ENTITY_PLAYER_NONE fuer keinen (view/rendering/entity.h); ohne Einheit bleibt
/// er ungenutzt. Ein Siegfeld wird als solches gezeichnet
/// (RAD_IsoObject_t.victory). Ausserhalb des Rasters wird verworfen.
///
void RAD_IsoMapApplyTile(RAD_IsoMap_t *map, const RAD_ClientTile_t *tile, int32_t player);

///
/// Nimmt das Feld (x, y) aus der Zeichenliste und gibt die
/// Figur darauf frei -- eine, die gerade etwas tut ("busy"), bleibt stehen.
/// Ausserhalb des Rasters wird verworfen.
///
void RAD_IsoMapRemoveTile(RAD_IsoMap_t *map, uint32_t x, uint32_t y);

///
/// Die Figur der Einheit "id"; NULL, wenn die Map keine hat. Sie gehoert weiter
/// der Map. Wo sie steht, sagt sie selbst -- auch mitten in einer Bewegung.
///
RAD_IsoEntity_t* RAD_IsoMapEntityOf(RAD_IsoMap_t *map, RAD_NetEntityId_t id);

///
/// Die Figur, die auf Feld (x, y) steht; NULL fuer keine. Eine in Bewegung
/// steht auf keinem Feld und kommt hier nicht heraus.
///
RAD_IsoEntity_t* RAD_IsoMapEntityAt(RAD_IsoMap_t *map, int32_t x, int32_t y);

///
/// Wie viele Figuren die Map haelt, und die an der Stelle "index" -- fuer
/// Schleifen ueber alle. NULL ausserhalb. Die Reihenfolge ist die der Ablage
/// und aendert sich, wenn eine Figur verschwindet.
///
int32_t RAD_IsoMapEntityCount(const RAD_IsoMap_t *map);
RAD_IsoEntity_t* RAD_IsoMapEntityAtIndex(RAD_IsoMap_t *map, int32_t index);

///
/// Nimmt die Figur der Einheit "id" aus der Map und gibt sie frei; eine, die es
/// nicht gibt, bleibt still. Danach zeigt kein Zeiger auf sie mehr.
///
void RAD_IsoMapRemoveEntity(RAD_IsoMap_t *map, RAD_NetEntityId_t id);

///
/// Laesst ab "start_ms" auf (x, y) den Effekt "sprite" laufen, im Atlas fuer
/// "facing" (view/rendering/iso_effect.h). Bis dahin wartet er. Gezeichnet wird er
/// auf der obersten Ebene (RAD_ISO_LAYER_EFFECT), eingereiht nach seiner Lage.
///
/// **Der Effekt gehoert der Map**; sie bringt ihn mit RAD_IsoMapUpdateEffects
/// voran und raeumt ihn weg, wenn er durch ist. false, wenn schon
/// RAD_ISO_MAP_EFFECTS_MAX laufen oder warten.
///
bool RAD_IsoMapStartEffect(RAD_IsoMap_t *map, RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing,
                           float x, float y, float start_ms);

///
/// Bringt alle Effekte auf "now_ms": einer, dessen Zeit gekommen ist, kommt in
/// die Zeichenliste, einer, der durch ist, wieder heraus. Mit INFINITY laeuft
/// alles zu Ende.
///
void RAD_IsoMapUpdateEffects(RAD_IsoMap_t *map, float now_ms);

///
/// Ob ueberhaupt ein Effekt laeuft oder wartet.
///
bool RAD_IsoMapHasEffects(const RAD_IsoMap_t *map);

///
/// Wann auf (x, y) der Einschlag faellt: der Beginn des spaetesten Effekts, der
/// dort wartet oder laeuft. "fallback_ms", wenn dort keiner ist -- etwa bei
/// einem Fehlschuss oder wenn kein Platz fuer ihn war.
///
float RAD_IsoMapImpactMs(const RAD_IsoMap_t *map, int32_t x, int32_t y, float fallback_ms);

void RAD_ToFlatCoordinates(RAD_IsoMap_t *map, const int32_t screen_x, const int32_t screen_y, int32_t *x, int32_t *y);

///
/// Der Mittelpunkt der Raute von Feld (x, y) auf dem Bildschirm, wie
/// RAD_RenderIsoMap sie gerade zeichnet -- also samt Kamera. false, wenn das
/// Feld ausserhalb des Rasters
/// liegt oder noch nicht gezeichnet wird; dann bleiben "screen_x" und
/// "screen_y" unberuehrt.
///
bool RAD_IsoMapTileScreenCenter(const RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t *screen_x, int32_t *screen_y);

///
/// Das Rechteck in Pixeln, das die ganze Karte beim Zeichnen belegt -- in
/// Kamerakoordinaten, also so, wie es bei camera == (0, 0) auf dem Bildschirm
/// laege. "max" ist der erste Pixel dahinter.
///
/// Gerechnet ueber das volle Raster (RAD_ISO_MAP_SIZE), nicht ueber die Felder,
/// die der Server schon geschickt hat: so bleibt es fest und
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
