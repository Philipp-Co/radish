#ifndef __RAD_GAME_USER_H__
#define __RAD_GAME_USER_H__

#include <stdint.h>

///
/// Uuid eines Benutzers: wer vor einem Client sitzt, nicht was in der Welt steht.
/// Beides gibt es nebeneinander -- ein Benutzer fuehrt eine Figur
/// (RAD_EntityId_t), ist aber nicht sie: die Figur kann fallen und neu gesetzt
/// werden, der Benutzer bleibt derselbe.
///
/// Vergeben wird sie nicht hier. Sie kommt von aussen, und der Server erkennt sie
/// nur wieder; wer sie ausstellt, ist eine Frage der Anmeldung und noch offen.
///
/// Vorzeichenlos und acht Byte breit, weil sie nicht durchgezaehlt, sondern
/// gezogen wird. Die 0 ist reserviert und nie ein gueltiger Benutzer -- damit
/// laesst sich "kein Benutzer" hinschreiben, ohne ein zweites Feld dafuer zu
/// fuehren, und ein genullter Platz in einer Tabelle faellt auf, statt als
/// Benutzer 0 durchzugehen.
///
/// Der Typ liegt im Spielmodul, und dort steht auch, wer mitspielt (turn.h)
/// und wem welche Figur gehoert (RAD_Entity_t.owner). Eine Welt hat damit nicht
/// nur Entitaeten, sondern auch Konten -- der Grund ist der Zug: wer dran ist,
/// ist eine Spielregel, und Zug und Besitz laufen ueber dieselben Benutzer.
///
typedef uint64_t RAD_UserId_t;

#define RAD_USER_NONE ((RAD_UserId_t)0)

#endif
