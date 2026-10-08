#include <radish/view/unit_deployment_view.h>
#include <radish/view/text_view.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Abstand zum Rand und zwischen den Zeilen, Breite des Scrollbalkens und
/// sein Abstand zur Liste, Mindesthoehe seines Griffs, in Pixeln.
///
#define RAD_UNIT_DEPLOYMENT_VIEW_PADDING 4
#define RAD_UNIT_DEPLOYMENT_VIEW_GAP 4
#define RAD_UNIT_DEPLOYMENT_VIEW_SCROLLBAR_WIDTH 8
#define RAD_UNIT_DEPLOYMENT_VIEW_SCROLLBAR_GAP 4
#define RAD_UNIT_DEPLOYMENT_VIEW_THUMB_MIN_HEIGHT 16

///
/// Ab wie vielen Pixeln Bewegung aus einem Klick ein Ziehen wird, und wie
/// lange zwei Klicks auseinander sein duerfen, um ein Doppelklick zu sein, in
/// Millisekunden.
///
#define RAD_UNIT_DEPLOYMENT_VIEW_DRAG_THRESHOLD 4
#define RAD_UNIT_DEPLOYMENT_VIEW_DOUBLE_CLICK_MS 400

///
/// Mit wie vielen Plaetzen fuer Zeilen die Liste anfaengt; reicht das nicht,
/// verdoppelt sie sich.
///
#define RAD_UNIT_DEPLOYMENT_VIEW_INITIAL_CAPACITY 8

///
/// Steht fuer "keine Zeile" (RAD_UnitDeploymentViewEntryAt).
///
#define RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY (-1)


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_UnitDeploymentView
{
    ///
    /// Die ganze Flaeche; darin die Liste, links, und der Scrollbalken, rechts
    /// (RAD_UnitDeploymentViewLayout).
    ///
    SDL_Rect area;
    SDL_Rect list_area;
    SDL_Rect scrollbar_area;

    TTF_Font *font;
    int32_t row_height;
    bool visible;

    ///
    /// Woher die Zeilen kommen (RAD_UnitDeploymentViewSetReserve). Gehoert der
    /// Welt; NULL fuer keine.
    ///
    const RAD_ClientReserve_t *reserve;
    const RAD_ClientUnitRepository_t *units;

    ///
    /// Je Zeile eine TextView mit dem Namen und die Id der Einheit dazu:
    /// "count" belegt von "capacity" Plaetzen (RAD_UnitDeploymentViewReadReserve).
    ///
    RAD_TextView_t **entries;
    RAD_ClientUnitId_t *entry_ids;
    int32_t count;
    int32_t capacity;

    ///
    /// Wie weit die Liste nach oben geschoben ist, in Pixeln: 0 zeigt die
    /// erste Zeile ganz oben (RAD_UnitDeploymentViewSetScroll).
    ///
    int32_t scroll;

    ///
    /// Die Id der ausgewaehlten Einheit, RAD_CLIENT_UNIT_ID_NONE fuer keine. Die Id
    /// und nicht die Zeile: die Reserve kann sich aendern.
    ///
    RAD_ClientUnitId_t selected_id;

    ///
    /// Ob die linke Maustaste in der Liste gedrueckt ist, und ob sie seitdem
    /// gezogen wird; wo der Zeiger beim Druecken stand und wie weit sie da
    /// geschoben war.
    ///
    bool pressed;
    bool dragging;
    int32_t press_y;
    int32_t press_scroll;

    ///
    /// Der erste Klick eines moeglichen Doppelklicks: welche Zeile, und wann
    /// (SDL-Zeitstempel in Millisekunden). RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY,
    /// wenn keiner aussteht.
    ///
    int32_t last_click_entry;
    uint32_t last_click_time;
};


