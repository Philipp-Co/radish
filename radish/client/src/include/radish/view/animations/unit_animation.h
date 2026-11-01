#ifndef __RAD_VIEW_ANIMATIONS_UNIT_ANIMATION_H__
#define __RAD_VIEW_ANIMATIONS_UNIT_ANIMATION_H__

#include <stdbool.h>
#include <stddef.h>
#include <radish/rendering/iso_facing.h>

///
/// Was eine Figur nacheinander tut: ein Plan aus Abschnitten -- fahren, sich
/// drehen, den Turm richten.
///
/// **Ein Zug** (RAD_CreateUnitAnimation) geht einen Pfad aus x/y-Koordinaten
/// entlang. **Gefahren wird nur geradeaus, gedreht nur auf der Stelle.** Je
/// Abschnitt des Pfades eine Fahrt (RAD_UNIT_ANIMATION_DRIVE), linear in
/// "step_ms". Zeigt der Abschnitt in eine andere Richtung als die Figur gerade
/// (RAD_IsoFacingOf), dreht sie sich davor auf dem Punkt, an dem sie steht
/// (RAD_UNIT_ANIMATION_TURN): je Vierteldrehung "turn_ms", auf dem kuerzeren
/// Weg (RAD_IsoFacingQuarterTurns). Auch vor dem ersten Abschnitt, wenn sie
/// nicht schon in seine Richtung schaut. Ist ihr Turm gedreht, laeuft er vor
/// allem anderen nach vorn zurueck (RAD_UNIT_ANIMATION_AIM) -- die Bilder der
/// Drehung kennen ihn nur so.
///
///   Pfad (0,0) (1,0) (1,1), Blick X_PLUS, Turm 0, start 0 ms, Schritt 750 ms,
///   Vierteldrehung 1000 ms:
///       0..750 ms     faehrt (0,0) -> (1,0), Blick X_PLUS
///       750..1750 ms  dreht auf (1,0) von X_PLUS nach Y_PLUS
///       1750..2500 ms faehrt (1,0) -> (1,1), Blick Y_PLUS
///
/// **Zielen** (RAD_CreateUnitAimAnimation, RAD_UnitAnimationAppendAim) dreht
/// den Turm auf dem kuerzeren Weg auf einen Winkel, je Vierteldrehung
/// "turret_ms", weniger weit anteilig kuerzer. Auf Wunsch bleibt er dort
/// "hold_ms" stehen und laeuft danach ebenso nach vorn zurueck: dann feuert
/// die Figur, und das Stehenbleiben ist ein eigener Abschnitt
/// (RAD_UNIT_ANIMATION_FIRE), der mit dem Schuss beginnt
/// (RAD_UnitAnimationFireMs).
///
/// Vor "start_ms" gilt der Anfang des ersten Abschnitts, nach dem letzten
/// dessen Ende. Ein Pfad aus einem Punkt ist eine Fahrt von ihm zu sich
/// selbst, so lang wie ein Schritt.
///
/// Winkel sind Grad, gezaehlt wie in rendering/iso_facing.h; der des Turms gilt
/// gegen die Wanne, 0 nach vorn.
///
/// **Die Zeit gibt der Aufrufer vor**, sie liest keine Uhr selbst; was zu einem
/// Zeitpunkt gilt, gibt RAD_UnitAnimationStateAt zurueck.
///
/// Die Struktur steht hier im Header, wie bei der Interpolation;
/// geaendert werden sollte sie nur ueber die Funktionen.
///
typedef enum
{
    RAD_UNIT_ANIMATION_DRIVE = 0,
    RAD_UNIT_ANIMATION_TURN,
    RAD_UNIT_ANIMATION_AIM,
    RAD_UNIT_ANIMATION_FIRE
} RAD_UnitAnimationPhaseType_t;

///
/// Ein Abschnitt des Plans: von "start_ms" an "duration_ms" lang. Eine Fahrt geht
/// von (from_x, from_y) nach (to_x, to_y), eine Drehung von "from_facing" nach
/// "to_facing", ein Zielen von "from_turret" nach "to_turret", ein Schuss
/// haelt den Turm auf "from_turret" gleich "to_turret" -- was der
/// Abschnitt nicht aendert, ist vorher und nachher gleich. "to_turret" kann
/// ausserhalb von [0, 360) liegen, damit es den kuerzeren Weg nimmt.
///
typedef struct RAD_UnitAnimationPhase
{
    RAD_UnitAnimationPhaseType_t type;
    float start_ms;
    float duration_ms;
    float from_x;
    float from_y;
    float to_x;
    float to_y;
    RAD_IsoFacing_t from_facing;
    RAD_IsoFacing_t to_facing;
    float from_turret;
    float to_turret;
} RAD_UnitAnimationPhase_t;

