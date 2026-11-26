#ifndef __RAD_VIEW_RENDERING_ISO_EFFECT_H__
#define __RAD_VIEW_RENDERING_ISO_EFFECT_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/view/rendering/iso_effect_images.h>
#include <radish/view/rendering/iso_facing.h>

///
/// Was die Darstellung von einem Effekt weiss -- etwa einer Explosion auf einem
/// Feld. Er liegt auf keinem Feld, sondern steht in der Zeichenliste der
/// Iso-Map, ueber allem anderen.
///
/// **Wem er gehoert**: der Iso-Map (RAD_IsoMapStartEffect). Wie weit er ist,
/// rechnet er selbst aus der Zeit (RAD_IsoEffectUpdate) -- wie eine Figur
/// (view/rendering/entity.h).
///
typedef struct
{
    ///
    /// Wie er aussieht (view/rendering/iso_effect_images.h), und welcher Atlas:
    /// bei einer Explosion die Richtung, aus der der Schuss kommt.
    ///
    RAD_IsoEffectSprite_t sprite;
    RAD_IsoFacing_t facing;

    ///
    /// Wo sein Anker liegt, in Feldern.
    ///
    float x;
    float y;

    ///
    /// Wie weit er abgelaufen ist, 0 bis 1 (RAD_IsoEffectUpdate).
    ///
    float progress;

    ///
    /// Ob der Platz belegt ist, und wann der Effekt beginnt. Bis dahin wartet
    /// er: ein Einschlag faellt mit dem Schuss, nicht mit dem Befehl dazu. Er
    /// wird erst gezeichnet, wenn er laeuft.
    ///
    bool active;
    float start_ms;
} RAD_IsoEffect_t;

///
/// Setzt den Effekt auf (x, y) und laesst ihn ab "start_ms" laufen.
///
void RAD_IsoEffectStart(RAD_IsoEffect_t *effect, RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing,
                        float x, float y, float start_ms);

///
/// Bringt den Effekt auf "now_ms". false, wenn er damit durch ist -- danach ist
/// sein Platz wieder frei.
///
bool RAD_IsoEffectUpdate(RAD_IsoEffect_t *effect, float now_ms);

///
/// Ob der Effekt zu "now_ms" schon laeuft -- nur dann wird er gezeichnet.
///
bool RAD_IsoEffectHasStarted(const RAD_IsoEffect_t *effect, float now_ms);

///
/// Zeichnet den Effekt "effect" mit "renderer" auf ein Feld, dessen Raute bei
/// (screen_x, screen_y) beginnt -- die Kamera schon eingerechnet, wie bei
/// RAD_RenderIsoEntity. Sein Anker kommt auf die Mitte der Raute. Ohne geladenes
/// Bild wird nichts gezeichnet.
///
void RAD_RenderIsoEffect(SDL_Renderer *renderer, const RAD_IsoEffect_t *effect, int32_t screen_x, int32_t screen_y);

#endif
