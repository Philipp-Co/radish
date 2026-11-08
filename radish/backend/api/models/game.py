from django.db import models

from api.models.game_server import GameServer
from api.models.game_status import GameStatus
from api.models.player import Player


class Game(models.Model):
    """
    Ein Spiel. Belegt beim Erstellen genau einen GameServer und hat bis zu
    zwei Spieler: den Ersteller (host) und einen weiteren Mitspieler
    (second_player). Beginnt in der Lobby, siehe GameStatus.
    """

    # unique, weil GameJoinView ein Spiel ueber genau diesen Namen findet
    # (siehe client_views.GameJoinView) -- ohne Eindeutigkeit waere nicht
    # klar, welchem Spiel beigetreten werden soll.
    name = models.CharField(max_length=255, unique=True)
    password = models.CharField(max_length=255)

    # Ein Game belegt genau einen Server, und ein Server ist immer nur von
    # hoechstens einem Game belegt -- deshalb OneToOne und nicht ForeignKey.
    # CASCADE: mit dem Server verschwindet auch das Game, das ihn belegte.
    server = models.OneToOneField(
        GameServer, on_delete=models.CASCADE, related_name="game"
    )

    # CASCADE: ohne den Spieler dahinter ergibt das Spiel keinen Sinn mehr.
    host = models.ForeignKey(Player, on_delete=models.CASCADE, related_name="hosted_games")
    second_player = models.ForeignKey(
        Player, on_delete=models.CASCADE, related_name="joined_games", null=True, blank=True
    )

    # Mit wie vielen Punkten gespielt wird: keine der beiden Armeen darf mehr
    # kosten (siehe client_views._army_for_game).
    points_limit = models.PositiveIntegerField()
    # Die Armeen, mit denen Host und zweiter Spieler antreten. PROTECT: eine
    # Armee in einem laufenden Spiel laesst sich nicht loeschen (und auch
    # nicht aendern, siehe army_views.ArmyDetailView). host_army ist nur in
    # der Datenbank nullable -- fuer Spiele, die es schon vor den Armeen gab;
    # neue Spiele verlangen sie immer (siehe GameCreateView).
    host_army = models.ForeignKey(
        "Army", on_delete=models.PROTECT, related_name="+", null=True, blank=True
    )
    second_player_army = models.ForeignKey(
        "Army", on_delete=models.PROTECT, related_name="+", null=True, blank=True
    )

    status = models.CharField(
        max_length=20, choices=GameStatus.choices, default=GameStatus.LOBBY
    )

    # Die Zucchini-Codes der beiden Spieler, je 16 Hexziffern (siehe
    # access.py). Geheim: vergeben beim Spielstart (GameStartView), an die
    # Instanz fuer ihre Whitelist uebergeben und vom WebSocket-Consumer vor
    # jede Nachricht seines Spielers gesetzt -- nie an einen Client
    # ausgeliefert. Leer, solange das Spiel nicht gestartet ist.
    host_zucchini_code = models.CharField(max_length=16, blank=True, default="")
    second_player_zucchini_code = models.CharField(max_length=16, blank=True, default="")

    def zucchini_code_for(self, player: Player) -> str:
        """Der Code des Spielers in diesem Spiel, "" fuer jemand anderen."""
        if player.pk == self.host_id:
            return self.host_zucchini_code
        if player.pk == self.second_player_id:
            return self.second_player_zucchini_code
        return ""

    def __str__(self) -> str:
        return self.name
