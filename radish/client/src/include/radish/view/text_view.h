#ifndef __RAD_VIEW_TEXT_VIEW_H__
#define __RAD_VIEW_TEXT_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

///
/// Ein Text in einer Zeile, oben links in seiner Flaeche. Was nicht hineinpasst,
/// wird abgeschnitten.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h). Der Text wird deshalb erst im naechsten
/// RAD_UpdateTextView nach einem Setter gerendert und bis zum naechsten
/// behalten.
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/text_view.c.
///
typedef struct RAD_TextView RAD_TextView_t;

///
/// Legt eine TextView mit der oberen linken Ecke bei (x, y) und width x height
/// Pixeln an, mit leerem Text in Weiss; NULL, wenn kein Speicher da ist. "font"
/// gehoert dem Aufrufer (view/view.h, RAD_ViewFont) und muss die TextView
/// ueberleben. RAD_DestroyTextView nullt den Zeiger des Aufrufers.
///
RAD_TextView_t* RAD_CreateTextView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font);
void RAD_DestroyTextView(RAD_TextView_t **text_view);

///
/// Der angezeigte Text, UTF-8; er wird kopiert. NULL heisst leer. false, wenn
/// fuer die Kopie kein Speicher da ist -- dann bleibt der bisherige Text.
///
bool RAD_TextViewSetText(RAD_TextView_t *text_view, const char *text);

///
/// Farbe des Textes.
///
void RAD_TextViewSetColor(RAD_TextView_t *text_view, SDL_Color color);

///
/// Wie breit der jetzige Text in der Schrift der TextView ist, in Pixeln --
/// ungekuerzt, also auch breiter als die TextView selbst. 0 fuer einen leeren
/// Text, oder wenn SDL_ttf ihn nicht vermessen kann.
///
int32_t RAD_TextViewTextWidth(const RAD_TextView_t *text_view);

///
/// Zeichnet die TextView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateTextView(RAD_TextView_t *text_view, SDL_Renderer *renderer);

#endif
