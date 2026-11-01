#ifndef __RAD_RENDERING_ISO_EFFECT_IMAGES_H__
#define __RAD_RENDERING_ISO_EFFECT_IMAGES_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/rendering/iso_facing.h>

///
/// Die Bilder der Effekte (RAD_IsoEffect_t.sprite) -- etwa einer Explosion --,
/// einmal geladen und von allen geteilt, wie die der Einheiten
/// (rendering/iso_unit_images.h): main() laedt sie nach dem Renderer vorab.
///
/// **Je Effekt eine Folge je Richtung** (RAD_IsoFacing_t): bei einer Explosion
/// die, in die der Schuss fliegt. Die Einzelbilder liegen in einem Atlas in
/// einem Raster, zeilenweise von links oben, in der Reihenfolge, in der sie
/// laufen; der Effekt spielt einmal durch und ist dann vorbei. Mehrere
/// Richtungen duerfen sich einen Atlas teilen -- nacheinander darin oder, bei
/// einem Effekt, der in alle Richtungen gleich aussieht, dieselbe Folge.
/// Ein Atlas wird nur einmal geladen.
///
/// Atlas, erste Zelle und Zahl der Bilder je Richtung, Dauer, Anker und Groesse
/// auf dem Bildschirm stehen je Effekt in iso_effect_images.c; die Spalten
/// ergeben sich aus der Breite des Atlas.
///
/// Die Dateien liegen unter assets/effects/; das Frontend legt sie vor main() ins
/// Dateisystem des Moduls (client/publish_assets.cmake).
///
typedef enum
{
    RAD_ISO_EFFECT_SPRITE_NONE = -1,
    RAD_ISO_EFFECT_SPRITE_EXPLOSION_0 = 0,
    RAD_ISO_EFFECT_SPRITE_EXPLOSION_1,
    RAD_ISO_EFFECT_SPRITE_EXPLOSION_2,
    RAD_ISO_EFFECT_SPRITES
} RAD_IsoEffectSprite_t;

///
/// Eine der Explosionen, zufaellig (rand()): welche ein Angriff zeigt, sagt
/// das Spiel nicht, jeder Client waehlt selbst.
///
RAD_IsoEffectSprite_t RAD_IsoEffectRandomExplosion(void);

///
/// Laedt alle Bilder fuer "renderer". false, wenn eines fehlt oder nicht zu
/// seinen Angaben passt; die uebrigen sind trotzdem geladen, ein Effekt ohne
/// sein Bild wird nicht gezeichnet.
///
bool RAD_IsoEffectImagesLoad(SDL_Renderer *renderer);

///
/// Gibt alle Bilder frei. Vor dem Renderer: die Texturen haengen an ihm.
///
void RAD_IsoEffectImagesFree(void);

///
/// Wie lange der Effekt "sprite" einmal durchlaeuft, in Millisekunden; 0
/// ausserhalb der Aufzaehlung.
///
float RAD_IsoEffectDurationMs(RAD_IsoEffectSprite_t sprite);

///
/// Der Atlas von "sprite" fuer Richtung "facing", oder NULL, wenn er nicht
/// geladen ist oder eine der beiden Angaben ausserhalb ihrer Aufzaehlung liegt.
///
SDL_Texture* RAD_IsoEffectAtlasTexture(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing);

///
/// Das Einzelbild von "sprite" in Richtung "facing" zum Anteil "progress" am
/// Ablauf (0 bis 1, darueber hinaus begrenzt), gezaehlt ab dem ersten der
/// Folge. -1, wenn eine der Angaben ausserhalb ihrer Aufzaehlung liegt.
///
int32_t RAD_IsoEffectFrameAt(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, float progress);

///
/// Wo das Einzelbild "frame" der Folge von "sprite" fuer "facing"
/// (RAD_IsoEffectFrameAt) im Atlas liegt, nach "source". false, wenn der Atlas
/// nicht geladen ist oder "frame" nicht in der Folge liegt; dann bleibt
/// "source" unberuehrt.
///
bool RAD_IsoEffectFrameSource(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, int32_t frame, SDL_Rect *source);

///
/// Wohin ein Einzelbild von "sprite" in Richtung "facing" auf dem Bildschirm
/// kommt, nach "target": so gross, wie der Effekt gezeichnet wird, und so
/// verschoben, dass sein Anker -- bei einer Explosion der Einschlag -- bei
/// (x, y) liegt. false, wenn eine der Angaben ausserhalb ihrer Aufzaehlung
/// liegt; dann bleibt "target" unberuehrt.
///
bool RAD_IsoEffectFrameTarget(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, int32_t x, int32_t y,
                              SDL_Rect *target);

#endif
