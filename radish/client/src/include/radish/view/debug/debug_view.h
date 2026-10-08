#ifndef __RAD_VIEW_DEBUG_DEBUG_VIEW_H__
#define __RAD_VIEW_DEBUG_DEBUG_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>

///
/// Ein rechteckiger Bereich mit Werten zur Fehlersuche, als Liste von
/// Schluessel-Wert-Paaren untereinander:
///
///   avg fps      59.8
///   avg update   1.23 ms
///   max update   4.10 ms
///
/// **Ihr Hintergrund ist zu 60% durchsichtig**, was darunter liegt, scheint
/// durch; die Schrift ist voll deckend.
///
/// **Welche Paare es gibt, steht fest**: von aussen kommen nur die Werte, ueber
/// je einen Setter (RAD_DebugViewSetAvgFps, RAD_DebugViewSetAvgUpdateMs,
/// RAD_DebugViewSetMaxUpdateMs). Bis ein Wert gesetzt ist, steht dort "-". Ein
/// neues Paar ist ein neuer Setter in diesem Modul.
///
/// **Ihre Breite steht mit dem Anlegen fest, ihre Hoehe folgt der Zahl der
/// Paare**: sie belegt die Flaeche aus RAD_CreateDebugView in voller Breite
/// von ihrer Oberkante an, so hoch wie alle Zeilen samt Rand, hoechstens so
/// hoch wie diese Flaeche. Was nicht hineinpasst, wird abgeschnitten.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in
/// view/debug/debug_view.c.
///
typedef struct RAD_DebugView RAD_DebugView_t;

///
/// Legt eine DebugView an, die hoechstens die Flaeche mit der oberen linken
/// Ecke bei (x, y) und width x height Pixeln belegt, alle Werte "-"; NULL, wenn
/// kein Speicher da ist. "font" gehoert dem Aufrufer (view/view.h,
/// RAD_ViewFont) und muss die DebugView ueberleben. RAD_DestroyDebugView nullt
/// den Zeiger des Aufrufers.
///
RAD_DebugView_t* RAD_CreateDebugView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font);
void RAD_DestroyDebugView(RAD_DebugView_t **debug_view);

///
/// Die durchschnittliche Zahl der Frames je Sekunde, angezeigt mit einer
/// Nachkommastelle unter "avg fps". Neu gerendert wird nur, wenn sich die
/// Anzeige aendert.
///
void RAD_DebugViewSetAvgFps(RAD_DebugView_t *debug_view, float avg_fps);

///
/// Wie lange ein Frame im Durchschnitt braucht, in Millisekunden, angezeigt
/// mit zwei Nachkommastellen und "ms" unter "avg update". Neu gerendert wird
/// nur, wenn sich die Anzeige aendert.
///
void RAD_DebugViewSetAvgUpdateMs(RAD_DebugView_t *debug_view, float avg_update_ms);

///
/// Wie lange der laengste Frame gebraucht hat, in Millisekunden, angezeigt wie
/// bei RAD_DebugViewSetAvgUpdateMs unter "max update". Ueber welchen Zeitraum,
/// entscheidet der Aufrufer.
///
void RAD_DebugViewSetMaxUpdateMs(RAD_DebugView_t *debug_view, float max_update_ms);

///
/// Zeichnet die DebugView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateDebugView(RAD_DebugView_t *debug_view, SDL_Renderer *renderer);

#endif
