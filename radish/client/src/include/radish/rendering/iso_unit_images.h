#ifndef __RAD_RENDERING_ISO_UNIT_IMAGES_H__
#define __RAD_RENDERING_ISO_UNIT_IMAGES_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/rendering/iso_facing.h>

///
/// Die Bilder der Einheiten (RAD_IsoEntity_t.sprite), einmal geladen und von
/// allen Figuren geteilt -- wie die Bilder der Felder
/// (rendering/iso_tile_images.h): main() laedt sie nach dem Renderer vorab.
///
/// **Je Einheit mehrere Atlanten** (RAD_IsoUnitAtlas_t): Bilder, in denen die
/// Einzelbilder einer vollen Drehung in einem Raster liegen, zeilenweise von
/// links oben, im Drehsinn von RAD_IsoFacing_t; nach dem letzten kommt wieder
/// das erste.
///
///   - Der Atlas der Drehung (RAD_ISO_UNIT_ATLAS_TURN) dreht die ganze Figur,
///     der Turm zeigt nach vorn. Vier seiner Bilder sind die Blickrichtungen,
///     in denen sie steht (RAD_IsoUnitFacingFrame); die dazwischen zeigt sie
///     nur, waehrend sie sich dreht (RAD_IsoUnitTurnFrame).
///   - Je Blickrichtung ein Atlas des Turms (RAD_IsoUnitTurretAtlas): die Wanne
///     steht, der Turm dreht sich einmal herum, beginnend nach vorn
///     (RAD_IsoUnitTurretFrame). Eine Figur ohne sie hat keinen drehbaren Turm
///     (RAD_IsoUnitHasTurret).
///
/// In jedem Atlas stehen die Bilder bei 0, 90, 180 und 270 Grad fest
/// (rendering/iso_facing.h), dazwischen wird linear gezaehlt -- die Abstaende
/// muessen nicht gleich sein. Groesse der Zellen, Zahl der Bilder und diese
/// vier stehen je Einheit und Atlas in iso_unit_images.c; die Spalten ergeben
/// sich aus der Breite des Atlas, die Groesse auf dem Bildschirm aus einer
/// eigenen Breite -- ein Atlas in anderer Aufloesung braucht dort nur andere
/// Zahlen. Alle Atlanten einer Einheit haben Zellen im selben Seitenverhaeltnis
/// und die Figur an derselben Stelle darin.
///
/// Die Dateien liegen unter assets/units/; das Frontend legt sie vor main() ins
/// Dateisystem des Moduls (client/publish_assets.cmake).
///
/// **Welche Einheit welches Bild hat, sagt ihr Typname** (RAD_IsoUnitSpriteOf).
/// Der Katalog kennt noch kein Feld dafuer; bis dahin steht die Zuordnung hier.
///
typedef enum
{
    RAD_ISO_UNIT_SPRITE_NONE = -1,
    RAD_ISO_UNIT_SPRITE_HEAVY_TANK = 0,
    RAD_ISO_UNIT_SPRITES
} RAD_IsoUnitSprite_t;

///
/// Das Bild fuer Einheiten vom Typ "type_name" (RAD_ClientUnit_t.name), ohne
/// Ruecksicht auf Gross- und Kleinschreibung; RAD_ISO_UNIT_SPRITE_NONE fuer einen
/// Typ ohne Bild oder NULL.
///
RAD_IsoUnitSprite_t RAD_IsoUnitSpriteOf(const char *type_name);

///
/// Wie lange ein Schritt von Feld zu Feld dauert, wenn eine Figur ohne eigenen
/// Wert zieht -- auch eine ohne Bild (RAD_ISO_UNIT_SPRITE_NONE).
///
#define RAD_ISO_UNIT_STEP_MS_DEFAULT 150.0f

///
/// Wie lange eine Figur mit dem Bild "sprite" fuer einen Schritt von Feld zu Feld
/// braucht, in Millisekunden (view/iso_map_view.c): kleiner ist schneller. Nur
/// die Darstellung, wie weit sie ziehen darf, sagt ihr Typ (movement).
/// RAD_ISO_UNIT_STEP_MS_DEFAULT fuer ein Bild ohne eigenen Wert oder ausserhalb
/// der Aufzaehlung.
///
float RAD_IsoUnitStepMs(RAD_IsoUnitSprite_t sprite);

///
/// Wie lange eine Vierteldrehung dauert, wenn eine Figur ohne eigenen Wert
/// sich dreht -- auch eine ohne Bild.
///
#define RAD_ISO_UNIT_TURN_MS_DEFAULT 1000.0f

///
/// Wie lange eine Figur mit dem Bild "sprite" fuer eine Vierteldrehung braucht,
/// von einer Blickrichtung zur naechsten, in Millisekunden; eine Kehrtwende
/// dauert doppelt so lange. RAD_ISO_UNIT_TURN_MS_DEFAULT fuer ein Bild ohne
/// eigenen Wert oder ausserhalb der Aufzaehlung.
///
float RAD_IsoUnitTurnMs(RAD_IsoUnitSprite_t sprite);

///
/// Wie lange der Turm fuer eine Vierteldrehung braucht, und wie lange er nach
/// einem Angriff auf das Ziel gerichtet bleibt, bevor er zuruecklaeuft, wenn
/// eine Figur ohne eigene Werte zielt.
///
#define RAD_ISO_UNIT_TURRET_MS_DEFAULT 500.0f
#define RAD_ISO_UNIT_FIRE_HOLD_MS_DEFAULT 500.0f

///
/// Wie lange der Turm einer Figur mit dem Bild "sprite" fuer eine
/// Vierteldrehung braucht, in Millisekunden; weniger weit dauert anteilig
/// kuerzer. RAD_ISO_UNIT_TURRET_MS_DEFAULT fuer ein Bild ohne eigenen Wert oder
/// ausserhalb der Aufzaehlung.
///
float RAD_IsoUnitTurretMs(RAD_IsoUnitSprite_t sprite);