static void RAD_UnitDeploymentViewLayout(RAD_UnitDeploymentView_t *unit_deployment_view, int32_t x, int32_t y, int32_t width, int32_t height);
static void RAD_UnitDeploymentViewReadReserve(RAD_UnitDeploymentView_t *unit_deployment_view);
static bool RAD_UnitDeploymentViewAddEntry(RAD_UnitDeploymentView_t *unit_deployment_view, const RAD_ClientUnit_t *unit);
static void RAD_UnitDeploymentViewClearEntries(RAD_UnitDeploymentView_t *unit_deployment_view);
static void RAD_UnitDeploymentViewResetInput(RAD_UnitDeploymentView_t *unit_deployment_view);
static bool RAD_UnitDeploymentViewContains(const SDL_Rect *area, int32_t x, int32_t y);
static int32_t RAD_UnitDeploymentViewEntryAt(const RAD_UnitDeploymentView_t *unit_deployment_view, int32_t x, int32_t y);
static int32_t RAD_UnitDeploymentViewMaxScroll(const RAD_UnitDeploymentView_t *unit_deployment_view);
static void RAD_UnitDeploymentViewSetScroll(RAD_UnitDeploymentView_t *unit_deployment_view, int32_t scroll);
static void RAD_UnitDeploymentViewClick(RAD_UnitDeploymentView_t *unit_deployment_view, const SDL_MouseButtonEvent *button);
static void RAD_UnitDeploymentViewDrawScrollbar(const RAD_UnitDeploymentView_t *unit_deployment_view, SDL_Renderer *renderer);


RAD_UnitDeploymentView_t* RAD_CreateUnitDeploymentView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font)
{
    RAD_UnitDeploymentView_t *unit_deployment_view = malloc(sizeof(struct RAD_UnitDeploymentView));
    if(unit_deployment_view == NULL)
    {
        return NULL;
    }

    *unit_deployment_view = (struct RAD_UnitDeploymentView){
        .font = font,
        .row_height = TTF_FontLineSkip(font) + RAD_UNIT_DEPLOYMENT_VIEW_GAP,
        .visible = false,
        .reserve = NULL,
        .units = NULL,
        .entries = NULL,
        .entry_ids = NULL,
        .count = 0,
        .capacity = 0,
        .scroll = 0,
        .selected_id = RAD_CLIENT_UNIT_ID_NONE,
        .pressed = false,
        .dragging = false,
        .press_y = 0,
        .press_scroll = 0,
        .last_click_entry = RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY,
        .last_click_time = 0
    };
    RAD_UnitDeploymentViewLayout(unit_deployment_view, x, y, width, height);

    return unit_deployment_view;
}

void RAD_DestroyUnitDeploymentView(RAD_UnitDeploymentView_t **unit_deployment_view)
{
    RAD_UnitDeploymentView_t *v = *unit_deployment_view;

    RAD_UnitDeploymentViewClearEntries(v);
    free(v->entries);
    free(v->entry_ids);

    free(v);
    *unit_deployment_view = NULL;
}

void RAD_UnitDeploymentViewSetReserve(RAD_UnitDeploymentView_t *unit_deployment_view,
                                      const RAD_ClientReserve_t *reserve,
                                      const RAD_ClientUnitRepository_t *units)
{
    unit_deployment_view->reserve = reserve;
    unit_deployment_view->units = units;
}

void RAD_UnitDeploymentViewSetPosition(RAD_UnitDeploymentView_t *unit_deployment_view, int32_t x, int32_t y)
{
    RAD_UnitDeploymentViewLayout(unit_deployment_view, x, y, unit_deployment_view->area.w, unit_deployment_view->area.h);
}

void RAD_UnitDeploymentViewSetVisible(RAD_UnitDeploymentView_t *unit_deployment_view, bool visible)
{
    if(visible)
    {
        RAD_UnitDeploymentViewReadReserve(unit_deployment_view);
    }
    unit_deployment_view->visible = visible;
    RAD_UnitDeploymentViewResetInput(unit_deployment_view);
}

bool RAD_UnitDeploymentViewIsVisible(const RAD_UnitDeploymentView_t *unit_deployment_view)
{
    return unit_deployment_view->visible;
}

