#ifndef __RAD_IO_NET_SESSION_H__
#define __RAD_IO_NET_SESSION_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_types.h>

///
/// Was der Client ueber seine Verbindung zum Backend weiss: wer er ist und was er
/// zuletzt verschickt hat. Die Anfragen (io/net_request.h) schreiben hinein, die
/// Event-Handler (io/net_event_handler.h) lesen daraus. main.c haelt die einzige
/// Instanz.
///

///
/// Die letzte Discover-Anfrage dieses Clients, damit das Log der Antwort sagt,
/// worauf sie antwortet.
///
/// Die Antwort selbst nennt ihre Anfrage nicht -- auf der Strecke sind es drei
/// Spielereignisse ohne Bezug (protobuf/game.proto). Und der Server schickt sie
/// an jeden Client, nicht nur an den fragenden (game-server-core/src/main.c,
/// send_discover_events). Eine Antwort kann also auch die auf die Anfrage eines
/// anderen sein; das Log nennt deshalb die *letzte eigene* Anfrage und keine
/// sichere Zuordnung.
///
typedef struct
{
    bool sent;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} RAD_IoNetDiscoverRequest_t;

typedef struct
{
    ///
    /// Die eigene, oeffentliche Spieler-Id. Das Backend schickt sie beim
    /// Verbinden mit, die einbettende Seite reicht sie herein
    /// (RAD_ClientSetPlayerId in main.c). Bis dahin RAD_NET_USER_NONE.
    ///
    /// **Nur zum Erkennen, nicht zum Ausweisen.** Mit ihr unterscheidet der
    /// Client, was ihm gehoert und was dem Gegner -- wer ein Kommando geschickt
    /// hat, setzt das Backend vor die Nachricht, und ein user_id, das der Client
    /// hineinschreibt, liest der Server nicht (protobuf/command.proto). Den
    /// geheimen Zucchini-Code, der vor jeder Nachricht steht, kennt der Client
    /// gar nicht: auch den setzt das Backend (radish/backend/api/access.py).
    ///
    RAD_NetUserId_t own_player_id;

    ///
    /// Zeitmessung fuer den Rundlauf: wann das letzte Kommando hinausging und
    /// welche Sequenznummer es trug. Passt die Nummer der Antwort dazu, steht die
    /// Zeit mit im Log.
    ///
    /// Nur die letzte, nicht eine Tabelle: bei 60 Kommandos je Sekunde koennen
    /// mehrere unterwegs sein, und dann trifft eine Antwort ein, deren Kommando
    /// schon zwei weiter ist. Statt dafuer Buch zu fuehren, entfaellt die Zeit in
    /// dem Fall -- sie waere sonst die Zeit eines anderen Kommandos.
    ///
    double last_send_time_ms;
    RAD_NetSequence_t awaiting_sequence;

    ///
    /// Die Sequenznummer des naechsten Kommandos (io/net_types.h). Wer ein
    /// Kommando verschickt, nimmt sie und zaehlt weiter
    /// (RAD_ControlDeployUnit). Beginnt bei 1.
    ///
    RAD_NetSequence_t next_sequence;

    RAD_IoNetDiscoverRequest_t last_discover_request;
} RAD_IoNetSession_t;

#endif
