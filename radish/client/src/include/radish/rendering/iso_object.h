#ifndef __RAD_ISO_OBJECT_H__
#define __RAD_ISO_OBJECT_H__

#include <SDL2/SDL_render.h>
#include <stdint.h>
#include <stdbool.h>
#include <radish/rendering/entity.h>
#include <radish/rendering/iso_definitions.h>
#include <radish/rendering/camera.h>

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
/// Die Tiefen, in denen ein Iso-Objekt Figuren traegt (RAD_IsoObject_t.entities),
/// je hoechstens eine. Gezeichnet in dieser Reihenfolge nach dem Boden: der
/// Hintergrund steht auf dem Boden hinter der Mitte, der Vordergrund davor.
///
typedef enum
{
    RAD_ISO_DEPTH_BACKGROUND = 0,
    RAD_ISO_DEPTH_MID,
    RAD_ISO_DEPTH_FOREGROUND,
    RAD_ISO_DEPTHS
} RAD_IsoDepth_t;

///
/// Wo eine Einheit auf ihrem Feld steht: immer in der Mitte. Hinter- und
/// Vordergrund sind fuer anderes vorbehalten.
///
#define RAD_ISO_UNIT_DEPTH RAD_ISO_DEPTH_MID


typedef struct
{
    int32_t x;
    int32_t y;
    int32_t screen_x;
    int32_t screen_y; 

    SDL_Color background_color;
    SDL_Color front_color;

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
    int32_t layer;

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
    /// bei jedem RAD_IsoMapApplyTile vom Typ des Feldes (rendering/iso_tile.h,
    /// rendering/iso_tile_images.h); RAD_CreateIsoObject setzt NULL.
    ///
    SDL_Texture *image;

    ///
    /// Was auf dem Feld steht, etwa Gras oder Wald: gezeichnet nach dem Boden
    /// (image oder Wuerfel) und vor den Figuren, im selben Rahmen wie image,
    /// aber um overlay_offset_x nach rechts (negativ: nach links) und um
    /// overlay_offset_y nach unten verschoben (negativ: nach oben) -- die
    /// Bilder stehen nicht alle auf der Oberseite ihres Rahmens. NULL fuer
    /// nichts. Fokus und Siegfeld faerben es ein wie image.
    ///
    /// Gehoert wie image dem, der es setzt; gesetzt vom Typ des Feldes,
    /// RAD_CreateIsoObject setzt NULL und je 0.
    ///
    SDL_Texture *overlay;
    int32_t overlay_offset_x;
    int32_t overlay_offset_y;

    ///
    /// Was vor den Figuren steht, etwa der vordere Teil von hohem Gras: wie
    /// overlay, aber nach den Figuren gezeichnet, damit eine Figur zwischen
    /// overlay und overlay_front steht. Im selben Rahmen und mit demselben
    /// Versatz wie overlay. NULL fuer nichts.
    ///
    /// Gilt nur fuer Figuren auf dem Feld (entities[]): eine Figur in der
    /// Zeichenliste (RAD_IsoMapAddEntity), etwa eine in Bewegung, wird nach
    /// dem ganzen Feld gezeichnet und steht vor overlay_front.
    ///
    /// Gehoert wie overlay dem, der es setzt; RAD_CreateIsoObject setzt NULL.
    ///
    SDL_Texture *overlay_front;

    ///
    /// Die Figuren auf dem Feld, eine je Tiefe (RAD_IsoDepth_t); NULL fuer
    /// keine. Sie gehoeren dem Objekt. Eine Einheit steht in
    /// RAD_ISO_UNIT_DEPTH.
    ///
    RAD_IsoEntity_t *entities[RAD_ISO_DEPTHS];

} RAD_IsoObject_t;


RAD_IsoObject_t RAD_CreateIsoObject(int32_t x, int32_t y, int32_t layer);
void RAD_RenderIsoObject(SDL_Renderer *renderer, RAD_IsoObject_t *object, int32_t offset_x, int32_t offset_y, RAD_Camera_t *camera);

void RAD_ToIsoCoordinates(int32_t x, int32_t y, int32_t *screen_x, int32_t *screen_y);

#endif