const RAD_ClientUnit_t* RAD_UnitDeploymentViewSelectedUnit(const RAD_UnitDeploymentView_t *unit_deployment_view)
{
    if(unit_deployment_view->selected_id == RAD_CLIENT_UNIT_ID_NONE
       || unit_deployment_view->reserve == NULL
       || unit_deployment_view->units == NULL
       || !RAD_ClientReserveContains(unit_deployment_view->reserve, unit_deployment_view->selected_id))
    {
        return NULL;
    }

    return RAD_ClientUnitRepositoryFindConst(unit_deployment_view->units, unit_deployment_view->selected_id);
}

void RAD_UnitDeploymentViewClearSelection(RAD_UnitDeploymentView_t *unit_deployment_view)
{
    unit_deployment_view->selected_id = RAD_CLIENT_UNIT_ID_NONE;
}

bool RAD_UnitDeploymentViewHandleMouseMotion(RAD_UnitDeploymentView_t *unit_deployment_view, const SDL_MouseMotionEvent *motion)
{
    if(!unit_deployment_view->visible)
    {
        return false;
    }

    if(!unit_deployment_view->pressed)
    {
        return RAD_UnitDeploymentViewContains(&unit_deployment_view->area, motion->x, motion->y);
    }

    // Erst ab ein paar Pixeln ist es ein Ziehen, sonst wird es ein Klick.
    const int32_t dy = motion->y - unit_deployment_view->press_y;
    if(!unit_deployment_view->dragging && abs(dy) >= RAD_UNIT_DEPLOYMENT_VIEW_DRAG_THRESHOLD)
    {
        unit_deployment_view->dragging = true;
        unit_deployment_view->last_click_entry = RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY;
    }

    // Die Liste folgt dem Zeiger: nach unten gezogen, rueckt sie nach unten.
    if(unit_deployment_view->dragging)
    {
        RAD_UnitDeploymentViewSetScroll(unit_deployment_view, unit_deployment_view->press_scroll - dy);
    }
    return true;
}

bool RAD_UnitDeploymentViewHandleMouseButton(RAD_UnitDeploymentView_t *unit_deployment_view, const SDL_MouseButtonEvent *button)
{
    if(!unit_deployment_view->visible)
    {
        return false;
    }

    const bool inside = RAD_UnitDeploymentViewContains(&unit_deployment_view->area, button->x, button->y);
    if(button->button != SDL_BUTTON_LEFT)
    {
        return inside;
    }

    if(button->type == SDL_MOUSEBUTTONDOWN)
    {
        if(!inside)
        {
            return false;
        }
        unit_deployment_view->pressed = true;
        unit_deployment_view->dragging = false;
        unit_deployment_view->press_y = button->y;
        unit_deployment_view->press_scroll = unit_deployment_view->scroll;
        return true;
    }

    // Loslassen: gehoert ihr, wenn das Druecken in ihr war -- auch ausserhalb.
    if(!unit_deployment_view->pressed)
    {
        return inside;
    }
    unit_deployment_view->pressed = false;

    if(unit_deployment_view->dragging)
    {
        unit_deployment_view->dragging = false;
        return true;
    }

    RAD_UnitDeploymentViewClick(unit_deployment_view, button);
    return true;
}

void RAD_UpdateUnitDeploymentView(RAD_UnitDeploymentView_t *unit_deployment_view, SDL_Renderer *renderer)
{
    if(!unit_deployment_view->visible)
    {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
    SDL_RenderFillRect(renderer, &unit_deployment_view->area);

    // Nur in die Liste zeichnen: halb sichtbare Zeilen oben und unten werden
    // abgeschnitten. Danach die Clip-Flaeche des Aufrufers wiederherstellen.
    const SDL_bool was_clipping = SDL_RenderIsClipEnabled(renderer);
    SDL_Rect previous_clip;
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &unit_deployment_view->list_area);

    const SDL_Rect *list = &unit_deployment_view->list_area;
    const int32_t row_height = unit_deployment_view->row_height;
    for(int32_t i = 0; i < unit_deployment_view->count; i++)
    {
        const int32_t row_y = list->y + i * row_height - unit_deployment_view->scroll;
        if(row_y + row_height <= list->y || row_y >= list->y + list->h)
        {
            continue;
        }

        if(unit_deployment_view->entry_ids[i] == unit_deployment_view->selected_id)
        {
            const SDL_Rect highlight = { .x = list->x, .y = row_y, .w = list->w, .h = row_height };
            SDL_SetRenderDrawColor(renderer, 0, 96, 160, 255);
            SDL_RenderFillRect(renderer, &highlight);
        }

        RAD_TextViewSetPosition(unit_deployment_view->entries[i], list->x, row_y + RAD_UNIT_DEPLOYMENT_VIEW_GAP / 2);
        RAD_UpdateTextView(unit_deployment_view->entries[i], renderer);
    }

    SDL_RenderSetClipRect(renderer, was_clipping ? &previous_clip : NULL);

    RAD_UnitDeploymentViewDrawScrollbar(unit_deployment_view, renderer);
}


