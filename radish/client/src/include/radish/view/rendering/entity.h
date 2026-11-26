#ifndef __RAD_ISO_ENTITY_H__
#define __RAD_ISO_ENTITY_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/io/net_types.h>
#include <radish/view/rendering/iso_unit_images.h>
#include <radish/view/rendering/animations/unit_animation.h>

///
/// Was die Darstellung von einer Figur weiss.
///
/// **Die Id sagt, welche Figur das hier ist.** Ohne sie waere ein Iso-Objekt nur
/// etwas, das auf einem Feld liegt, und wiederzufinden allein ueber dieses Feld --
/// also nur so lange, wie es stimmt.
///
/// **Fuers Aufraeumen**: ob auf einem Feld noch dieselbe Figur liegt, ist eine
/// Frage, die nur die Id beantwortet.
///
/// **Wem sie gehoert**: immer der Iso-Map -- sie legt sie an und gibt sie frei
/// (view/rendering/iso_map.h), auch waehrend sie zieht. Wer sie bewegt, schreibt nur
/// ihre Werte.
///
/// Das Feld selbst (RAD_IsoObject_t) kennt sie nicht: gezeichnet wird sie als
/// eigener Eintrag der Zeichenliste.
///
///
/// Was eine Figur gerade abspielt. IDLE heisst: sie steht auf ihrem Feld, und
/// die Iso-Map bestimmt, wo das ist (RAD_IsoMapApplyTile).
///
/// Waehrend MOVING und WRECK gehoert ihr Platz ihr selbst
/// (RAD_IsoEntityHoldsPosition): sie steht zwischen zwei Feldern oder brennt
/// noch, nachdem die Welt ihre Einheit schon vergessen hat. AIMING dreht nur den
/// Turm; dabei bleibt sie, wo sie ist.
///
typedef enum
{
    RAD_ISO_ENTITY_IDLE = 0,
    RAD_ISO_ENTITY_MOVING,
    RAD_ISO_ENTITY_AIMING,
    RAD_ISO_ENTITY_WRECK
} RAD_IsoEntityActivity_t;

