#ifndef __RAD_RENDERING_ISO_EFFECT_H__
#define __RAD_RENDERING_ISO_EFFECT_H__

#include <SDL2/SDL_render.h>
#include <stdint.h>
#include <radish/rendering/iso_effect_images.h>
#include <radish/rendering/iso_facing.h>

///
/// Was die Darstellung von einem Effekt weiss -- etwa einer Explosion auf einem
/// Feld. Er liegt auf keinem Feld, sondern steht in der Zeichenliste der
/// Iso-Map (RAD_IsoMapAddEffect), wie eine Figur in Bewegung.
///
/// **Wem er gehoert**: dem, der ihn eingetragen hat. Er setzt auch, wie weit
/// der Effekt ist ("progress"); der Effekt selbst kennt keine Zeit.
///
typedef struct
{
    ///
    /// Wie er aussieht (rendering/iso_effect_images.h), und welcher Atlas:
    /// bei einer Explosion die Richtung, aus der der Schuss kommt.
    ///
    RAD_IsoEffectSprite_t sprite;
    RAD_IsoFacing_t facing;

    ///
    /// Wo sein Anker liegt, in Feldern, und auf welcher Ebene.
    ///
    float x;
    float y;
    int32_t layer;

    ///
    /// Wie weit er abgelaufen ist, 0 bis 1.
    ///
    float progress;
} RAD_IsoEffect_t;

///
/// Zeichnet den Effekt "effect" mit "renderer" auf ein Feld, dessen Raute bei
/// (screen_x, screen_y) beginnt -- Kamera und Ebene schon eingerechnet, wie bei
/// RAD_RenderIsoEntity. Sein Anker kommt auf die Mitte der Raute. Ohne geladenes
/// Bild wird nichts gezeichnet.
///
void RAD_RenderIsoEffect(SDL_Renderer *renderer, const RAD_IsoEffect_t *effect, int32_t screen_x, int32_t screen_y);

#endif
