"""
Ablauf eines Spiels, den mehrere Schnittstellen teilen: Spieler (Verlassen)
und Spielinstanzen (Spielende melden).
"""

from django.db import transaction

from api.domain.servers import instance_client
from api.models import Game, GameServer


def end_started_game(game: Game) -> None:
    """
    Beendet ein gestartetes Spiel fuer beide Spieler: das Game verschwindet
    (Spieler und Armeen sind damit wieder frei), der Server wird frei, und
    nach dem Commit entzieht die Instanz die Zugangscodes und verwirft das
    Spiel (instance_client.cancel_game).

    Der gemeinsame Weg fuer das Verlassen (LeaveGameView, interfaces/player)
    und fuer das Ende, das die Instanz meldet (GameFinishedView,
    interfaces/servers). Erwartet, in
    einer Transaktion gerufen zu werden, die "game" gesperrt haelt
    (select_for_update).
    """
    server: GameServer = game.server
    game.delete()
    server.is_occupied = False
    server.save(update_fields=["is_occupied"])
    transaction.on_commit(lambda: instance_client.cancel_game(server))