///
/// Legt die Flaechen fest: das Ganze mit der oberen linken Ecke bei (x, y) und
/// width x height Pixeln, darin der Scrollbalken am rechten Rand und die Liste
/// links daneben, beide so hoch wie die Flaeche ohne Rand.
///
static void RAD_UnitDeploymentViewLayout(RAD_UnitDeploymentView_t *unit_deployment_view, int32_t x, int32_t y, int32_t width, int32_t height)
{
    const int32_t inner_height = height - 2 * RAD_UNIT_DEPLOYMENT_VIEW_PADDING;
    const int32_t scrollbar_x = x + width - RAD_UNIT_DEPLOYMENT_VIEW_PADDING - RAD_UNIT_DEPLOYMENT_VIEW_SCROLLBAR_WIDTH;
    const int32_t list_x = x + RAD_UNIT_DEPLOYMENT_VIEW_PADDING;
    const int32_t list_width = scrollbar_x - RAD_UNIT_DEPLOYMENT_VIEW_SCROLLBAR_GAP - list_x;

    unit_deployment_view->area = (SDL_Rect){ .x = x, .y = y, .w = width, .h = height };
    unit_deployment_view->list_area = (SDL_Rect){
        .x = list_x,
        .y = y + RAD_UNIT_DEPLOYMENT_VIEW_PADDING,
        .w = (list_width > 0) ? list_width : 0,
        .h = (inner_height > 0) ? inner_height : 0
    };
    unit_deployment_view->scrollbar_area = (SDL_Rect){
        .x = scrollbar_x,
        .y = y + RAD_UNIT_DEPLOYMENT_VIEW_PADDING,
        .w = RAD_UNIT_DEPLOYMENT_VIEW_SCROLLBAR_WIDTH,
        .h = (inner_height > 0) ? inner_height : 0
    };
}

///
/// Baut die Zeilen aus der Reserve neu auf, eine je Einheit, in deren
/// Reihenfolge. Die Liste steht danach ganz oben, nichts ist ausgewaehlt.
/// Fehlt fuer eine Zeile der Speicher, fehlt nur sie -- und alle danach.
///
static void RAD_UnitDeploymentViewReadReserve(RAD_UnitDeploymentView_t *unit_deployment_view)
{
    RAD_UnitDeploymentViewClearEntries(unit_deployment_view);
    unit_deployment_view->scroll = 0;
    unit_deployment_view->selected_id = RAD_CLIENT_UNIT_ID_NONE;

    const RAD_ClientReserve_t *reserve = unit_deployment_view->reserve;
    if(reserve == NULL || unit_deployment_view->units == NULL)
    {
        return;
    }

    const uint32_t number_of_units = RAD_ClientReserveNumberOfUnits(reserve);
    for(uint32_t i = 0; i < number_of_units; i++)
    {
        const RAD_ClientUnit_t *unit = RAD_ClientUnitRepositoryFindConst(unit_deployment_view->units, RAD_ClientReserveUnitAt(reserve, i));
        if(unit != NULL && !RAD_UnitDeploymentViewAddEntry(unit_deployment_view, unit))
        {
            printf("[UnitDeploymentView] Kein Speicher fuer Zeile %u\n", (unsigned)i);
            return;
        }
    }
}

