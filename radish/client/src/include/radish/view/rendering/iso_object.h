#ifndef __RAD_ISO_OBJECT_H__
#define __RAD_ISO_OBJECT_H__

#include <SDL2/SDL_render.h>
#include <stdint.h>
#include <stdbool.h>
#include <radish/view/rendering/iso_definitions.h>
#include <radish/view/rendering/camera.h>

///
/// Die Farbe eines Siegfeldes (RAD_IsoObject_t.victory): ein kraeftiges Gold,
/// das sich vom weissen Boden abhebt.
///
#define RAD_ISO_OBJECT_VICTORY_COLOR ((SDL_Color){ 230, 180, 40, 255 })

///
/// Wie hell ein Feld im Nebel bleibt (RAD_IsoObject_t.light), von 255: das
/// Gelaende bleibt erkennbar, aber sichtbar dunkler als das, was die eigenen
/// Einheiten sehen.
///
#define RAD_ISO_OBJECT_FOG_LIGHT 110

///
/// Die Farbe der Seitenflaechen des Quaders unter der Raute. Weiss: nur die
/// Oberseite traegt die Farbe des Gelaendes (RAD_IsoObject_t.background_color),
/// die Seiten stehen fuer die Erde darunter. Fokus, Siegfeld und Nebel faerben
/// sie wie alles andere (RAD_RenderIsoObject).
///
#define RAD_ISO_OBJECT_SIDE_COLOR ((SDL_Color){ 255, 255, 255, 255 })


///
/// Ein Feld, wie es gezeichnet wird. Was auf ihm steht, kennt es nicht: die
/// Figuren (RAD_IsoMapEntityAt) und die Dekoration (view/rendering/iso_decor.h)
/// haelt die Iso-Map und zeichnet sie als eigene Eintraege der Zeichenliste.
///
typedef struct
{
    int32_t x;
    int32_t y;
    int32_t screen_x;
    int32_t screen_y; 

    SDL_Color background_color;

    bool focus;

    ///
    /// Ein Siegfeld (RAD_ClientTile_t.victory, gesetzt in RAD_IsoMapApplyTile):
    /// gezeichnet in RAD_ISO_OBJECT_VICTORY_COLOR statt der Hintergrundfarbe. Der
    /// Fokus geht vor.
    ///
    bool victory;
    ///
    /// Wie hell das Feld gezeichnet wird, von 255: jede Farbe wird damit
    /// malgenommen, auch die eines Siegfeldes; der Fokus geht vor und bleibt
    /// hell. 255 ist sichtbar, RAD_ISO_OBJECT_FOG_LIGHT liegt im Nebel -- und
    /// dazwischen ein Feld, das gerade in den Nebel faellt oder aus ihm kommt:
    /// die IsoMapView blendet je Bild ein Stueck weiter (view/iso_map_view.c).
    ///
    uint8_t light;

    ///
    /// Ob das Objekt schon in RAD_IsoMap_t.iso_objects steht. Ein Feld, das der
    /// Server ein zweites Mal schickt, wird dann aktualisiert und nicht noch
    /// einmal eingetragen.
    ///
    bool present;

    ///
    /// Das Bild des Feldes, gezeichnet statt des Wuerfels und gestreckt auf
    /// dessen Rahmen: RAD_ISO_TILE_WIDTH x 2 * RAD_ISO_TILE_HEIGHT ab der Ecke der
    /// Raute. NULL heisst: der Wuerfel in background_color/front_color wie
    /// bisher. Fokus und Siegfeld faerben das Bild ein.
    ///
    /// **Gehoert dem, der es setzt**: das Iso-Objekt gibt nichts frei. Gesetzt
    /// bei jedem RAD_IsoMapApplyTile vom Typ des Feldes (view/rendering/iso_tile.h,
    /// rendering/iso_tile_images.h); RAD_CreateIsoObject setzt NULL.
    ///
    SDL_Texture *image;

} RAD_IsoObject_t;


RAD_IsoObject_t RAD_CreateIsoObject(int32_t x, int32_t y);

///
/// Zeichnet "image" in den Rahmen bei (screen_x, screen_y) -- eine Kachel breit,
/// zwei Kachelhoehen hoch --, gefaerbt wie ein Feld: rot im Fokus, golden auf
/// einem Siegfeld, und alles mal "light" (RAD_IsoObject_t.light). Der Fokus geht
/// vor. Dieselbe Faerbung nutzt die Dekoration (view/rendering/iso_decor.h).
///
void RAD_RenderIsoImage(SDL_Renderer *renderer, SDL_Texture *image, bool focus, bool victory,
                        uint8_t light, int32_t screen_x, int32_t screen_y);
void RAD_RenderIsoObject(SDL_Renderer *renderer, RAD_IsoObject_t *object, RAD_Camera_t *camera);

void RAD_ToIsoCoordinates(int32_t x, int32_t y, int32_t *screen_x, int32_t *screen_y);

#endif
