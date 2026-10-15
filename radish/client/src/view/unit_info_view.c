#include <radish/view/unit_info_view.h>
#include <radish/view/abbreviations.h>
#include <radish/view/typewriter_view.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Abstand zum Rand und zwischen Kopf und Zeilen, in Pixeln. Der Abstand zum
/// Rand zaehlt ab der Aussenkante, der Rahmen liegt also darin.
///
#define RAD_UNIT_INFO_VIEW_PADDING 12
#define RAD_UNIT_INFO_VIEW_GAP 6

///
/// Wie dick der weisse Rahmen ist, in Pixeln, nach innen gezeichnet.
///
#define RAD_UNIT_INFO_VIEW_BORDER 2

///
/// Wie weit eine eingerueckte Zeile eingerueckt ist (view/unit_info_view.h).
///
#define RAD_UNIT_INFO_VIEW_INDENT_SAMPLE "99x "

///
/// Platz fuer die Lebenspunkte als Text samt abschliessender Null:
/// "LP -2147483648/-2147483648".
///
#define RAD_UNIT_INFO_VIEW_HIT_POINTS_MAX 28

///
/// Platz fuer die Bewegungsreichweite als Text samt abschliessender Null:
/// "MV -2147483648".
///
#define RAD_UNIT_INFO_VIEW_MOVEMENT_RANGE_MAX 16

///
/// Oberkante der ersten Zeile, relativ: unter dem Namen und der Zeile mit
/// Lebenspunkten und Bewegungsreichweite (view/unit_info_view.h).
///
#define RAD_UNIT_INFO_VIEW_FIRST_LINE_TOP(line) \
    (RAD_UNIT_INFO_VIEW_PADDING + 2 * ((line) + RAD_UNIT_INFO_VIEW_GAP))


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_UnitInfoView
{
    ///
    /// Die groesstmoegliche Flaeche aus RAD_CreateUnitInfoView, und die, die sie
    /// gerade belegt: deren unterer Teil, so hoch wie der Inhalt
    /// (RAD_UnitInfoViewFitToContent).
    ///
    SDL_Rect max_area;
    SDL_Rect area;
    TTF_Font *font;

    ///
    /// Alle Kinder liegen relativ zur linken oberen Ecke von "area"; gezeichnet
    /// werden sie mit "area" als Viewport (RAD_UpdateUnitInfoView). So muss beim
    /// Wachsen keines verschoben werden.
    ///
    RAD_TypewriterView_t *name;

    ///
    /// Unter dem Namen nebeneinander: die Summe der Lebenspunkte und die
    /// Bewegungsreichweite.
    ///
    RAD_TypewriterView_t *hit_points;
    RAD_TypewriterView_t *movement_range;

    ///
    /// Wie weit eine eingerueckte Zeile eingerueckt ist, in Pixeln.
    ///
    int32_t indent;

    ///
    /// Die Zeilen, je eine Zeile der Schrift hoch, die erste bei
    /// RAD_UNIT_INFO_VIEW_FIRST_LINE_TOP.
    ///
    size_t number_of_lines;
    RAD_TypewriterView_t *lines[RAD_UNIT_INFO_VIEW_LINES_MAX];
};


static int32_t RAD_UnitInfoViewLinesBottom(const RAD_UnitInfoView_t *unit_info_view, size_t number_of_lines);
static void RAD_UnitInfoViewFitToContent(RAD_UnitInfoView_t *unit_info_view);
static void RAD_UnitInfoViewDestroyHeader(RAD_UnitInfoView_t *unit_info_view);