typedef struct
{
    RAD_NetEntityId_t id;

    ///
    /// Wo die Figur steht, in Feldern -- Nachkommastellen erlaubt. Danach
    /// zeichnet und sortiert sie die Iso-Map, und danach findet
    /// RAD_IsoMapEntityAt sie. Eine ruhende Figur steht auf ganzen Zahlen.
    ///
    float x;
    float y;

    ///
    /// Was die Figur gerade abspielt, und der Plan dazu
    /// (RAD_IsoEntityMoveAlong, RAD_IsoEntityAim, RAD_IsoEntityDestroy). Bei
    /// RAD_ISO_ENTITY_IDLE ist "animation" NULL und alles darunter ungenutzt.
    ///
    /// "start_ms" ist der Beginn -- eines Zuges, oder der Einschlag, der die
    /// Figur zerstoert hat; "end_ms" das Ende eines Wracks; (to_x, to_y) das
    /// Zielfeld eines Zuges.
    ///
    RAD_IsoEntityActivity_t activity;
    RAD_UnitAnimation_t *animation;
    float start_ms;
    float end_ms;
    int32_t to_x;
    int32_t to_y;

    ///
    /// Wie die Figur aussieht: das Bild zu ihrem Bild-Schluessel
    /// (RAD_IsoUnitSpriteOf), RAD_ISO_UNIT_SPRITE_NONE fuer das Rechteck. Gesetzt
    /// von der Iso-Map, sobald eine Einheit auf das Feld kommt.
    ///
    RAD_IsoUnitSprite_t sprite;

    ///
    /// Wem sie gehoert, als Stelle unter den Mitspielern
    /// (RAD_ClientWorldPlayerIndex): 0 zeichnet einen blauen Ring unter ihr, 1
    /// einen gelben, RAD_ISO_ENTITY_PLAYER_NONE keinen. Gesetzt von der Iso-Map
    /// mit dem Feld (RAD_IsoMapApplyTile).
    ///
    int32_t player;

    ///
    /// Wohin sie schaut. Nur der Client weiss davon, das Spiel kennt keine
    /// Blickrichtung: gesetzt beim Aufstellen und bei jedem Schritt eines Zuges
    /// (RAD_IsoEntityUpdate). "facing_set" ist false, bis jemand sie gesetzt hat.
    ///
    RAD_IsoFacing_t facing;
    bool facing_set;

    ///
    /// Ob sie sich gerade dreht, von "facing" nach "turn_to", und wie weit sie
    /// damit ist (0 bis 1). Gezeichnet wird dann das Einzelbild dazwischen
    /// (RAD_IsoUnitTurnFrame), sonst das ihrer Blickrichtung. Gesetzt von der
    /// Animation eines Zuges (RAD_IsoEntityUpdate); am Ende einer Drehung steht
    /// "turning" wieder auf false und "facing" auf "turn_to".
    ///
    bool turning;
    RAD_IsoFacing_t turn_to;
    float turn_progress;

    ///
    /// Wie weit ihr Turm gegen die Wanne gedreht ist, in Grad, gezaehlt wie in
    /// rendering/iso_facing.h; 0 nach vorn. Gesetzt von der Animation, wenn sie
    /// zielt (RAD_IsoEntityUpdate). Waehrend einer Drehung der Figur ist er 0 --
    /// dort zeigt der Atlas den Turm nur nach vorn.
    ///
    float aim_angle;

    ///
    /// Welche Folge ihres Bildes gerade laeuft (RAD_IsoUnitSequenceFrameOf) --
    /// MOVE, FIRE oder DESTROYED -- und seit wie vielen Millisekunden; ohne
    /// "sequence_active" keine. Gesetzt je Frame von RAD_IsoEntityUpdate.
    /// Eine Drehung geht vor; FIRE zeigt die Figur nur,
    /// solange sie geradeaus zielt ("aim_angle" 0), denn die Bilder einer Folge
    /// kennen nur die Blickrichtung -- sonst bleibt das Bild des Zielens.
    ///
    bool sequence_active;
    RAD_IsoUnitAnimation_t sequence;
    float sequence_ms;
} RAD_IsoEntity_t;

/// RAD_IsoEntity_t.player fuer eine Figur ohne bekannten Besitzer.
#define RAD_ISO_ENTITY_PLAYER_NONE (-1)


///
/// Laesst die Figur den Weg (xs, ys) ablaufen, ab "now_ms", und auf (to_x, to_y)
/// enden. Gefahren wird geradeaus, gedreht auf der Stelle
/// (view/rendering/animations/unit_animation.h); ihr Bild sagt, wie schnell
/// (RAD_IsoUnitStepMs). Ein laufender Zug wird ersetzt, ein Zielen abgebrochen:
/// der Turm laeuft zu Beginn nach vorn zurueck.
///
/// false, wenn kein Speicher da ist; dann steht die Figur still auf (to_x, to_y),
/// als waere der Zug schon abgelaufen.
///
bool RAD_IsoEntityMoveAlong(RAD_IsoEntity_t *entity, const float *xs, const float *ys, size_t count,
                            int32_t to_x, int32_t to_y, float now_ms);

///
/// Richtet den Turm auf Feld (target_x, target_y) -- ohne "has_target" nach vorn
/// zurueck. Mit "back" bleibt er "hold_ms" auf dem Ziel und laeuft dann zurueck;
/// dann feuert die Figur, und nach "fire_ms" kommt, wann. Zieht sie gerade, haengt
/// das Zielen an ihren Zug an und rechnet von dessen Ende aus.
///
/// true nur, wenn sie feuert; sonst bleibt "fire_ms" unberuehrt. Ohne drehbaren
/// Turm (RAD_IsoUnitCanAim), auf das eigene Feld oder ohne Speicher geschieht
/// nichts.
///
bool RAD_IsoEntityAim(RAD_IsoEntity_t *entity, bool has_target, int32_t target_x, int32_t target_y,
                      bool back, float hold_ms, float now_ms, float *fire_ms);

