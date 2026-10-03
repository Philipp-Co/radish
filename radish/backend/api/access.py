"""
Wer eine Nachricht an den Spielserver schicken darf und in wessen Namen.

Zwei Werte je Spieler, und der Unterschied ist der Zweck:

    Zucchini-Code   geheim. zucchini_server laesst nur Pakete durch, deren
                    erste 8 Byte ein Code aus seiner Whitelist sind. Das
                    Backend vergibt beim Spielstart je Spieler einen neuen
                    (new_zucchini_code), uebergibt beide der Instanz und setzt
                    ihn im WebSocket-Consumer vor jede Nachricht. Ein Client
                    bekommt ihn nie zu sehen.

    Spieler-Id      oeffentlich. Die Kennung des Spielers (Player.identifier),
                    Zeichen fuer Zeichen in die acht Bytes einer 64-Bit-Zahl
                    gepackt (player_id). Unter ihr fuehrt das Spiel den Spieler
                    und seine Einheiten; sie steht in Antworten und Ereignissen,
                    und der Client bekommt seine eigene beim Verbinden.

Was beim Spielserver ankommt, baut damit allein der Consumer (frame):

    [ Zucchini-Code 8 Byte ][ Spieler-Id 8 Byte ][ Nachricht des Clients ]

zucchini prueft den Code und schneidet ihn ab; der Spielserver liest den
Absender aus den naechsten 8 Byte (RAD_ParseSenderFromMessage in
radish/game-server-core/src/include/radish/server/interface/message.h). Weil
nur das Backend die Codes kennt, kommt kein Paket durch, das es nicht gebaut
hat -- und die Spieler-Id darin ist die des angemeldeten Spielers.
"""

import secrets

# Codes, die nicht vergeben werden: 0 ist in zucchinis Whitelist ein freier
# Platz, und 1 war der feste Code, den alle Clients frueher selbst setzten.
_RESERVED_CODES = {0, 1}


def new_zucchini_code():
    """Ein neuer, zufaelliger Code als 16 Hexziffern."""
    while True:
        code = secrets.randbits(64)
        if code not in _RESERVED_CODES:
            return f"{code:016x}"


def player_id(identifier):
    """
    Die Spieler-Id zur Kennung: die acht ASCII-Zeichen als Big-Endian-Zahl,
    "aB3xK9pQ" -> 0x614233784B397051. Dieselbe Rechnung wie
    RAD_ControlUserIdFromIdentifier im Spielserver (control/game_start.h).
    """
    encoded = identifier.encode("ascii")
    if len(encoded) != 8:
        raise ValueError(f"Kennung muss genau 8 Zeichen haben: {identifier!r}")
    return int.from_bytes(encoded, "big")


def frame(zucchini_code, identifier, payload):
    """Die Nachricht an den Spielserver: Code, Spieler-Id, Nutzlast."""
    return (
        int(zucchini_code, 16).to_bytes(8, "big")
        + player_id(identifier).to_bytes(8, "big")
        + payload
    )
