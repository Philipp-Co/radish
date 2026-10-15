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

///
/// Zeichnet eine Dekoration auf ein Feld, dessen Raute bei (screen_x, screen_y)
/// beginnt -- Kamera und Ebene schon eingerechnet, wie bei RAD_RenderIsoEntity.
/// "user_argument" ist das aus RAD_IsoDecoration_t.
///
typedef void (*RAD_IsoDecorationRender_t)(SDL_Renderer *renderer, const void *user_argument,
                                          int32_t screen_x, int32_t screen_y);

///
/// Etwas, das auf einem Feld hinter oder vor der Einheit gezeichnet wird
/// (RAD_IsoObject_t.background, .foreground). Das Iso-Objekt weiss nicht, was es
/// ist: es ruft nur "render" mit "user_argument" auf. "render" NULL heisst:
/// nichts.
///
/// **Gehoert dem, der sie setzt.** Das Iso-Objekt gibt nichts frei;
/// "user_argument" muss gueltig bleiben, solange die Dekoration gesetzt ist.
///
typedef struct
{
    RAD_IsoDecorationRender_t render;
    const void *user_argument;
} RAD_IsoDecoration_t;

#define RAD_ISO_DECORATION_NONE ((RAD_IsoDecoration_t){ .render = NULL, .user_argument = NULL })


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
    /// **Gehoert dem, der es setzt** (wie die Dekorationen): das Iso-Objekt gibt
    /// nichts frei. Gesetzt bei jedem RAD_IsoMapApplyTile vom Typ des Feldes
    /// (rendering/iso_tile.h, rendering/iso_tile_images.h); RAD_CreateIsoObject
    /// setzt NULL.
    ///
    SDL_Texture *image;

    ///
    /// Die Dekorationen des Feldes: "background" nach dem Wuerfel und vor den
    /// Figuren, "foreground" nach ihnen. Beide gehoeren dem Typ des Feldes und
    /// werden bei jedem RAD_IsoMapApplyTile neu gesetzt (rendering/iso_tile.h);
    /// RAD_CreateIsoObject setzt RAD_ISO_DECORATION_NONE.
    ///
    RAD_IsoDecoration_t background;
    RAD_IsoDecoration_t foreground;

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
