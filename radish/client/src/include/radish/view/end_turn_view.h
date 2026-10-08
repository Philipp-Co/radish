#ifndef __RAD_VIEW_END_TURN_VIEW_H__
#define __RAD_VIEW_END_TURN_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_session.h>
#include <radish/model/world.h>

///
/// Ein Knopf zum Abgeben des eigenen Zuges, der zugleich zeigt, wer am Zug ist:
///
///   "Zug beenden"      gruen -- man ist selbst dran, ein Klick gibt ab
///   "Gegner am Zug"    grau  -- ein anderer ist dran, Klicks tun nichts
///   "Niemand am Zug"   grau  -- der Server hat noch keinen genannt
///
/// Der Text steht mittig in der Flaeche. Gedrueckt ist der gruene Knopf dunkler.
///
/// **Wer dran ist, liest sie bei jedem Update aus der Welt**
/// (RAD_ClientWorldCurrentPlayer, model/world.h), wer man selbst ist, aus der
/// Session (own_player_id, io/net_session.h). Neu gesetzt wird der Text nur,
/// wenn sich etwas geaendert hat.
///
/// **Ein Klick ist Druecken und Loslassen der linken Maustaste in der
/// Flaeche.** Ist man dran, geht dann das Kommando an den Server
/// (control/end_turn.h). Ob der Zug wirklich weitergeht, entscheidet er; die
/// Anzeige wechselt erst, wenn er meldet, wer jetzt dran ist.
///
/// **Maus-Ereignisse bekommt sie vom Aufrufer** (view/view.h), in
/// Fensterkoordinaten, wie SDL sie liefert.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/end_turn_view.c.
///
typedef struct RAD_EndTurnView RAD_EndTurnView_t;

///
/// Legt eine EndTurnView mit der oberen linken Ecke bei (x, y) und width x
/// height Pixeln an; NULL, wenn kein Speicher da ist. "font" gehoert dem
/// Aufrufer (view/view.h, RAD_ViewFont) und muss die EndTurnView ueberleben,
/// ebenso "world" und "session". RAD_DestroyEndTurnView nullt den Zeiger des
/// Aufrufers.
///
RAD_EndTurnView_t* RAD_CreateEndTurnView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font,
                                         const RAD_ClientWorld_t *world, RAD_IoNetSession_t *session);
void RAD_DestroyEndTurnView(RAD_EndTurnView_t **end_turn_view);

///
/// Wertet das Druecken oder Loslassen einer Maustaste aus (siehe oben). true,
/// wenn es ihr gehoert -- in ihrer Flaeche oder das Loslassen nach einem
/// Druecken in ihr --, dann sollte der Aufrufer es nicht an das weitergeben,
/// was unter ihr liegt. Das gilt auch, wenn man nicht dran ist: ein Klick auf
/// den grauen Knopf waehlt nicht das Feld darunter aus.
///
bool RAD_EndTurnViewHandleMouseButton(RAD_EndTurnView_t *end_turn_view, const SDL_MouseButtonEvent *button);

///
/// Bringt den Text auf den Stand der Welt und zeichnet die EndTurnView mit
/// "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateEndTurnView(RAD_EndTurnView_t *end_turn_view, SDL_Renderer *renderer);

#endif
