#include <radish/view/unit_info_view.h>
#include <radish/view/abbreviations.h>
#include <radish/view/labeled_bar_view.h>
#include <radish/view/text_view.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Abstand zum Rand, zwischen den Spalten und zwischen den Zeilen, in Pixeln.
///
#define RAD_UNIT_INFO_VIEW_PADDING 4
#define RAD_UNIT_INFO_VIEW_GAP 4

///
/// Wofuer die Spalte mit der Anzahl breit genug ist (view/unit_info_view.h).
///
#define RAD_UNIT_INFO_VIEW_COUNT_SAMPLE "99x"

///
/// Platz fuer die Anzahl als Text samt abschliessender Null: "4294967295x".
///
#define RAD_UNIT_INFO_VIEW_COUNT_MAX 12

///
/// Platz fuer die Bewegungsreichweite als Text samt abschliessender Null:
/// "MV -2147483648".
///
#define RAD_UNIT_INFO_VIEW_MOVEMENT_RANGE_MAX 16

///
/// Oberkante der ersten Zeile, relativ: unter dem Namen und der Zeile mit
/// Lebenspunkten und Bewegungsreichweite (view/unit_info_view.h).
///
#define RAD_UNIT_INFO_VIEW_FIRST_ROW_TOP(line) \
    (RAD_UNIT_INFO_VIEW_PADDING + 2 * ((line) + RAD_UNIT_INFO_VIEW_GAP))


///
/// Eine Zeile: die Anzahl links, die EntityView rechts. "bottom" ist die
/// Unterkante in Pixeln, relativ zur Oberkante der UnitInfoView -- liegt sie
/// unter deren Unterkante, wird die Zeile nicht gezeichnet.
///
typedef struct
{
    RAD_TextView_t *count;
    RAD_EntityView_t *entity;
    int32_t bottom;
} RAD_UnitInfoViewRow_t;

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
    RAD_TextView_t *name;

    ///
    /// Unter dem Namen nebeneinander: die Summe der Lebenspunkte als Balken,
    /// die Bewegungsreichweite als Zahl.
    ///
    RAD_LabeledBarView_t *hit_points;
    RAD_TextView_t *movement_range;

    ///
    /// Breite der Spalte mit der Anzahl, und Oberkante der naechsten Zeile, die
    /// angehaengt wird (relativ).
    ///
    int32_t count_column_width;
    int32_t next_row_top;

    size_t number_of_rows;
    RAD_UnitInfoViewRow_t rows[RAD_UNIT_INFO_VIEW_ENTITIES_MAX];
};


static void RAD_UnitInfoViewDestroyRow(RAD_UnitInfoViewRow_t *row);
static bool RAD_UnitInfoViewSetCountText(RAD_TextView_t *count_view, uint32_t count);
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

    int count_column_width = 0;
    if(TTF_SizeUTF8(font, RAD_UNIT_INFO_VIEW_COUNT_SAMPLE, &count_column_width, NULL) != 0)
    {
        count_column_width = line * 2;
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
        .name = RAD_CreateTextView(
            RAD_UNIT_INFO_VIEW_PADDING,
            RAD_UNIT_INFO_VIEW_PADDING,
            inner_width,
            line,
            font
        ),
        .hit_points = RAD_CreateLabeledBarView(RAD_UNIT_INFO_VIEW_PADDING, second_line_top, half_width, line, font),
        .movement_range = RAD_CreateTextView(
            RAD_UNIT_INFO_VIEW_PADDING + half_width + RAD_UNIT_INFO_VIEW_GAP,
            second_line_top,
            inner_width - half_width - RAD_UNIT_INFO_VIEW_GAP,
            line,
            font
        ),
        .count_column_width = count_column_width,
        .next_row_top = RAD_UNIT_INFO_VIEW_FIRST_ROW_TOP(line),
        .number_of_rows = 0,
        .rows = { { NULL, NULL, 0 } }
    };

    if(unit_info_view->name == NULL || unit_info_view->hit_points == NULL || unit_info_view->movement_range == NULL
       || !RAD_LabeledBarViewSetLabel(unit_info_view->hit_points, RAD_VIEW_ABBREVIATION_HIT_POINTS)
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

    RAD_UnitInfoViewClearEntities(v);
    RAD_UnitInfoViewDestroyHeader(v);
    free(v);
    *unit_info_view = NULL;
}

bool RAD_UnitInfoViewSetName(RAD_UnitInfoView_t *unit_info_view, const char *name)
{
    return RAD_TextViewSetText(unit_info_view->name, name);
}

void RAD_UnitInfoViewSetHitPoints(RAD_UnitInfoView_t *unit_info_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(unit_info_view->hit_points, current, maximum);
}

bool RAD_UnitInfoViewSetMovementRange(RAD_UnitInfoView_t *unit_info_view, int32_t movement_range)
{
    char text[RAD_UNIT_INFO_VIEW_MOVEMENT_RANGE_MAX];
    snprintf(text, sizeof(text), "%s %d", RAD_VIEW_ABBREVIATION_MOVEMENT_RANGE, (int)movement_range);
    return RAD_TextViewSetText(unit_info_view->movement_range, text);
}

