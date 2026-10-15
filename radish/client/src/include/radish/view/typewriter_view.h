#ifndef __RAD_VIEW_TYPEWRITER_VIEW_H__
#define __RAD_VIEW_TYPEWRITER_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

///
/// Ein Text, der wie in einem alten Terminal Zeichen fuer Zeichen erscheint,
/// mit einem Cursor "_" dahinter. Eine Zeile, oben links in ihrer Flaeche; was
/// nicht hineinpasst, wird abgeschnitten.
///
/// **Alle 100 ms erscheint ein Zeichen** (UTF-8-Zeichen, nicht Bytes). Die Zeit
/// liest sie selbst (SDL_GetTicks64); RAD_TypewriterViewSetText beginnt von
/// vorn. Waehrend des Tippens leuchtet der Cursor durchgehend, danach blinkt
/// er, 500 ms an und 500 ms aus -- im selben Takt wie jede andere
/// TypewriterView. Ohne Schrift oder Text zeichnet sie nichts.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h). Text und Cursor werden deshalb erst im naechsten
/// RAD_UpdateTypewriterView nach einem Setter gerendert.
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in
/// view/typewriter_view.c.
///
typedef struct RAD_TypewriterView RAD_TypewriterView_t;

///
/// Legt eine TypewriterView mit der oberen linken Ecke bei (x, y) und
/// width x height Pixeln an; NULL, wenn kein Speicher da ist.
/// RAD_DestroyTypewriterView nullt den Zeiger des Aufrufers.
///
RAD_TypewriterView_t* RAD_CreateTypewriterView(int32_t x, int32_t y, int32_t width, int32_t height);
void RAD_DestroyTypewriterView(RAD_TypewriterView_t **typewriter_view);

///
/// Der angezeigte Text, UTF-8; er wird kopiert. NULL heisst leer. false, wenn
/// fuer die Kopie kein Speicher da ist -- dann bleibt der bisherige Text.
///
bool RAD_TypewriterViewSetText(RAD_TypewriterView_t *typewriter_view, const char *text);

///
/// Die Schrift des Textes. Sie gehoert dem Aufrufer (view/view.h,
/// RAD_ViewFont) und muss die TypewriterView ueberleben. NULL heisst keine
/// Schrift; so ist die TypewriterView auch angelegt.
///
void RAD_TypewriterViewSetFont(RAD_TypewriterView_t *typewriter_view, TTF_Font *font);

///
/// Farbe des Textes. Angelegt ist die TypewriterView in Weiss.
///
void RAD_TypewriterViewSetColor(RAD_TypewriterView_t *typewriter_view, SDL_Color color);

///
/// Wie breit der ganze Text samt Cursor in der Schrift der TypewriterView ist,
/// in Pixeln -- egal, wie viel davon schon getippt ist, und ungekuerzt, also
/// auch breiter als die TypewriterView selbst. 0 ohne Schrift oder Text, oder
/// wenn SDL_ttf ihn nicht vermessen kann.
///
int32_t RAD_TypewriterViewTextWidth(const RAD_TypewriterView_t *typewriter_view);

///
/// Zeichnet die TypewriterView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateTypewriterView(RAD_TypewriterView_t *typewriter_view, SDL_Renderer *renderer);

#endif
