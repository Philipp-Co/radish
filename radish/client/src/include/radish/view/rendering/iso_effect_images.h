#ifndef __RAD_VIEW_RENDERING_ISO_EFFECT_IMAGES_H__
#define __RAD_VIEW_RENDERING_ISO_EFFECT_IMAGES_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/view/rendering/iso_facing.h>

///
/// Die Bilder der Effekte (RAD_IsoEffect_t.sprite) -- etwa einer Explosion --,
/// einmal geladen und von allen geteilt, wie die der Einheiten
/// (view/rendering/iso_unit_images.h): main() laedt sie nach dem Renderer vorab.
///
/// **Welche Effekte es gibt, sagt nicht der Code**: jeder Ordner unter
/// assets/effects/ mit einer effect.json ist einer (view/rendering/
/// iso_effect_sprite.h, client/schema/effect.schema.json). Die Dateien kommen
/// aus der Datenbank des Backends (api/domain/effects/); das Frontend legt sie
/// vor main() ins Dateisystem des Moduls. Der Build bringt keine mit.
///
/// **Je Effekt eine Folge je Richtung** (RAD_IsoFacing_t): bei einer Explosion
/// die, in die der Schuss fliegt. Die Einzelbilder liegen in einem Atlas in
/// einem Raster, zeilenweise von links oben, in der Reihenfolge, in der sie
/// laufen; der Effekt spielt einmal durch und ist dann vorbei. Mehrere
/// Richtungen eines Effekts duerfen sich einen Atlas teilen -- er wird nur
/// einmal geladen. Die Spalten ergeben sich aus der Breite des Atlas.
///
/// **Alles in festen Plaetzen**: hoechstens RAD_ISO_EFFECT_SPRITES Effekte, die
/// Ordner nach Namen sortiert -- der Index eines Effekts ist sein Platz.
///

///
/// Ein geladener Effekt, als Platz in der Liste; RAD_ISO_EFFECT_SPRITE_NONE fuer
/// keinen.
///
typedef int32_t RAD_IsoEffectSprite_t;

#define RAD_ISO_EFFECT_SPRITE_NONE ((RAD_IsoEffectSprite_t)-1)

///
/// Wie viele Effekte der Client hoechstens laedt; weitere Ordner bleiben mit
/// einer Meldung liegen.
///
#define RAD_ISO_EFFECT_SPRITES 16

///
/// Eine der geladenen Explosionen, zufaellig (rand()): welche ein Angriff zeigt,
/// sagt das Spiel nicht, jeder Client waehlt selbst.
/// RAD_ISO_EFFECT_SPRITE_NONE, wenn keine geladen ist -- dann explodiert nichts
/// Sichtbares.
///
RAD_IsoEffectSprite_t RAD_IsoEffectRandomExplosion(void);

///
/// Laedt alle Effekte unter /assets/effects/ fuer "renderer". false, wenn eine
/// effect.json ungueltig ist oder ein Bild darin fehlt oder nicht zu seinen
/// Angaben passt -- der Grund steht auf der Konsole; die uebrigen sind trotzdem
/// geladen. Ein Effekt, dem ein Bild fehlt, wird gar nicht geladen.
///
bool RAD_IsoEffectImagesLoad(SDL_Renderer *renderer);

///
/// Gibt alle Bilder frei. Vor dem Renderer: die Texturen haengen an ihm.
///
void RAD_IsoEffectImagesFree(void);

///
/// Wie lange der Effekt "sprite" einmal durchlaeuft, in Millisekunden; 0 fuer
/// einen, der nicht geladen ist.
///
float RAD_IsoEffectDurationMs(RAD_IsoEffectSprite_t sprite);

///
/// Der Atlas von "sprite" fuer Richtung "facing", oder NULL, wenn er nicht
/// geladen ist oder eine der beiden Angaben ausserhalb ihres Bereichs liegt.
///
SDL_Texture* RAD_IsoEffectAtlasTexture(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing);

///
/// Das Einzelbild von "sprite" in Richtung "facing" zum Anteil "progress" am
/// Ablauf (0 bis 1, darueber hinaus begrenzt), gezaehlt ab dem ersten der
/// Folge. -1, wenn der Effekt nicht geladen ist oder "facing" ausserhalb liegt.
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
/// (x, y) liegt. false, wenn der Effekt nicht geladen ist oder "facing"
/// ausserhalb liegt; dann bleibt "target" unberuehrt.
///
bool RAD_IsoEffectFrameTarget(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, int32_t x, int32_t y,
                              SDL_Rect *target);

#endif
