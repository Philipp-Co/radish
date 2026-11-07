from rest_framework import serializers

from ....models import Game, Player


class GameDetailSerializer(serializers.ModelSerializer):
    """
    Antwort auf ein erstelltes/beigetretenes Spiel. Ohne password -- die
    muss der Client ohnehin schon kennen, es besteht kein Grund, sie in
    der Antwort zu wiederholen. Bewusst auch ohne den belegten Server:
    Adresse und Port eines GameServer sollen ueber diese Endpunkte nicht
    nach aussen dringen (die UDP-Bruecke zum Server laeuft ueber den
    WebSocket-Consumer, siehe consumers.EchoConsumer -- der Client braucht
    die Serveradresse dafuer nicht).

    host/second_player sind Player-Fremdschluessel -- nach aussen aber
    weiterhin einfach deren Kennung (identifier), keine verschachtelten
    Player-Objekte, da Player ohnehin nichts weiter als die Kennung traegt.
    """

    host_identifier: serializers.SlugRelatedField[Player] = serializers.SlugRelatedField(
        source="host", slug_field="identifier", read_only=True
    )
    second_player_identifier: serializers.SlugRelatedField[Player] = serializers.SlugRelatedField(
        source="second_player", slug_field="identifier", read_only=True
    )

    host_army_name = serializers.CharField(source="host_army.name", read_only=True)
    second_player_army_name = serializers.CharField(
        source="second_player_army.name", read_only=True
    )

    # Ob der anfragende Spieler (context["player"]) Host ist -- nur der darf
    # das Spiel aus der Lobby heraus starten (siehe client_views
    # .GameStartView). Ohne Player im Kontext False.
    is_host = serializers.SerializerMethodField()

    class Meta:
        model = Game
        fields = [
            "id",
            "name",
            "host_identifier",
            "second_player_identifier",
            "points_limit",
            "host_army_name",
            "second_player_army_name",
            "status",
            "is_host",
        ]

    def get_is_host(self, obj: Game) -> bool:
        player: Player | None = self.context.get("player")
        return player is not None and obj.host_id == player.id
