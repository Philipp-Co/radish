#include <radish/view/end_turn_view.h>
#include <radish/control/end_turn.h>
#include <radish/view/text_view.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Abstand des Textes zum Rand, in Pixeln.
///
#define RAD_END_TURN_VIEW_PADDING 4

///
/// Dicke des Rahmens, in Pixeln -- innen am Rand, kleiner als der Abstand oben.
///
#define RAD_END_TURN_VIEW_BORDER 2

///
/// Die Texte und Farben je Zustand (view/end_turn_view.h).
///
#define RAD_END_TURN_VIEW_TEXT_OWN_TURN "Zug beenden"
#define RAD_END_TURN_VIEW_TEXT_OTHER_TURN "Gegner am Zug"
#define RAD_END_TURN_VIEW_TEXT_NO_TURN "Niemand am Zug"
#define RAD_END_TURN_VIEW_TEXT_WON "Gewonnen"
#define RAD_END_TURN_VIEW_TEXT_LOST "Verloren"
#define RAD_END_TURN_VIEW_TEXT_DRAW "Unentschieden"

///
/// Platz fuer den ganzen Text: Runde und Zustand, oder Ausgang und Punkte.
///
#define RAD_END_TURN_VIEW_TEXT_MAX 64

///
/// Die Farben der Buttons im Frontend (frontend/src/styles.css): schwarz, Schrift
/// in --heading, Rahmen in --button-hover; gedrueckt ist der Hintergrund
/// --button-hover, ebenso unter der Maus (button:hover); der Rahmen behaelt seine
/// Farbe. Gesperrt ist dort alles halb durchsichtig (opacity: 0.5) --
/// hier auf schwarzem Grund schon eingerechnet.
///
#define RAD_END_TURN_VIEW_COLOR_BACKGROUND ((SDL_Color){ 0x00, 0x00, 0x00, 255 })       // --button-bg
#define RAD_END_TURN_VIEW_COLOR_PRESSED ((SDL_Color){ 0x66, 0x5b, 0x29, 255 })          // --button-hover
#define RAD_END_TURN_VIEW_COLOR_TEXT ((SDL_Color){ 0xcc, 0xa3, 0x00, 255 })             // --heading
#define RAD_END_TURN_VIEW_COLOR_BORDER ((SDL_Color){ 0x66, 0x5b, 0x29, 255 })           // --button-hover
#define RAD_END_TURN_VIEW_COLOR_DISABLED_TEXT ((SDL_Color){ 0x66, 0x52, 0x00, 255 })    // --heading, halb
#define RAD_END_TURN_VIEW_COLOR_DISABLED_BORDER ((SDL_Color){ 0x33, 0x2e, 0x15, 255 })  // --button-hover, halb


///
/// Wer am Zug ist, aus Sicht des eigenen Spielers.
///
typedef enum
{
    RAD_END_TURN_VIEW_STATE_NO_TURN = 0,
    RAD_END_TURN_VIEW_STATE_OWN_TURN,
    RAD_END_TURN_VIEW_STATE_OTHER_TURN,
    RAD_END_TURN_VIEW_STATE_FINISHED
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
    int32_t shown_round;
    int32_t shown_rounds;

    ///
    /// Ob die linke Maustaste in der Flaeche gedrueckt und noch nicht
    /// losgelassen wurde.
    ///
    bool pressed;

    ///
    /// Ob der Mauszeiger ueber der Flaeche steht (RAD_EndTurnViewHandleMouseMotion).
    ///
    bool hovered;
};


static RAD_EndTurnViewState_t RAD_EndTurnViewState(const RAD_EndTurnView_t *end_turn_view);
static bool RAD_EndTurnViewSync(RAD_EndTurnView_t *end_turn_view);
static void RAD_EndTurnViewResultText(const RAD_EndTurnView_t *end_turn_view, char *text, size_t size);


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
        .shown_round = 0,
        .shown_rounds = 0,
        .pressed = false,
        .hovered = false
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

void RAD_EndTurnViewHandleMouseMotion(RAD_EndTurnView_t *end_turn_view, const SDL_MouseMotionEvent *motion)
{
    const SDL_Point point = { .x = motion->x, .y = motion->y };
    end_turn_view->hovered = SDL_PointInRect(&point, &end_turn_view->area);
}

void RAD_EndTurnViewHandleMouseLeave(RAD_EndTurnView_t *end_turn_view)
{
    end_turn_view->hovered = false;
}