///
/// Haengt eine Zeile fuer "unit" an: ihr Name und ihre Id. false, wenn kein
/// Speicher da ist -- dann bleibt die Liste, wie sie war.
///
static bool RAD_UnitDeploymentViewAddEntry(RAD_UnitDeploymentView_t *unit_deployment_view, const RAD_ClientUnit_t *unit)
{
    if(unit_deployment_view->count == unit_deployment_view->capacity)
    {
        const int32_t capacity = (unit_deployment_view->capacity > 0)
            ? 2 * unit_deployment_view->capacity
            : RAD_UNIT_DEPLOYMENT_VIEW_INITIAL_CAPACITY;

        // Beide Listen wachsen gemeinsam; scheitert die zweite, ist die erste
        // nur groesser als noetig.
        RAD_TextView_t **entries = realloc(unit_deployment_view->entries, (size_t)capacity * sizeof(RAD_TextView_t*));
        if(entries == NULL)
        {
            return false;
        }
        unit_deployment_view->entries = entries;

        RAD_ClientUnitId_t *entry_ids = realloc(unit_deployment_view->entry_ids, (size_t)capacity * sizeof(RAD_ClientUnitId_t));
        if(entry_ids == NULL)
        {
            return false;
        }
        unit_deployment_view->entry_ids = entry_ids;
        unit_deployment_view->capacity = capacity;
    }

    // Wohin sie gehoert, setzt RAD_UpdateUnitDeploymentView jeden Frame.
    const SDL_Rect *list = &unit_deployment_view->list_area;
    RAD_TextView_t *entry = RAD_CreateTextView(list->x, list->y, list->w, TTF_FontLineSkip(unit_deployment_view->font), unit_deployment_view->font);
    if(entry == NULL)
    {
        return false;
    }
    if(!RAD_TextViewSetText(entry, unit->name))
    {
        RAD_DestroyTextView(&entry);
        return false;
    }

    unit_deployment_view->entries[unit_deployment_view->count] = entry;
    unit_deployment_view->entry_ids[unit_deployment_view->count] = unit->id;
    unit_deployment_view->count++;
    return true;
}

///
/// Entfernt alle Zeilen; die Plaetze bleiben fuer die naechsten.
///
static void RAD_UnitDeploymentViewClearEntries(RAD_UnitDeploymentView_t *unit_deployment_view)
{
    for(int32_t i = 0; i < unit_deployment_view->count; i++)
    {
        RAD_DestroyTextView(&unit_deployment_view->entries[i]);
    }
    unit_deployment_view->count = 0;
}

///
/// Bricht ein Ziehen ab und vergisst einen halben Doppelklick.
///
static void RAD_UnitDeploymentViewResetInput(RAD_UnitDeploymentView_t *unit_deployment_view)
{
    unit_deployment_view->pressed = false;
    unit_deployment_view->dragging = false;
    unit_deployment_view->last_click_entry = RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY;
}

static bool RAD_UnitDeploymentViewContains(const SDL_Rect *area, int32_t x, int32_t y)
{
    const SDL_Point point = { .x = x, .y = y };
    return SDL_PointInRect(&point, area);
}

///
/// Die Zeile unter dem Punkt (x, y), samt Scrollstand;
/// RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY, wenn er nicht in der Liste oder unter
/// der letzten Zeile liegt.
///
static int32_t RAD_UnitDeploymentViewEntryAt(const RAD_UnitDeploymentView_t *unit_deployment_view, int32_t x, int32_t y)
{
    const SDL_Rect *list = &unit_deployment_view->list_area;
    if(!RAD_UnitDeploymentViewContains(list, x, y) || unit_deployment_view->row_height <= 0)
    {
        return RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY;
    }

    const int32_t entry = (y - list->y + unit_deployment_view->scroll) / unit_deployment_view->row_height;
    return (entry < unit_deployment_view->count) ? entry : RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY;
}

