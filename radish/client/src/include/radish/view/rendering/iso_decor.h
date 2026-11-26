#ifndef __RAD_VIEW_RENDERING_ISO_DECOR_H__
#define __RAD_VIEW_RENDERING_ISO_DECOR_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/view/rendering/camera.h>
#include <radish/view/rendering/iso_definitions.h>
#include <radish/view/rendering/iso_object.h>

///
/// Was auf einem Feld steht, ohne darauf zu laufen: Gras, Wald, Drachenzaehne.
///
/// **Ein eigener Eintrag der Zeichenliste**, kein Teil des Feldes mehr: so
/// kann eine Figur zwischen zwei Dekorationen desselben Feldes stehen -- hinter
/// der vorderen Haelfte des hohen Grases, vor der hinteren -- und das Gras
/// braucht dafuer keine Sonderbehandlung beim Zeichnen des Feldes.
///
/// **Sortiert wird sie mit der Tiefe ihres Feldes**, nicht mit der ihres
/// eigenen Rahmens: ihr Bild sitzt meist hoeher als das Feld (offset_y ist
/// negativ, die Krone eines Baumes ragt darueber hinaus), und nach dem Rahmen
/// sortiert landete sie hinter ihrem eigenen Feld. Gegen die Figuren ordnet
/// sie ihre Ebene (RAD_ISO_LAYER_DECOR, view/rendering/iso_map.h).
///
typedef struct
{
    ///
    /// Das Bild, gestreckt auf RAD_ISO_TILE_WIDTH x 2 * RAD_ISO_TILE_HEIGHT ab
    /// (screen_x, screen_y) -- derselbe Rahmen wie beim Feld. NULL heisst: hier
    /// steht nichts; dann steht die Dekoration auch nicht in der Zeichenliste.
    ///
    /// **Gehoert dem, der es setzt** (view/rendering/iso_tile_images.h), wie das Bild
    /// eines Feldes.
    ///
    SDL_Texture *image;

    ///
    /// Wie weit ihr Rahmen gegen den ihres Feldes verschoben ist: nach rechts
    /// und nach unten, negativ nach links und nach oben. Die Bilder stehen nicht
    /// alle auf der Oberseite ihres Rahmens.
    ///
    int32_t offset_x;
    int32_t offset_y;

    ///
    /// Ob sie vor den Figuren steht. Gelesen aus der tile.json, beim Sortieren
    /// derzeit nicht beachtet: dort zaehlen nur Lage und Ebene (iso_map.h).
    ///
    bool in_front;

    ///
    /// Ihr Feld -- davon kommen Fokus, Siegfeld und Helligkeit beim Zeichnen.
    /// Sie fuehrt nichts davon selbst mit, sonst muesste der Nebel beides
    /// setzen. NULL, solange sie nicht gesetzt ist (RAD_IsoDecorPlace).
    ///
    const RAD_IsoObject_t *tile;

    ///
    /// Wo ihr Rahmen beginnt, ohne Kamera, und die Tiefe ihres Feldes. Beides
    /// setzt RAD_IsoDecorPlace.
    ///
    int32_t screen_x;
    int32_t screen_y;
    float depth;
} RAD_IsoDecor_t;

///
/// Wie viele Dekorationen ein Feld tragen kann. Zwei: das hohe Gras steht mit
/// einer Haelfte vor und mit der anderen hinter den Figuren.
///
#define RAD_ISO_DECOR_PER_TILE 2

///
/// Wie weit eine Dekoration hoechstens ueber den Ankerpunkt ihres Feldes nach
/// oben hinausragt (offset_y, negativ): eine Feldhoehe, mehr laesst die
/// tile.json nicht zu (RAD_ISO_TILE_DECOR_OFFSET_MAX, iso_tile_sprite.h). Der
/// Wald nutzt das ganz aus. Danach richtet sich, wie weit die Kamera nach oben
/// laufen darf (RAD_IsoMapScreenBounds).
///
#define RAD_ISO_DECOR_MAX_RISE RAD_ISO_TILE_HEIGHT

///
/// Setzt die Dekoration auf das Feld "tile": Rahmen samt Versatz, Tiefe und das
/// Feld, dem sie ihre Farbe abnimmt. Ohne Bild geschieht nichts.
///
void RAD_IsoDecorPlace(RAD_IsoDecor_t *decor, const RAD_IsoObject_t *tile);

///
/// Zeichnet die Dekoration, gefaerbt wie ihr Feld. Ohne Bild oder ohne Feld
/// geschieht nichts.
///
void RAD_RenderIsoDecor(SDL_Renderer *renderer, const RAD_IsoDecor_t *decor, const RAD_Camera_t *camera);

#endif