RAD_UnitInfoView_t* RAD_CreateUnitInfoView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font)
{
    RAD_UnitInfoView_t *unit_info_view = malloc(sizeof(struct RAD_UnitInfoView));
    if(unit_info_view == NULL)
    {
        return NULL;
    }

    const int32_t line = TTF_FontLineSkip(font);

    int indent = 0;
    if(TTF_SizeUTF8(font, RAD_UNIT_INFO_VIEW_INDENT_SAMPLE, &indent, NULL) != 0)
    {
        indent = line * 2;
    }

    // Die Zeile unter dem Namen: links die Lebenspunkte, rechts die
    // Bewegungsreichweite, je die halbe Breite.
    const int32_t inner_width = width - 2 * RAD_UNIT_INFO_VIEW_PADDING;
    const int32_t half_width = (inner_width - RAD_UNIT_INFO_VIEW_GAP) / 2;
    const int32_t second_line_top = RAD_UNIT_INFO_VIEW_PADDING + line + RAD_UNIT_INFO_VIEW_GAP;

    *unit_info_view = (struct RAD_UnitInfoView){
        .max_area = { .x = x, .y = y, .w = width, .h = height },
        .area = { .x = x, .y = y, .w = width, .h = height },
        .font = font,
        .name = RAD_CreateTypewriterView(
            RAD_UNIT_INFO_VIEW_PADDING,
            RAD_UNIT_INFO_VIEW_PADDING,
            inner_width,
            line
        ),
        .hit_points = RAD_CreateTypewriterView(RAD_UNIT_INFO_VIEW_PADDING, second_line_top, half_width, line),
        .movement_range = RAD_CreateTypewriterView(
            RAD_UNIT_INFO_VIEW_PADDING + half_width + RAD_UNIT_INFO_VIEW_GAP,
            second_line_top,
            inner_width - half_width - RAD_UNIT_INFO_VIEW_GAP,
            line
        ),
        .indent = indent,
        .number_of_lines = 0,
        .lines = { NULL }
    };

    if(unit_info_view->name == NULL || unit_info_view->hit_points == NULL || unit_info_view->movement_range == NULL)
    {
        RAD_UnitInfoViewDestroyHeader(unit_info_view);
        free(unit_info_view);
        return NULL;
    }

    RAD_TypewriterViewSetFont(unit_info_view->name, font);
    RAD_TypewriterViewSetFont(unit_info_view->hit_points, font);
    RAD_TypewriterViewSetFont(unit_info_view->movement_range, font);

    if(!RAD_UnitInfoViewSetHitPoints(unit_info_view, 0, 0)
       || !RAD_UnitInfoViewSetMovementRange(unit_info_view, 0))
    {
        RAD_UnitInfoViewDestroyHeader(unit_info_view);
        free(unit_info_view);
        return NULL;
    }

    RAD_UnitInfoViewFitToContent(unit_info_view);
    return unit_info_view;
}

void RAD_DestroyUnitInfoView(RAD_UnitInfoView_t **unit_info_view)
{
    RAD_UnitInfoView_t *v = *unit_info_view;

    RAD_UnitInfoViewClearLines(v);
    RAD_UnitInfoViewDestroyHeader(v);
    free(v);
    *unit_info_view = NULL;
}

bool RAD_UnitInfoViewSetName(RAD_UnitInfoView_t *unit_info_view, const char *name)
{
    return RAD_TypewriterViewSetText(unit_info_view->name, name);
}

bool RAD_UnitInfoViewSetHitPoints(RAD_UnitInfoView_t *unit_info_view, int32_t current, int32_t maximum)
{
    char text[RAD_UNIT_INFO_VIEW_HIT_POINTS_MAX];
    snprintf(text, sizeof(text), "%s %d/%d", RAD_VIEW_ABBREVIATION_HIT_POINTS, (int)current, (int)maximum);
    return RAD_TypewriterViewSetText(unit_info_view->hit_points, text);
}

bool RAD_UnitInfoViewSetMovementRange(RAD_UnitInfoView_t *unit_info_view, int32_t movement_range)
{
    char text[RAD_UNIT_INFO_VIEW_MOVEMENT_RANGE_MAX];
    snprintf(text, sizeof(text), "%s %d", RAD_VIEW_ABBREVIATION_MOVEMENT_RANGE, (int)movement_range);
    return RAD_TypewriterViewSetText(unit_info_view->movement_range, text);
}

bool RAD_UnitInfoViewAddLine(RAD_UnitInfoView_t *unit_info_view, const char *text, bool indented)
{
    if(unit_info_view->number_of_lines >= RAD_UNIT_INFO_VIEW_LINES_MAX)
    {
        return false;
    }

    const int32_t line = TTF_FontLineSkip(unit_info_view->font);
    const int32_t x = RAD_UNIT_INFO_VIEW_PADDING + (indented ? unit_info_view->indent : 0);
    const int32_t width = unit_info_view->max_area.w - RAD_UNIT_INFO_VIEW_PADDING - x;
    const int32_t top = RAD_UnitInfoViewLinesBottom(unit_info_view, unit_info_view->number_of_lines);

    RAD_TypewriterView_t *typewriter_view = RAD_CreateTypewriterView(x, top, (width > 0) ? width : 0, line);
    if(typewriter_view == NULL)
    {
        return false;
    }
    RAD_TypewriterViewSetFont(typewriter_view, unit_info_view->font);
    if(!RAD_TypewriterViewSetText(typewriter_view, text))
    {
        RAD_DestroyTypewriterView(&typewriter_view);
        return false;
    }

    unit_info_view->lines[unit_info_view->number_of_lines] = typewriter_view;
    ++unit_info_view->number_of_lines;
    RAD_UnitInfoViewFitToContent(unit_info_view);
    return true;
}