///
/// Wie weit die Liste hoechstens geschoben werden kann: bis ihre letzte Zeile
/// unten in der Flaeche steht. 0, wenn sie ganz hineinpasst.
///
static int32_t RAD_UnitDeploymentViewMaxScroll(const RAD_UnitDeploymentView_t *unit_deployment_view)
{
    const int32_t content_height = unit_deployment_view->count * unit_deployment_view->row_height;
    const int32_t max_scroll = content_height - unit_deployment_view->list_area.h;
    return (max_scroll > 0) ? max_scroll : 0;
}

///
/// Setzt den Scrollstand, begrenzt auf 0 bis RAD_UnitDeploymentViewMaxScroll.
///
static void RAD_UnitDeploymentViewSetScroll(RAD_UnitDeploymentView_t *unit_deployment_view, int32_t scroll)
{
    const int32_t max_scroll = RAD_UnitDeploymentViewMaxScroll(unit_deployment_view);
    if(scroll < 0)
    {
        scroll = 0;
    }
    if(scroll > max_scroll)
    {
        scroll = max_scroll;
    }
    unit_deployment_view->scroll = scroll;
}

///
/// Ein Klick, also Druecken und Loslassen ohne Ziehen. Der zweite auf dieselbe
/// Zeile innerhalb von RAD_UNIT_DEPLOYMENT_VIEW_DOUBLE_CLICK_MS waehlt ihre
/// Einheit aus; sonst ist er der erste eines moeglichen Doppelklicks.
///
static void RAD_UnitDeploymentViewClick(RAD_UnitDeploymentView_t *unit_deployment_view, const SDL_MouseButtonEvent *button)
{
    const int32_t entry = RAD_UnitDeploymentViewEntryAt(unit_deployment_view, button->x, button->y);
    if(entry == RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY)
    {
        unit_deployment_view->last_click_entry = RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY;
        return;
    }

    if(entry == unit_deployment_view->last_click_entry
       && button->timestamp - unit_deployment_view->last_click_time <= RAD_UNIT_DEPLOYMENT_VIEW_DOUBLE_CLICK_MS)
    {
        unit_deployment_view->selected_id = unit_deployment_view->entry_ids[entry];
        unit_deployment_view->last_click_entry = RAD_UNIT_DEPLOYMENT_VIEW_NO_ENTRY;
        return;
    }

    unit_deployment_view->last_click_entry = entry;
    unit_deployment_view->last_click_time = button->timestamp;
}

///
/// Zeichnet den Scrollbalken: die Schiene, und darauf den Griff, so hoch wie
/// der sichtbare Anteil der Liste, mindestens
/// RAD_UNIT_DEPLOYMENT_VIEW_THUMB_MIN_HEIGHT, und so weit unten, wie die
/// Liste geschoben ist. Passt die Liste ganz hinein, fuellt der Griff die
/// Schiene.
///
static void RAD_UnitDeploymentViewDrawScrollbar(const RAD_UnitDeploymentView_t *unit_deployment_view, SDL_Renderer *renderer)
{
    const SDL_Rect *track = &unit_deployment_view->scrollbar_area;
    SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255);
    SDL_RenderFillRect(renderer, track);

    const int32_t content_height = unit_deployment_view->count * unit_deployment_view->row_height;
    const int32_t max_scroll = RAD_UnitDeploymentViewMaxScroll(unit_deployment_view);

    SDL_Rect thumb = *track;
    if(max_scroll > 0)
    {
        // In 64 Bit gerechnet, wie bei der BarView.
        thumb.h = (int)(((int64_t)track->h * unit_deployment_view->list_area.h) / content_height);
        if(thumb.h < RAD_UNIT_DEPLOYMENT_VIEW_THUMB_MIN_HEIGHT)
        {
            thumb.h = RAD_UNIT_DEPLOYMENT_VIEW_THUMB_MIN_HEIGHT;
        }
        if(thumb.h > track->h)
        {
            thumb.h = track->h;
        }
        thumb.y = track->y + (int)(((int64_t)(track->h - thumb.h) * unit_deployment_view->scroll) / max_scroll);
    }

    SDL_SetRenderDrawColor(renderer, 160, 160, 160, 255);
    SDL_RenderFillRect(renderer, &thumb);
}
