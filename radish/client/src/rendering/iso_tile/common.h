#ifndef __RAD_RENDERING_ISO_TILE_COMMON_H__
#define __RAD_RENDERING_ISO_TILE_COMMON_H__

#include <radish/rendering/iso_tile.h>

///
/// Die Einrichtung je Typ, eine Datei je Funktion (rendering/iso_tile/). Nicht
/// unter include/: aufgerufen wird nur aus RAD_IsoTileApplyType. Jede setzt
/// alles, was vom Typ abhaengt (rendering/iso_tile.h).
///
void RAD_IsoTileInitVoid(RAD_IsoObject_t *object);
void RAD_IsoTileInitGround(RAD_IsoObject_t *object);
void RAD_IsoTileInitWaterShallow(RAD_IsoObject_t *object);
void RAD_IsoTileInitMud(RAD_IsoObject_t *object);
void RAD_IsoTileInitMeadow(RAD_IsoObject_t *object);
void RAD_IsoTileInitForest(RAD_IsoObject_t *object);
void RAD_IsoTileInitWaterDeep(RAD_IsoObject_t *object);
void RAD_IsoTileInitCity(RAD_IsoObject_t *object);
void RAD_IsoTileInitRoad(RAD_IsoObject_t *object);
void RAD_IsoTileInitFortification(RAD_IsoObject_t *object);
void RAD_IsoTileInitUnknown(RAD_IsoObject_t *object);

///
/// Eine Anordnung, wie sie eine Dekoration als "user_argument" bekommt: eine
/// Tabelle (in der Datei ihres Typs) und wie viele Eintraege sie hat. Was ein
/// Eintrag ist, weiss nur der Callback, der sie liest.
///
typedef struct
{
    const void *entries;
    int32_t number_of_entries;
} RAD_IsoTileLayout_t;

///
/// Formen fuer Dekorationen (RAD_IsoDecoration_t), die mehrere Typen nutzen
/// koennen (rendering/iso_tile/decorations.c). Koordinaten in Pixeln auf dem
/// Bildschirm, wie sie ein RAD_IsoDecorationRender_t bekommt.
///

///
/// Eine Tanne, deren Krone bei (x, y) aufsitzt: ein Dreieck "height" hoch und
/// "width" breit, links heller als rechts, darunter ein kurzer Stamm.
///
void RAD_IsoTileDrawTree(SDL_Renderer *renderer, int32_t x, int32_t y, int32_t height, int32_t width);

///
/// Ein Grasbueschel, das bei (x, y) aufsitzt: drei spitze Halme nebeneinander,
/// der mittlere hoeher und heller als die aeusseren.
///
void RAD_IsoTileDrawTuft(SDL_Renderer *renderer, int32_t x, int32_t y);

///
/// Ein einzelner Grashalm, der bei (x, y) aufsitzt: ein schmales Dreieck
/// "height" hoch, dessen Spitze um "lean" Pixel zur Seite kippt.
///
void RAD_IsoTileDrawBlade(SDL_Renderer *renderer, int32_t x, int32_t y, int32_t height, int32_t lean);

///
/// Ein Haus, dessen Grundriss eine Raute um (x, y) ist, "half_width" nach links
/// und rechts und halb so weit nach oben und unten -- wie ein kleines Feld. Darauf
/// Waende "height" hoch und ein Pyramidendach "roof_height" hoch.
///
void RAD_IsoTileDrawHouse(SDL_Renderer *renderer, int32_t x, int32_t y,
                          int32_t half_width, int32_t height, int32_t roof_height);

///
/// Ein Wall aus Sandsaecken von (x0, y0) nach (x1, y1), "height" hoch: ein
/// Band, durch Fugen in Saecke geteilt.
///
void RAD_IsoTileDrawSandbagWall(SDL_Renderer *renderer, int32_t x0, int32_t y0,
                                int32_t x1, int32_t y1, int32_t height);

#endif