typedef struct RAD_UnitAnimation
{
    ///
    /// Die Abschnitte in der Reihenfolge, in der sie laufen, "phase_count"
    /// Stueck, mindestens einer; jeder beginnt, wo der vorige endet. Platz ist
    /// fuer "phase_capacity".
    ///
    RAD_UnitAnimationPhase_t *phases;
    size_t phase_count;
    size_t phase_capacity;
} RAD_UnitAnimation_t;

///
/// Wo die Figur zu einem Zeitpunkt steht und wie sie schaut -- dieselben
/// Bedeutungen wie in RAD_IsoEntity_t (rendering/entity.h): waehrend einer
/// Drehung ist "turning" gesetzt, "facing" die Richtung, aus der sie kommt.
/// "turret_angle" liegt in [0, 360).
///
typedef struct RAD_UnitAnimationState
{
    float x;
    float y;
    RAD_IsoFacing_t facing;
    bool turning;
    RAD_IsoFacing_t turn_to;
    float turn_progress;
    float turret_angle;
} RAD_UnitAnimationState_t;

///
/// Legt den Plan fuer einen Zug entlang "count" Punkten an, deren Koordinaten in
/// "xs" und "ys" stehen, fuer eine Figur, die zu Beginn in Richtung "facing"
/// schaut, den Turm um "turret_angle" gedreht; die Punkte werden kopiert, die
/// Arrays muessen die Animation nicht ueberleben.
///
/// NULL, wenn "count" 0 ist oder kein Speicher da ist.
/// RAD_DestroyUnitAnimation nullt den Zeiger des Aufrufers.
///
RAD_UnitAnimation_t* RAD_CreateUnitAnimation(const float *xs, const float *ys, size_t count,
                                             RAD_IsoFacing_t facing, float turret_angle,
                                             float start_ms, float step_ms, float turn_ms, float turret_ms);

///
/// Legt den Plan an, mit dem eine Figur auf (x, y) in Richtung "facing" ihren
/// Turm von "turret_angle" auf "target_angle" richtet -- und, wenn "back"
/// gesetzt ist, nach "hold_ms" wieder nach vorn. NULL, wenn kein Speicher da
/// ist.
///
RAD_UnitAnimation_t* RAD_CreateUnitAimAnimation(float x, float y, RAD_IsoFacing_t facing, float turret_angle,
                                                float target_angle, bool back, float hold_ms,
                                                float start_ms, float turret_ms);

void RAD_DestroyUnitAnimation(RAD_UnitAnimation_t **unit_animation);

///
/// Haengt an das Ende des Plans an, dass die Figur dort ihren Turm auf
/// "target_angle" richtet, wie bei RAD_CreateUnitAimAnimation. false, wenn kein
/// Speicher da ist; dann bleibt der Plan, wie er war.
///
bool RAD_UnitAnimationAppendAim(RAD_UnitAnimation_t *unit_animation, float target_angle, bool back, float hold_ms,
                                float turret_ms);

///
/// Wo die Figur zum Zeitpunkt "now_ms" steht und wie sie schaut. Regelmaessig zu
/// rufen, etwa einmal je Frame.
///
RAD_UnitAnimationState_t RAD_UnitAnimationStateAt(const RAD_UnitAnimation_t *unit_animation, float now_ms);

///
/// Wann die Figur nach dem Plan zuletzt feuert: der Beginn seines letzten
/// Schusses (RAD_UNIT_ANIMATION_FIRE). false, wenn er keinen hat; dann bleibt
/// "fire_ms" unberuehrt.
///
bool RAD_UnitAnimationFireMs(const RAD_UnitAnimation_t *unit_animation, float *fire_ms);

///
/// Ob der Plan zum Zeitpunkt "now_ms" abgelaufen ist: ab dem Ende des letzten
/// Abschnitts.
///
bool RAD_UnitAnimationIsFinished(const RAD_UnitAnimation_t *unit_animation, float now_ms);

#endif