RAD_EntityView_t* RAD_UnitInfoViewAddEntity(RAD_UnitInfoView_t *unit_info_view, uint32_t count, size_t number_of_weapons)
{
    if(unit_info_view->number_of_rows >= RAD_UNIT_INFO_VIEW_ENTITIES_MAX || number_of_weapons > RAD_ENTITY_VIEW_WEAPONS_MAX)
    {
        return NULL;
    }

    const int32_t line = TTF_FontLineSkip(unit_info_view->font);
    const int32_t top = unit_info_view->next_row_top;
    const int32_t count_x = RAD_UNIT_INFO_VIEW_PADDING;
    const int32_t entity_x = count_x + unit_info_view->count_column_width + RAD_UNIT_INFO_VIEW_GAP;
    const int32_t entity_width = unit_info_view->max_area.w - RAD_UNIT_INFO_VIEW_PADDING - entity_x;
    const int32_t entity_height = RAD_EntityViewHeight(unit_info_view->font, number_of_weapons);

    RAD_UnitInfoViewRow_t row = {
        .count = RAD_CreateTextView(count_x, top, unit_info_view->count_column_width, line, unit_info_view->font),
        .entity = RAD_CreateEntityView(entity_x, top, (entity_width > 0) ? entity_width : 0, entity_height, unit_info_view->font),
        .bottom = top + entity_height
    };

    if(row.count == NULL || row.entity == NULL
       || !RAD_UnitInfoViewSetCountText(row.count, count)
       || !RAD_EntityViewSetNumberOfWeapons(row.entity, number_of_weapons))
    {
        RAD_UnitInfoViewDestroyRow(&row);
        return NULL;
    }

    unit_info_view->rows[unit_info_view->number_of_rows] = row;
    ++unit_info_view->number_of_rows;
    unit_info_view->next_row_top = row.bottom + RAD_UNIT_INFO_VIEW_GAP;
    RAD_UnitInfoViewFitToContent(unit_info_view);

    return row.entity;
}

bool RAD_UnitInfoViewSetEntityCount(RAD_UnitInfoView_t *unit_info_view, size_t row, uint32_t count)
{
    if(row >= unit_info_view->number_of_rows)
    {
        return false;
    }
    return RAD_UnitInfoViewSetCountText(unit_info_view->rows[row].count, count);
}

void RAD_UnitInfoViewClearEntities(RAD_UnitInfoView_t *unit_info_view)
{
    for(size_t i = 0; i < unit_info_view->number_of_rows; ++i)
    {
        RAD_UnitInfoViewDestroyRow(&unit_info_view->rows[i]);
    }

    unit_info_view->number_of_rows = 0;
    unit_info_view->next_row_top = RAD_UNIT_INFO_VIEW_FIRST_ROW_TOP(TTF_FontLineSkip(unit_info_view->font));
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
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderFillRect(renderer, &background);

    RAD_UpdateTextView(unit_info_view->name, renderer);
    RAD_UpdateLabeledBarView(unit_info_view->hit_points, renderer);
    RAD_UpdateTextView(unit_info_view->movement_range, renderer);

    const int32_t bottom = unit_info_view->area.h;
    for(size_t i = 0; i < unit_info_view->number_of_rows; ++i)
    {
        const RAD_UnitInfoViewRow_t *row = &unit_info_view->rows[i];
        if(row->bottom > bottom)
        {
            break;
        }

        RAD_UpdateTextView(row->count, renderer);
        RAD_UpdateEntityView(row->entity, renderer);
    }

    SDL_RenderSetViewport(renderer, &previous_viewport);
}


///
/// Baut die Views einer Zeile ab, auch einer halb angelegten
/// (RAD_UnitInfoViewAddEntity im Fehlerfall).
///
static void RAD_UnitInfoViewDestroyRow(RAD_UnitInfoViewRow_t *row)
{
    if(row->count != NULL)
    {
        RAD_DestroyTextView(&row->count);
    }
    if(row->entity != NULL)
    {
        RAD_DestroyEntityView(&row->entity);
    }
}

static bool RAD_UnitInfoViewSetCountText(RAD_TextView_t *count_view, uint32_t count)
{
    char text[RAD_UNIT_INFO_VIEW_COUNT_MAX];
    snprintf(text, sizeof(text), "%ux", (unsigned)count);
    return RAD_TextViewSetText(count_view, text);
}

///
/// Passt "area" dem Inhalt an: so hoch, wie Name und Zeilen samt Rand brauchen,
/// hoechstens so hoch wie "max_area", und an deren Unterkante.
///
static void RAD_UnitInfoViewFitToContent(RAD_UnitInfoView_t *unit_info_view)
{
    // next_row_top steht schon einen Abstand unter dem letzten Inhalt.
    int32_t height = unit_info_view->next_row_top - RAD_UNIT_INFO_VIEW_GAP + RAD_UNIT_INFO_VIEW_PADDING;
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
        RAD_DestroyTextView(&unit_info_view->name);
    }
    if(unit_info_view->hit_points != NULL)
    {
        RAD_DestroyLabeledBarView(&unit_info_view->hit_points);
    }
    if(unit_info_view->movement_range != NULL)
    {
        RAD_DestroyTextView(&unit_info_view->movement_range);
    }
}
