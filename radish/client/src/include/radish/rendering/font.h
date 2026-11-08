#ifndef __RAD_RENDERING_FONT_H__
#define __RAD_RENDERING_FONT_H__

#include <SDL2/SDL_render.h>
#include <stddef.h>
#include <stdint.h>

///
/// Eine Schrift in einer Groesse: messen und zeichnen von Text in einer Zeile.
/// Wie sie zeichnet, sieht der Aufrufer nicht -- die Views kennen nur diesen
/// Header. Die einzige Umsetzung ist rendering/bitmap_font.c: ein Atlas im
/// BMFont-Textformat (.fnt samt PNG), wie unter assets/fonts/michroma/.
///
/// **Text ist UTF-8.** Zeichen, die die Schrift nicht hat (bei Michroma alles
/// ausserhalb von ASCII 32-126), werden als "?" gezeichnet und gemessen.
///
/// **Eine Schrift haengt an ihrem Renderer**: ihre Textur gehoert zu ihm. Sie
/// muss vor ihm zerstoert werden.
///

///
/// Die Schrift selbst: nur ein Name. Was sie haelt, steht in
/// rendering/bitmap_font.c.
///
typedef struct RAD_Font RAD_Font_t;

///
/// Laedt die Schrift "fnt_path" (BMFont, Textformat) samt dem Atlas, den ihre
/// "page"-Zeile nennt -- relativ zum Verzeichnis der .fnt -- und skaliert sie
/// so, dass sie "pixel_size" Pixel gross ist statt der Groesse, in der der
/// Atlas gezeichnet ist. NULL bei einem Fehler, der Grund steht dann auf
/// stdout. RAD_DestroyFont nullt den Zeiger des Aufrufers.
///
RAD_Font_t* RAD_CreateFont(SDL_Renderer *renderer, const char *fnt_path, int32_t pixel_size);
void RAD_DestroyFont(RAD_Font_t **font);

///
/// Abstand von einer Zeile zur naechsten, in Pixeln. So hoch ist auch, was
/// RAD_FontDraw hoechstens zeichnet.
///
int32_t RAD_FontLineHeight(const RAD_Font_t *font);

///
/// Wie breit die ersten "length" Bytes von "text" sind, in Pixeln, samt
/// Unterschneidung. Ein '\0' davor beendet den Text frueher. 0 fuer NULL.
///
int32_t RAD_FontMeasure(const RAD_Font_t *font, const char *text, size_t length);

///
/// Zeichnet die ersten "length" Bytes von "text" in "color", mit der oberen
/// linken Ecke der Zeile bei (x, y). Was aus "clip" herausragt, wird
/// abgeschnitten; NULL zeichnet alles. Der Clip-Bereich des Renderers bleibt,
/// wie er ist.
///
void RAD_FontDraw(const RAD_Font_t *font, SDL_Renderer *renderer,
                  const char *text, size_t length,
                  int32_t x, int32_t y, SDL_Color color, const SDL_Rect *clip);

#endif