void RAD_UnitInfoViewClearLines(RAD_UnitInfoView_t *unit_info_view)
{
    for(size_t i = 0; i < unit_info_view->number_of_lines; ++i)
    {
        RAD_DestroyTypewriterView(&unit_info_view->lines[i]);
    }

    unit_info_view->number_of_lines = 0;
    RAD_UnitInfoViewFitToContent(unit_info_view);
}

void RAD_UpdateUnitInfoView(RAD_UnitInfoView_t *unit_info_view, SDL_Renderer *renderer)
{
    // Die Kinder zeichnen relativ zu "area" (struct RAD_UnitInfoView); der
    // Viewport schneidet zugleich ab, was darueber hinausgeht. Danach den des
    // Aufrufers wiederherstellen.
    SDL_Rect previous_viewport;
    SDL_RenderGetViewport(renderer, &previous_viewport);
    SDL_RenderSetViewport(renderer, &unit_info_view->area);

    const SDL_Rect background = { .x = 0, .y = 0, .w = unit_info_view->area.w, .h = unit_info_view->area.h };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &background);

    // Der Rahmen: je Pixel Dicke ein Rechteck, eins weiter innen als das davor.
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for(int32_t i = 0; i < RAD_UNIT_INFO_VIEW_BORDER; ++i)
    {
        const SDL_Rect frame = { .x = i, .y = i, .w = background.w - 2 * i, .h = background.h - 2 * i };
        SDL_RenderDrawRect(renderer, &frame);
    }

    RAD_UpdateTypewriterView(unit_info_view->name, renderer);
    RAD_UpdateTypewriterView(unit_info_view->hit_points, renderer);
    RAD_UpdateTypewriterView(unit_info_view->movement_range, renderer);

    const int32_t bottom = unit_info_view->area.h;
    for(size_t i = 0; i < unit_info_view->number_of_lines; ++i)
    {
        if(RAD_UnitInfoViewLinesBottom(unit_info_view, i + 1) > bottom)
        {
            break;
        }
        RAD_UpdateTypewriterView(unit_info_view->lines[i], renderer);
    }

    SDL_RenderSetViewport(renderer, &previous_viewport);
}


///
/// Unterkante der ersten "number_of_lines" Zeilen, relativ zur Oberkante der
/// UnitInfoView -- zugleich Oberkante der Zeile danach. Ohne Zeilen die
/// Oberkante der ersten.
///
static int32_t RAD_UnitInfoViewLinesBottom(const RAD_UnitInfoView_t *unit_info_view, size_t number_of_lines)
{
    const int32_t line = TTF_FontLineSkip(unit_info_view->font);
    return RAD_UNIT_INFO_VIEW_FIRST_LINE_TOP(line) + (int32_t)number_of_lines * line;
}

///
/// Passt "area" dem Inhalt an: so hoch, wie Kopf und Zeilen samt Rand brauchen,
/// hoechstens so hoch wie "max_area", und an deren Unterkante.
///
static void RAD_UnitInfoViewFitToContent(RAD_UnitInfoView_t *unit_info_view)
{
    // Ohne Zeilen steht die Oberkante der ersten einen Abstand unter dem Kopf.
    int32_t height = RAD_UnitInfoViewLinesBottom(unit_info_view, unit_info_view->number_of_lines) + RAD_UNIT_INFO_VIEW_PADDING;
    if(unit_info_view->number_of_lines == 0)
    {
        height -= RAD_UNIT_INFO_VIEW_GAP;
    }
    if(height > unit_info_view->max_area.h)
    {
        height = unit_info_view->max_area.h;
    }

    unit_info_view->area = (SDL_Rect){
        .x = unit_info_view->max_area.x,
        .y = unit_info_view->max_area.y + unit_info_view->max_area.h - height,
        .w = unit_info_view->max_area.w,
        .h = height
    };
}

///
/// Baut Name, Lebenspunkte und Bewegungsreichweite ab, auch wenn sie nur zum
/// Teil angelegt sind (RAD_CreateUnitInfoView im Fehlerfall).
///
static void RAD_UnitInfoViewDestroyHeader(RAD_UnitInfoView_t *unit_info_view)
{
    if(unit_info_view->name != NULL)
    {
        RAD_DestroyTypewriterView(&unit_info_view->name);
    }
    if(unit_info_view->hit_points != NULL)
    {
        RAD_DestroyTypewriterView(&unit_info_view->hit_points);
    }
    if(unit_info_view->movement_range != NULL)
    {
        RAD_DestroyTypewriterView(&unit_info_view->movement_range);
    }
}