///
/// Wie lange der Turm einer Figur mit dem Bild "sprite" nach einem Angriff auf
/// das Ziel gerichtet bleibt, bevor er nach vorn zuruecklaeuft, in
/// Millisekunden. RAD_ISO_UNIT_FIRE_HOLD_MS_DEFAULT fuer ein Bild ohne eigenen
/// Wert oder ausserhalb der Aufzaehlung.
///
float RAD_IsoUnitFireHoldMs(RAD_IsoUnitSprite_t sprite);

///
/// Die Atlanten einer Einheit: der der Drehung und je Blickrichtung einer des
/// Turms, in der Reihenfolge von RAD_IsoFacing_t.
///
typedef enum
{
    RAD_ISO_UNIT_ATLAS_TURN = 0,
    RAD_ISO_UNIT_ATLAS_TURRET_X_MINUS,
    RAD_ISO_UNIT_ATLAS_TURRET_Y_PLUS,
    RAD_ISO_UNIT_ATLAS_TURRET_X_PLUS,
    RAD_ISO_UNIT_ATLAS_TURRET_Y_MINUS,
    RAD_ISO_UNIT_ATLASES
} RAD_IsoUnitAtlas_t;

///
/// Der Atlas des Turms fuer eine Wanne in Blickrichtung "facing";
/// RAD_ISO_UNIT_ATLASES ausserhalb der Aufzaehlung.
///
RAD_IsoUnitAtlas_t RAD_IsoUnitTurretAtlas(RAD_IsoFacing_t facing);

///
/// Laedt alle Bilder fuer "renderer". false, wenn eines fehlt oder nicht zu
/// seinen Angaben passt; die uebrigen sind trotzdem geladen, und eine Figur ohne
/// ihr Bild wird als Rechteck gezeichnet.
///
bool RAD_IsoUnitImagesLoad(SDL_Renderer *renderer);

///
/// Gibt alle Bilder frei. Vor dem Renderer: die Texturen haengen an ihm.
///
void RAD_IsoUnitImagesFree(void);

///
/// Der Atlas "atlas" des Bildes "sprite", oder NULL, wenn er nicht geladen ist
/// oder einer der beiden ausserhalb seiner Aufzaehlung liegt.
///
SDL_Texture* RAD_IsoUnitAtlasTexture(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAtlas_t atlas);

///
/// Ob "sprite" einen drehbaren Turm hat: alle vier Atlanten des Turms sind
/// geladen. false ausserhalb der Aufzaehlung.
///
bool RAD_IsoUnitHasTurret(RAD_IsoUnitSprite_t sprite);

///
/// Das Einzelbild im Atlas der Drehung von "sprite", das die Figur in
/// Blickrichtung "facing" stehend zeigt; -1, wenn einer der beiden ausserhalb seiner
/// Aufzaehlung liegt.
///
int32_t RAD_IsoUnitFacingFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t facing);

///
/// Das Einzelbild im Atlas der Drehung von "sprite" fuer eine Drehung von
/// "from" nach "to", zum Anteil "progress" (0 bis 1, darueber hinaus begrenzt)
/// -- auf dem kuerzeren Weg (RAD_IsoFacingQuarterTurns), gegen die Reihenfolge
/// des Atlas rueckwaerts durch ihn hindurch. Bei 0 das Bild von "from", bei 1 das von
/// "to"; bei gleicher Richtung immer das von "from". -1, wenn eine der Angaben
/// ausserhalb ihrer Aufzaehlung liegt.
///
int32_t RAD_IsoUnitTurnFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t from, RAD_IsoFacing_t to, float progress);

///
/// Das Einzelbild im Atlas des Turms fuer eine Wanne in Blickrichtung "facing"
/// (RAD_IsoUnitTurretAtlas), dessen Rohr um "angle" Grad gegen die Wanne
/// gedreht ist -- 0 nach vorn, gezaehlt wie in rendering/iso_facing.h,
/// beliebig gross oder negativ. -1, wenn eine der Angaben ausserhalb ihrer
/// Aufzaehlung liegt.
///
int32_t RAD_IsoUnitTurretFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t facing, float angle);

///
/// Wo das Einzelbild "frame" im Atlas "atlas" von "sprite" liegt, nach
/// "source". false, wenn der Atlas nicht geladen ist oder "frame" nicht in ihm
/// liegt; dann bleibt "source" unberuehrt.
///
bool RAD_IsoUnitFrameSource(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAtlas_t atlas, int32_t frame, SDL_Rect *source);

///
/// Wohin ein Einzelbild von "sprite" auf dem Bildschirm kommt, nach "target":
/// so gross, wie die Figur gezeichnet wird, und so verschoben, dass der Punkt,
/// auf dem sie steht -- die Mitte ihrer Grundflaeche, nicht die ihrer Umrisse,
/// der Turm ragt nach oben heraus --, bei (x, y) liegt. Fuer alle Einzelbilder
/// aller Atlanten gleich: in ihnen dreht sich die Figur auf der Stelle. false, wenn "sprite"
/// ausserhalb der Aufzaehlung liegt; dann bleibt "target" unberuehrt.
///
bool RAD_IsoUnitFrameTarget(RAD_IsoUnitSprite_t sprite, int32_t x, int32_t y, SDL_Rect *target);

///
/// Ob die Bilder von "sprite" ihren Schatten schon mitbringen. Sonst zeichnet
/// rendering/entity.c einen darunter. false ausserhalb der Aufzaehlung.
///
bool RAD_IsoUnitHasOwnShadow(RAD_IsoUnitSprite_t sprite);

#endif
