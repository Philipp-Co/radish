#ifndef __RAD_VIEW_ANIMATIONS_MOVE_ANIMATION_H__
#define __RAD_VIEW_ANIMATIONS_MOVE_ANIMATION_H__

#include <stdbool.h>
#include <stddef.h>
#include <radish/view/animations/interpolation.h>

///
/// Eine Bewegung entlang eines Pfades aus x/y-Koordinaten.
///
/// **Jeder Abschnitt dauert gleich lang**: Ab "start_ms" geht es in
/// "step_duration_ms" von einem Pfadpunkt zum naechsten, je Abschnitt linear
/// (view/animations/interpolation.h, eine Interpolation fuer x und eine fuer y).
///
///   Pfad (0,0) (10,0) (10,20), start 100 ms, je Schritt 1000 ms:
///       100 ms -> (0,0), 600 ms -> (5,0), 1100 ms -> (10,0),
///       1600 ms -> (10,10), 2100 ms und spaeter -> (10,20)
///
/// Vor "start_ms" steht sie am ersten Punkt, nach dem letzten Abschnitt am
/// letzten. Ein Pfad aus einem Punkt bleibt dort stehen.
///
/// **Das Ergebnis schreibt sie in "x_out" und "y_out"**, Zeiger, die der
/// Aufrufer beim Anlegen mitgibt -- erst beim ersten RAD_UpdateMoveAnimation.
///
/// **Die Zeit gibt der Aufrufer vor**, sie liest keine Uhr selbst.
///
/// Die Struktur steht hier im Header, wie bei der Interpolation; geaendert
/// werden sollte sie nur ueber die Funktionen.
///

typedef struct RAD_MoveAnimation
{
    ///
    /// Je Abschnitt eine Interpolation fuer x und eine fuer y, "segment_count"
    /// Stueck; Abschnitt i laeuft von Pfadpunkt i nach i + 1. Bei einem Pfad
    /// aus einem Punkt ist es ein Abschnitt von diesem Punkt zu sich selbst.
    ///
    RAD_Interpolation_t **x_interpolations;
    RAD_Interpolation_t **y_interpolations;
    size_t segment_count;

    float start_ms;
    float step_duration_ms;
} RAD_MoveAnimation_t;

///
/// Legt eine Bewegung entlang "count" Punkten an, deren Koordinaten in "xs"
/// und "ys" stehen; die Punkte werden kopiert, die Arrays muessen die
/// MoveAnimation nicht ueberleben. "x_out" und "y_out" gehoeren dem Aufrufer
/// und muessen sie ueberleben.
///
/// NULL, wenn "count" 0 ist oder kein Speicher da ist.
/// RAD_DestroyMoveAnimation nullt den Zeiger des Aufrufers.
///
RAD_MoveAnimation_t* RAD_CreateMoveAnimation(const float *xs, const float *ys, size_t count,
                                             float start_ms, float step_duration_ms,
                                             float *x_out, float *y_out);
void RAD_DestroyMoveAnimation(RAD_MoveAnimation_t **move_animation);

///
/// Schreibt die Position zum Zeitpunkt "now_ms" nach "x_out" und "y_out".
/// Regelmaessig zu rufen, etwa einmal je Frame.
///
void RAD_UpdateMoveAnimation(RAD_MoveAnimation_t *move_animation, float now_ms);

///
/// Ob die Bewegung zum Zeitpunkt "now_ms" am letzten Punkt angekommen ist: ab
/// "start_ms" plus der Dauer aller Abschnitte. Ein Pfad aus einem Punkt zaehlt
/// als ein Abschnitt.
///
bool RAD_MoveAnimationIsFinished(const RAD_MoveAnimation_t *move_animation, float now_ms);

#endif
