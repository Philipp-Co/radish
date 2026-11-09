#ifndef __RAD_RENDERING_ISO_UNIT_IMAGES_H__
#define __RAD_RENDERING_ISO_UNIT_IMAGES_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/rendering/iso_facing.h>
#include <radish/rendering/iso_unit_sprite.h>

///
/// Die Bilder der Einheiten (RAD_IsoEntity_t.sprite), von allen Figuren mit
/// demselben Bild geteilt.
///
/// **Welche Einheit welches Bild hat, sagt ihr Bild-Schluessel**
/// (RAD_ClientUnit_t.sprite, UnitType.sprite im Katalog) -- nicht ihr Typname,
/// der sich aendern darf. Der Schluessel ist ein Ordner unter assets/units/; was
/// darin liegt und wie es aufgeteilt ist, beschreibt seine sprite.json
/// (rendering/iso_unit_sprite.h, client/schema/sprite.schema.json). Ein neues
/// Bild braucht also nur einen Ordner mit PNGs und sprite.json, keinen Code.
///
/// **Geladen wird bei Bedarf**: das erste RAD_IsoUnitSpriteOf mit einem
/// Schluessel liest dessen sprite.json und laedt die Atlanten; jedes weitere
/// bekommt dasselbe Bild. Ein Bild, das nicht zu laden war, bleibt bis
/// RAD_IsoUnitImagesFree ein Rechteck und wird nicht bei jeder Figur neu
/// versucht -- der Grund steht einmal auf der Konsole.
///
/// **Alles in festen Plaetzen, nichts auf dem Heap**: hoechstens
/// RAD_ISO_UNIT_SPRITES_MAX Bilder, je Bild genau RAD_ISO_UNIT_ATLASES Atlanten,
/// ein statischer Lesepuffer fuer die sprite.json. Ein Bild mehr wird als
/// Rechteck gezeichnet. Die Texturen selbst legt SDL an.
///
/// **Je Bild alle Animationen** (RAD_IsoUnitAnimation_t): die Drehung
/// (RAD_ISO_UNIT_ATLAS_TURN), deren vier Schluesselbilder zugleich die Standbilder
/// sind (RAD_IsoUnitFacingFrame, RAD_IsoUnitTurnFrame), das Zielen je
/// Blickrichtung (RAD_IsoUnitAimAtlas, RAD_IsoUnitAimFrame) und die Folgen fuer
/// Ziehen, Schiessen und Zerstoertwerden (RAD_IsoUnitSequenceFrameOf). Alle
/// Atlanten eines Bildes haben Zellen im selben Seitenverhaeltnis und die Figur
/// an derselben Stelle darin.
///
/// Die Dateien legt das Frontend vor main() ins Dateisystem des Moduls
/// (client/publish_assets.cmake).
///

///
/// Ein Bild: seine Stelle unter den geladenen. RAD_ISO_UNIT_SPRITE_NONE fuer
/// keines -- die Figur wird als Rechteck gezeichnet.
///
typedef int32_t RAD_IsoUnitSprite_t;
#define RAD_ISO_UNIT_SPRITE_NONE (-1)

///
/// Wie viele verschiedene Bilder gleichzeitig geladen sein koennen. Ein Spiel hat
/// zwei Armeen aus wenigen Einheitentypen; 32 laesst viel Luft.
///
#define RAD_ISO_UNIT_SPRITES_MAX 32

///
/// Setzt den Renderer, an dem die Texturen haengen. Vor dem ersten
/// RAD_IsoUnitSpriteOf; vorher liefert es fuer jeden Schluessel
/// RAD_ISO_UNIT_SPRITE_NONE, ohne sich das als Fehler zu merken.
///
void RAD_IsoUnitImagesInit(SDL_Renderer *renderer);

///
/// Gibt alle Bilder frei und vergisst auch die, die nicht zu laden waren. Vor dem
/// Renderer: die Texturen haengen an ihm. Danach gilt keine Stelle mehr, die
/// RAD_IsoUnitSpriteOf geliefert hat.
///
void RAD_IsoUnitImagesFree(void);

///
/// Das Bild zum Schluessel "key" (RAD_ClientUnit_t.sprite), beim ersten Mal
/// geladen. RAD_ISO_UNIT_SPRITE_NONE fuer einen leeren Schluessel oder NULL --
/// ohne Meldung, leer heisst "noch kein Bild" --, und fuer einen, dessen Bild
/// fehlt oder nicht zu seiner sprite.json passt, mit einer Meldung beim ersten Mal.
///
RAD_IsoUnitSprite_t RAD_IsoUnitSpriteOf(const char *key);

///
/// Die Zeiten, wenn ein Bild keine eigenen nennt (sprite.json "timing_ms") --
/// auch fuer eine Figur ohne Bild.
///
#define RAD_ISO_UNIT_STEP_MS_DEFAULT 150.0f
#define RAD_ISO_UNIT_TURN_MS_DEFAULT 1000.0f
#define RAD_ISO_UNIT_AIM_MS_DEFAULT 500.0f
#define RAD_ISO_UNIT_AIM_HOLD_MS_DEFAULT 500.0f

///
/// Wie lange eine Figur mit dem Bild "sprite" fuer einen Schritt von Feld zu Feld
/// braucht, in Millisekunden (view/iso_map_view.c): kleiner ist schneller. Nur
/// die Darstellung, wie weit sie ziehen darf, sagt ihr Typ (movement).
///
float RAD_IsoUnitStepMs(RAD_IsoUnitSprite_t sprite);

