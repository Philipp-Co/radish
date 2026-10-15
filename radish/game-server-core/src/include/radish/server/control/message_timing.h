#ifndef __RAD_CONTROL_MESSAGE_TIMING_H__
#define __RAD_CONTROL_MESSAGE_TIMING_H__

#include <stdint.h>

///
/// Wie lange der Server fuer eine Nachricht braucht: vom Herausholen aus dem
/// Ringpuffer bis die letzte Antwort darauf im Sendepuffer liegt. Gezaehlt wird
/// seit dem Start des Servers, zurueckgesetzt wird nicht -- der Hoechstwert ist
/// damit der schlimmste Fall der ganzen Laufzeit.
///
/// Gemessen wird in main um handle_message herum; dieses Modul rechnet nur und
/// gibt nichts aus. Was ins Log geht, entscheidet das Programm.
///
/// **Ein offenes struct und kein unvollstaendiger Typ wie RAD_Control_t.** Es
/// sind drei Zahlen ohne Regeln dazwischen: die Statistik liegt auf dem Stack
/// von main, ohne malloc und ohne Abbau.
///
/// **Zum Senden gehoert auch das Warten.** Ist der Sendepuffer voll, wartet
/// send_message bis zu RAD_SERVER_SEND_RETRY_LIMIT Versuche lang -- das zaehlt mit.
/// Ein hoher Hoechstwert heisst also nicht unbedingt, dass das Spiel langsam
/// rechnet; eine Discover-Antwort ueber die ganze Welt fuellt den Puffer.
///
typedef struct
{
    /// Wie viele Nachrichten gemessen sind.
    uint64_t count;

    /// Die Summe ihrer Dauern, in Nanosekunden. Reicht bei 64 Bit fuer
    /// ueber 500 Jahre Bearbeitungszeit.
    uint64_t total_ns;

    /// Die laengste einzelne Dauer, in Nanosekunden.
    uint64_t max_ns;
} RAD_MessageTiming_t;

///
/// Setzt alles auf null: noch keine Nachricht gemessen.
///
void RAD_MessageTimingInitialize(RAD_MessageTiming_t *timing);

///
/// Die Zeit jetzt in Nanosekunden, von CLOCK_MONOTONIC: sie springt nicht, wenn
/// jemand die Systemzeit verstellt. Fuer sich allein bedeutungslos -- nur die
/// Differenz zweier Werte ist eine Dauer.
///
uint64_t RAD_MessageTimingNow(void);

///
/// Nimmt die Dauer einer Nachricht auf.
///
void RAD_MessageTimingAdd(RAD_MessageTiming_t *timing, uint64_t duration_ns);

///
/// Der Durchschnitt aller aufgenommenen Dauern in Nanosekunden, abgerundet.
/// 0, solange keine aufgenommen ist.
///
uint64_t RAD_MessageTimingAverage(const RAD_MessageTiming_t *timing);

#endif