///
/// Laesst die Figur ab "impact_ms" ihre Zerstoerung zeigen und bis "end_ms"
/// stehen bleiben. Ein laufender Zug oder ein Zielen endet damit; die Figur
/// bleibt, wo sie gerade ist.
///
void RAD_IsoEntityDestroy(RAD_IsoEntity_t *entity, float impact_ms, float end_ms);

///
/// Bringt die Figur auf "now_ms": Position, Blickrichtung, Turm und die Folge
/// ihres Bildes. Zurueck kommt, was dabei zu Ende gegangen ist, sonst
/// RAD_ISO_ENTITY_IDLE -- danach ist die Figur wieder untaetig.
///
/// Nach einem Zug steht sie am Ende ihres Weges; wohin sie wirklich gehoert,
/// weiss nur die Welt (view/iso_map_view.c). Ein Wrack, das zu Ende ist, gehoert
/// aus der Map genommen.
///
RAD_IsoEntityActivity_t RAD_IsoEntityUpdate(RAD_IsoEntity_t *entity, float now_ms);

///
/// Ob die Figur ihren Platz selbst bestimmt -- waehrend eines Zuges und als
/// Wrack. Dann setzt die Iso-Map sie nicht um und nimmt sie nicht mit ihrem Feld
/// (RAD_IsoMapApplyTile).
///
bool RAD_IsoEntityHoldsPosition(const RAD_IsoEntity_t *entity);

///
/// Ob auf die Figur zu warten ist, bevor die naechste Nachricht vom Server
/// drankommt (RAD_IsoMapViewIsBusy): ein Zug, ein Schuss, eine Zerstoerung. Ein
/// Zielen ohne Schuss ist die Vorschau aus dem Kontextmenue und zaehlt nicht.
///
bool RAD_IsoEntityIsBusy(const RAD_IsoEntity_t *entity);

///
/// Wo die Figur steht oder, waehrend eines Zuges, stehen wird -- immer ein Feld.
///
void RAD_IsoEntityRestingPlace(const RAD_IsoEntity_t *entity, int32_t *x, int32_t *y);

///
/// Zeichnet die Figur "entity" mit "renderer" auf ein Feld, dessen Raute bei
/// (screen_x, screen_y) beginnt -- in Bildschirmkoordinaten, die Kamera schon
/// eingerechnet, wie RAD_RenderIsoObject sie bestimmt. Fuer eine Figur in
/// Bewegung duerfen das Koordinaten zwischen zwei Feldern sein.
///
/// Mit Bild (entity->sprite) das Einzelbild ihrer Blickrichtung aus dem Atlas
/// der Drehung, waehrend einer Drehung das dazwischen, waehrend einer Folge
/// (entity->sequence) deren Bild, beim Zielen das aus dessen Atlas
/// (view/rendering/iso_unit_images.h), so gesetzt, dass die Figur
/// auf der Mitte der Raute steht; darunter ein weicher, flacher Schatten, wenn das Bild
/// keinen eigenen mitbringt. Unter allem ein Ring in der Farbe ihres Besitzers
/// (entity->player), flach auf der Raute -- auch unter dem Rechteck. Ohne Bild, oder solange es nicht
/// geladen ist, ein gruenes Rechteck, so breit und hoch wie ein Feld, das eine
/// halbe Feldhoehe ueber der Raute beginnt.
///
void RAD_RenderIsoEntity(SDL_Renderer *renderer, const RAD_IsoEntity_t *entity, int32_t screen_x, int32_t screen_y);


#endif