///
/// Wie lange sie fuer eine Vierteldrehung braucht, von einer Blickrichtung zur
/// naechsten; eine Kehrtwende dauert doppelt so lange.
///
float RAD_IsoUnitTurnMs(RAD_IsoUnitSprite_t sprite);

///
/// Wie lange sie beim Zielen fuer eine Vierteldrehung braucht; weniger weit
/// dauert anteilig kuerzer.
///
float RAD_IsoUnitAimMs(RAD_IsoUnitSprite_t sprite);

///
/// Wie lange sie nach einem Angriff auf das Ziel gerichtet bleibt, bevor sie
/// wieder nach vorn zielt.
///
float RAD_IsoUnitAimHoldMs(RAD_IsoUnitSprite_t sprite);

///
/// Der Atlas des Zielens bei Blickrichtung "facing"; -1 ausserhalb der
/// Aufzaehlung.
///
RAD_IsoUnitAtlas_t RAD_IsoUnitAimAtlas(RAD_IsoFacing_t facing);

///
/// Der Atlas "atlas" des Bildes "sprite", oder NULL, wenn das Bild nicht geladen
/// ist oder der Atlas ausserhalb [0, RAD_ISO_UNIT_ATLASES) liegt.
///
SDL_Texture* RAD_IsoUnitAtlasTexture(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAtlas_t atlas);

///
/// Ob "sprite" zielen kann, ohne sich zu drehen. Jedes geladene Bild kann es --
/// jede Figur bietet alle Animationen an --, eines, das nicht geladen ist, nicht.
///
bool RAD_IsoUnitCanAim(RAD_IsoUnitSprite_t sprite);

///
/// Das Einzelbild im Atlas der Drehung, das die Figur in Blickrichtung "facing"
/// stehend zeigt; -1, wenn das Bild nicht geladen ist oder "facing" ausserhalb
/// der Aufzaehlung liegt.
///
int32_t RAD_IsoUnitFacingFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t facing);

///
/// Das Einzelbild im Atlas der Drehung fuer eine Drehung von "from" nach "to",
/// zum Anteil "progress" (0 bis 1, darueber hinaus begrenzt) -- auf dem kuerzeren
/// Weg (RAD_IsoFacingQuarterTurns), gegen die Reihenfolge des Atlas rueckwaerts
/// durch ihn hindurch. Bei 0 das Bild von "from", bei 1 das von "to"; bei
/// gleicher Richtung immer das von "from". -1 wie oben.
///
int32_t RAD_IsoUnitTurnFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t from, RAD_IsoFacing_t to, float progress);

///
/// Das Einzelbild im Atlas des Zielens bei Blickrichtung "facing"
/// (RAD_IsoUnitAimAtlas), das um "angle" Grad gegen die Blickrichtung zielt --
/// 0 nach vorn, gezaehlt wie in rendering/iso_facing.h, beliebig gross oder
/// negativ. -1 wie oben.
///
int32_t RAD_IsoUnitAimFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t facing, float angle);

///
/// Fuer eine Folge (MOVE, FIRE, DESTROYED) bei Blickrichtung "facing": ihr Atlas
/// nach "atlas" und das Bild, das "elapsed_ms" nach ihrem Beginn zu sehen ist --
/// MOVE in Schleife, die anderen bis zum letzten Bild und dort stehend
/// (RAD_IsoUnitAnimationLoops). -1, wenn das Bild nicht geladen ist oder
/// "animation" keine Folge ist; "atlas" bleibt dann unberuehrt.
///
int32_t RAD_IsoUnitSequenceFrameOf(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAnimation_t animation, RAD_IsoFacing_t facing,
                                   float elapsed_ms, RAD_IsoUnitAtlas_t *atlas);

///
/// Wie lange die Folge "animation" bei "facing" einmal durchlaeuft, in
/// Millisekunden; 0, wenn das Bild nicht geladen ist oder es keine Folge ist.
///
float RAD_IsoUnitSequenceMsOf(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAnimation_t animation, RAD_IsoFacing_t facing);

///
/// Wo das Einzelbild "frame" im Atlas "atlas" von "sprite" liegt, nach
/// "source". false, wenn der Atlas nicht geladen ist oder "frame" nicht in ihm
/// liegt; dann bleibt "source" unberuehrt.
///
bool RAD_IsoUnitFrameSource(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAtlas_t atlas, int32_t frame, SDL_Rect *source);

///
/// Wohin ein Einzelbild von "sprite" auf dem Bildschirm kommt, nach "target":
/// so gross, wie die Figur gezeichnet wird, und so verschoben, dass der Punkt,
/// auf dem sie steht -- die Mitte ihrer Grundflaeche, nicht die ihrer Umrisse
/// --, bei (x, y) liegt. Fuer alle Einzelbilder aller Atlanten gleich. false,
/// wenn das Bild nicht geladen ist; dann bleibt "target" unberuehrt.
///
bool RAD_IsoUnitFrameTarget(RAD_IsoUnitSprite_t sprite, int32_t x, int32_t y, SDL_Rect *target);

///
/// Ob die Bilder von "sprite" ihren Schatten schon mitbringen. Sonst zeichnet
/// rendering/entity.c einen darunter. false, wenn das Bild nicht geladen ist.
///
bool RAD_IsoUnitHasOwnShadow(RAD_IsoUnitSprite_t sprite);

#endif
