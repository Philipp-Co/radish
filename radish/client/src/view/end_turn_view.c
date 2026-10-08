#include <radish/view/end_turn_view.h>
#include <radish/control/end_turn.h>
#include <radish/view/text_view.h>
#include <stdlib.h>


///
/// Abstand des Textes zum Rand, in Pixeln.
///
#define RAD_END_TURN_VIEW_PADDING 4

///
/// Die Texte und Farben je Zustand (view/end_turn_view.h).
///
#define RAD_END_TURN_VIEW_TEXT_OWN_TURN "Zug beenden"
#define RAD_END_TURN_VIEW_TEXT_OTHER_TURN "Gegner am Zug"
#define RAD_END_TURN_VIEW_TEXT_NO_TURN "Niemand am Zug"

#define RAD_END_TURN_VIEW_COLOR_ENABLED ((SDL_Color){ 0, 140, 0, 255 })
#define RAD_END_TURN_VIEW_COLOR_PRESSED ((SDL_Color){ 0, 90, 0, 255 })
#define RAD_END_TURN_VIEW_COLOR_DISABLED ((SDL_Color){ 80, 80, 80, 255 })
#define RAD_END_TURN_VIEW_COLOR_DISABLED_TEXT ((SDL_Color){ 180, 180, 180, 255 })
#define RAD_END_TURN_VIEW_COLOR_TEXT ((SDL_Color){ 255, 255, 255, 255 })


///
/// Wer am Zug ist, aus Sicht des eigenen Spielers.
///
typedef enum
{
    RAD_END_TURN_VIEW_STATE_NO_TURN = 0,
    RAD_END_TURN_VIEW_STATE_OWN_TURN,
    RAD_END_TURN_VIEW_STATE_OTHER_TURN
} RAD_EndTurnViewState_t;


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_EndTurnView
{
    SDL_Rect area;
    RAD_TextView_t *label;

    ///
    /// Wo die Oberkante des Textes steht: senkrecht mittig in "area". Die
    /// linke Kante folgt jedem neuen Text (RAD_EndTurnViewSync).
    ///
    int32_t text_y;

    ///
    /// Woher sie liest, wer dran ist und wer man selbst ist; ueber "session"
    /// geht auch das Kommando hinaus. Gehoeren dem Aufrufer.
    ///
    const RAD_ClientWorld_t *world;
    RAD_IoNetSession_t *session;

    ///
    /// Wozu der Text gerade angezeigt wird (RAD_EndTurnViewSync). Gilt erst,
    /// wenn "synced".
    ///
    bool synced;
    RAD_EndTurnViewState_t shown_state;

    ///
    /// Ob die linke Maustaste in der Flaeche gedrueckt und noch nicht
    /// losgelassen wurde.
    ///
    bool pressed;
};


static RAD_EndTurnViewState_t RAD_EndTurnViewState(const RAD_EndTurnView_t *end_turn_view);
static bool RAD_EndTurnViewSync(RAD_EndTurnView_t *end_turn_view);


RAD_EndTurnView_t* RAD_CreateEndTurnView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font,
                                         const RAD_ClientWorld_t *world, RAD_IoNetSession_t *session)
{
    RAD_EndTurnView_t *end_turn_view = malloc(sizeof(struct RAD_EndTurnView));
    if(end_turn_view == NULL)
    {
        return NULL;
    }

    // Senkrecht mittig; waagerecht setzt RAD_EndTurnViewSync den Text je nach
    // seiner Breite.
    const int32_t line = TTF_FontLineSkip(font);
    const int32_t text_y = y + (height - line) / 2;

    *end_turn_view = (struct RAD_EndTurnView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .label = RAD_CreateTextView(x + RAD_END_TURN_VIEW_PADDING, text_y, width - 2 * RAD_END_TURN_VIEW_PADDING, line, font),
        .text_y = text_y,
        .world = world,
        .session = session,
        .synced = false,
        .shown_state = RAD_END_TURN_VIEW_STATE_NO_TURN,
        .pressed = false
    };

    if(end_turn_view->label == NULL || !RAD_EndTurnViewSync(end_turn_view))
    {
        RAD_DestroyEndTurnView(&end_turn_view);
        return NULL;
    }

    return end_turn_view;
}

///
/// Baut auch eine halb angelegte EndTurnView ab (RAD_CreateEndTurnView im
/// Fehlerfall): die TextView ist dann womoeglich NULL.
///
void RAD_DestroyEndTurnView(RAD_EndTurnView_t **end_turn_view)
{
    RAD_EndTurnView_t *v = *end_turn_view;

    if(v->label != NULL)
    {
        RAD_DestroyTextView(&v->label);
    }

    free(v);
    *end_turn_view = NULL;
}

