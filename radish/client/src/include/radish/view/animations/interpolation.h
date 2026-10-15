#ifndef __RAD_VIEW_ANIMATIONS_INTERPOLATION_H__
#define __RAD_VIEW_ANIMATIONS_INTERPOLATION_H__

///
/// Ein Wert, der linear von einem Punkt zum anderen laeuft.
///
/// **Der Aufrufer gibt nur zwei Punkte an**: Wert und Zeitpunkt in ms, wo es
/// losgeht ("from"), und wo es ankommen soll ("to"). Die Geschwindigkeit
/// rechnet sie daraus selbst aus.
///
///   from (0, 0 ms), to (10, 1000 ms):
///       -50 ms -> 0, 100 ms -> 1, 500 ms -> 5, 1000 ms -> 10, 2000 ms -> 10
///
/// Vor "from" gilt dessen Wert, nach "to" dessen -- sie schiesst nicht
/// darueber hinaus.
///
/// **Das Ergebnis schreibt sie in "result"**, einen Zeiger, den der Aufrufer
/// beim Anlegen mitgibt -- etwa auf eine Koordinate, die animiert werden soll.
/// Der Aufrufer ruft dazu regelmaessig RAD_UpdateInterpolation, etwa einmal je
/// Frame.
///
/// **Die Zeit gibt der Aufrufer vor**, sie liest keine Uhr selbst. Die
/// Zeitpunkte der Punkte und der Updates muessen sich auf dieselbe Uhr beziehen.
///
/// **Liegt "to" nicht nach "from"**, springt der Wert zum Zeitpunkt von "to"
/// auf dessen Wert.
///
/// Die Struktur steht hier im Header, damit sie auch als Teil einer anderen
/// angelegt werden kann; geaendert werden sollte sie nur ueber die Funktionen.
///

///
/// Ein Wert und der Zeitpunkt in ms, zu dem er gilt.
///
typedef struct RAD_InterpolationPoint
{
    float value;
    float timestamp_ms;
} RAD_InterpolationPoint_t;

typedef struct RAD_Interpolation
{
    RAD_InterpolationPoint_t from;
    RAD_InterpolationPoint_t to;

    ///
    /// Einheiten je ms von "from" nach "to", beim Anlegen aus beiden
    /// errechnet. 0, wenn "to" nicht nach "from" liegt.
    ///
    float speed;

    ///
    /// Wohin RAD_UpdateInterpolation das Ergebnis schreibt. Gehoert dem
    /// Aufrufer und muss die Interpolation ueberleben.
    ///
    float *result;
} RAD_Interpolation_t;

///
/// Legt eine Interpolation von "from" nach "to" an, die ihr Ergebnis nach
/// "result" schreibt; NULL, wenn kein Speicher da ist. Geschrieben wird erst
/// beim ersten RAD_UpdateInterpolation. RAD_DestroyInterpolation nullt den
/// Zeiger des Aufrufers.
///
RAD_Interpolation_t* RAD_CreateInterpolation(RAD_InterpolationPoint_t from, RAD_InterpolationPoint_t to,
                                             float *result);
void RAD_DestroyInterpolation(RAD_Interpolation_t **interpolation);

///
/// Der Wert zum Zeitpunkt "now_ms". Schreibt nichts.
///
float RAD_InterpolationCurrentValue(const RAD_Interpolation_t *interpolation, float now_ms);

///
/// Schreibt den Wert zum Zeitpunkt "now_ms" nach "result". Regelmaessig zu
/// rufen, etwa einmal je Frame.
///
void RAD_UpdateInterpolation(RAD_Interpolation_t *interpolation, float now_ms);

#endif