void RAD_UpdateEndTurnView(RAD_EndTurnView_t *end_turn_view, SDL_Renderer *renderer)
{
    // Scheitert es, bleibt der alte Text stehen; der naechste Frame versucht es neu.
    RAD_EndTurnViewSync(end_turn_view);

    // Die Farbe folgt dem Zustand, wie er jetzt ist -- nicht dem, zu dem der
    // Text zuletzt gesetzt werden konnte.
    // Der Rahmen ist so hell wie die Schrift: voll, solange man dran ist oder
    // das Spiel vorbei ist, sonst gedaempft (RAD_EndTurnViewSync).
    const RAD_EndTurnViewState_t state = RAD_EndTurnViewState(end_turn_view);
    SDL_Color color = RAD_END_TURN_VIEW_COLOR_BACKGROUND;
    SDL_Color border = RAD_END_TURN_VIEW_COLOR_DISABLED_BORDER;
    if(state == RAD_END_TURN_VIEW_STATE_OWN_TURN)
    {
        // Unter der Maus wie gedrueckt, wie ein Button im Frontend (button:hover).
        const bool lit = end_turn_view->pressed || end_turn_view->hovered;
        color = lit ? RAD_END_TURN_VIEW_COLOR_PRESSED : RAD_END_TURN_VIEW_COLOR_BACKGROUND;
        border = RAD_END_TURN_VIEW_COLOR_BORDER;
    }
    else if(state == RAD_END_TURN_VIEW_STATE_FINISHED)
    {
        border = RAD_END_TURN_VIEW_COLOR_BORDER;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &end_turn_view->area);
    // Je Pixel Dicke ein Rechteck, eins weiter innen als das davor -- wie bei
    // der UnitInfoView.
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    for(int32_t i = 0; i < RAD_END_TURN_VIEW_BORDER; ++i)
    {
        const SDL_Rect frame = {
            .x = end_turn_view->area.x + i,
            .y = end_turn_view->area.y + i,
            .w = end_turn_view->area.w - 2 * i,
            .h = end_turn_view->area.h - 2 * i
        };
        SDL_RenderDrawRect(renderer, &frame);
    }

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
    // Vorbei ist vorbei: dann ist niemand mehr dran, auch man selbst nicht.
    if(RAD_ClientWorldIsFinished(end_turn_view->world))
    {
        return RAD_END_TURN_VIEW_STATE_FINISHED;
    }

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
    // Der Ausgang aendert sich nicht mehr, wenn er einmal da ist: dafuer reicht der
    // Zustand. Waehrend gespielt wird, zaehlt auch die Runde.
    const RAD_EndTurnViewState_t state = RAD_EndTurnViewState(end_turn_view);
    const int32_t round = RAD_ClientWorldRound(end_turn_view->world);
    const int32_t rounds = RAD_ClientWorldNumberOfRounds(end_turn_view->world);
    if(end_turn_view->synced && state == end_turn_view->shown_state
       && round == end_turn_view->shown_round && rounds == end_turn_view->shown_rounds)
    {
        return true;
    }

    const char *turn = RAD_END_TURN_VIEW_TEXT_NO_TURN;
    SDL_Color text_color = RAD_END_TURN_VIEW_COLOR_DISABLED_TEXT;
    switch(state)
    {
        case RAD_END_TURN_VIEW_STATE_OWN_TURN:
            turn = RAD_END_TURN_VIEW_TEXT_OWN_TURN;
            text_color = RAD_END_TURN_VIEW_COLOR_TEXT;
            break;
        case RAD_END_TURN_VIEW_STATE_OTHER_TURN:
            turn = RAD_END_TURN_VIEW_TEXT_OTHER_TURN;
            break;
        case RAD_END_TURN_VIEW_STATE_FINISHED:
            text_color = RAD_END_TURN_VIEW_COLOR_TEXT;
            break;
        case RAD_END_TURN_VIEW_STATE_NO_TURN:
        default:
            break;
    }

    // Vorne die Runde, solange der Server eine geschickt hat; nach dem Ende der
    // Ausgang mit den Punkten.
    char text[RAD_END_TURN_VIEW_TEXT_MAX];
    if(state == RAD_END_TURN_VIEW_STATE_FINISHED)
    {
        RAD_EndTurnViewResultText(end_turn_view, text, sizeof(text));
    }
    else if(rounds > 0)
    {
        snprintf(text, sizeof(text), "Runde %d/%d: %s", (int)round, (int)rounds, turn);
    }
    else
    {
        snprintf(text, sizeof(text), "%s", turn);
    }

    end_turn_view->synced = RAD_TextViewSetText(end_turn_view->label, text);
    end_turn_view->shown_state = state;
    end_turn_view->shown_round = round;
    end_turn_view->shown_rounds = rounds;
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

///
/// Der Ausgang aus Sicht des eigenen Spielers, mit den eigenen Punkten zuerst:
/// "Gewonnen 25:10", "Verloren 10:25", "Unentschieden 10:10". Ohne Punkte -- der
/// Server hat keine geschickt -- nur das Wort.
///
static void RAD_EndTurnViewResultText(const RAD_EndTurnView_t *end_turn_view, char *text, size_t size)
{
    const RAD_ClientGameResult_t *result = RAD_ClientWorldResult(end_turn_view->world);
    const RAD_ClientPlayerId_t own = (RAD_ClientPlayerId_t)end_turn_view->session->own_player_id;

    const char *outcome = RAD_END_TURN_VIEW_TEXT_DRAW;
    if(result->winner != RAD_CLIENT_PLAYER_NONE)
    {
        outcome = (own != RAD_CLIENT_PLAYER_NONE && result->winner == own) ? RAD_END_TURN_VIEW_TEXT_WON : RAD_END_TURN_VIEW_TEXT_LOST;
    }

    // Die eigenen Punkte und die des anderen; kennt der Client sich nicht, die
    // Reihenfolge des Servers.
    int32_t own_score = -1;
    int32_t other_score = -1;
    for(int32_t i = 0; i < result->number_of_players; ++i)
    {
        const bool is_own = (own != RAD_CLIENT_PLAYER_NONE) ? (result->players[i] == own) : (own_score < 0);
        if(is_own && own_score < 0)
        {
            own_score = result->scores[i];
        }
        else if(other_score < 0)
        {
            other_score = result->scores[i];
        }
    }

    if(own_score >= 0 && other_score >= 0)
    {
        snprintf(text, size, "%s %d:%d", outcome, (int)own_score, (int)other_score);
    }
    else
    {
        snprintf(text, size, "%s", outcome);
    }
}