bool RAD_EndTurnViewHandleMouseButton(RAD_EndTurnView_t *end_turn_view, const SDL_MouseButtonEvent *button)
{
    const SDL_Point point = { .x = button->x, .y = button->y };
    const bool inside = SDL_PointInRect(&point, &end_turn_view->area);

    if(button->button != SDL_BUTTON_LEFT)
    {
        return inside;
    }

    if(button->type == SDL_MOUSEBUTTONDOWN)
    {
        end_turn_view->pressed = inside;
        return inside;
    }

    // Losgelassen: ein Klick nur, wenn auch das Druecken in der Flaeche war.
    const bool was_pressed = end_turn_view->pressed;
    end_turn_view->pressed = false;

    if(was_pressed && inside && RAD_EndTurnViewState(end_turn_view) == RAD_END_TURN_VIEW_STATE_OWN_TURN)
    {
        RAD_ControlEndTurn(end_turn_view->session);
    }
    return inside || was_pressed;
}

void RAD_UpdateEndTurnView(RAD_EndTurnView_t *end_turn_view, SDL_Renderer *renderer)
{
    // Scheitert es, bleibt der alte Text stehen; der naechste Frame versucht es neu.
    RAD_EndTurnViewSync(end_turn_view);

    // Die Farbe folgt dem Zustand, wie er jetzt ist -- nicht dem, zu dem der
    // Text zuletzt gesetzt werden konnte.
    SDL_Color color = RAD_END_TURN_VIEW_COLOR_DISABLED;
    if(RAD_EndTurnViewState(end_turn_view) == RAD_END_TURN_VIEW_STATE_OWN_TURN)
    {
        color = end_turn_view->pressed ? RAD_END_TURN_VIEW_COLOR_PRESSED : RAD_END_TURN_VIEW_COLOR_ENABLED;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &end_turn_view->area);

    // Ist die Flaeche kleiner als der Text, schneidet sie ihn ab; danach die
    // Clip-Flaeche des Aufrufers wiederherstellen.
    const SDL_bool was_clipping = SDL_RenderIsClipEnabled(renderer);
    SDL_Rect previous_clip;
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &end_turn_view->area);

    RAD_UpdateTextView(end_turn_view->label, renderer);

    SDL_RenderSetClipRect(renderer, was_clipping ? &previous_clip : NULL);
}


///
/// Wer am Zug ist, aus Sicht des eigenen Spielers. Solange der Client sich
/// selbst nicht kennt (RAD_NET_USER_NONE), ist er nie selbst dran.
///
static RAD_EndTurnViewState_t RAD_EndTurnViewState(const RAD_EndTurnView_t *end_turn_view)
{
    const RAD_ClientPlayerId_t current = RAD_ClientWorldCurrentPlayer(end_turn_view->world);
    if(current == RAD_CLIENT_PLAYER_NONE)
    {
        return RAD_END_TURN_VIEW_STATE_NO_TURN;
    }

    const RAD_ClientPlayerId_t own = (RAD_ClientPlayerId_t)end_turn_view->session->own_player_id;
    return (own != RAD_CLIENT_PLAYER_NONE && current == own) ? RAD_END_TURN_VIEW_STATE_OWN_TURN : RAD_END_TURN_VIEW_STATE_OTHER_TURN;
}

///
/// Setzt Text und Textfarbe auf den Zustand (RAD_EndTurnViewState) und den Text
/// waagerecht mittig in die Flaeche -- nur, wenn sich der Zustand seit dem
/// letzten Mal geaendert hat: jeder neue Text muss neu gerendert werden. false,
/// wenn fuer den Text kein Speicher da ist; dann gilt sie als nicht auf Stand.
///
static bool RAD_EndTurnViewSync(RAD_EndTurnView_t *end_turn_view)
{
    const RAD_EndTurnViewState_t state = RAD_EndTurnViewState(end_turn_view);
    if(end_turn_view->synced && state == end_turn_view->shown_state)
    {
        return true;
    }

    const char *text = RAD_END_TURN_VIEW_TEXT_NO_TURN;
    SDL_Color text_color = RAD_END_TURN_VIEW_COLOR_DISABLED_TEXT;
    switch(state)
    {
        case RAD_END_TURN_VIEW_STATE_OWN_TURN:
            text = RAD_END_TURN_VIEW_TEXT_OWN_TURN;
            text_color = RAD_END_TURN_VIEW_COLOR_TEXT;
            break;
        case RAD_END_TURN_VIEW_STATE_OTHER_TURN:
            text = RAD_END_TURN_VIEW_TEXT_OTHER_TURN;
            break;
        case RAD_END_TURN_VIEW_STATE_NO_TURN:
        default:
            break;
    }

    end_turn_view->synced = RAD_TextViewSetText(end_turn_view->label, text);
    end_turn_view->shown_state = state;
    if(!end_turn_view->synced)
    {
        return false;
    }

    RAD_TextViewSetColor(end_turn_view->label, text_color);

    // Mittig, aber nie links aus der Flaeche heraus: ein zu breiter Text
    // beginnt am Rand und wird rechts abgeschnitten.
    const SDL_Rect *area = &end_turn_view->area;
    const int32_t centered = area->x + (area->w - RAD_TextViewTextWidth(end_turn_view->label)) / 2;
    const int32_t left = area->x + RAD_END_TURN_VIEW_PADDING;
    RAD_TextViewSetPosition(end_turn_view->label, (centered > left) ? centered : left, end_turn_view->text_y);
    return true;
}
