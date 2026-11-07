from api.models import (
    Game,
    GameServer,
    Player,
)
from api.interfaces.player.serializers.army_write_serializer import ArmyWriteSerializer

from api.tests.army_test_case import ArmyTestCase
from api.tests.helpers import User


class GameTestCase(ArmyTestCase):
    """
    Ein Spieler (self.player) mit einer Armee und ein freier Server --
    Grundlage fuer die Tests rund um Spiele.
    """

    def setUp(self):
        super().setUp()
        self.player = Player.objects.create(user=self.user)
        self.army = self.army_for(self.player)
        self.server = GameServer.objects.create(
            name="server-a", address="10.0.0.1", port=7000, control_url="http://instanz:8000",
            instance_name="game-instance-1",
        )

    def army_for(self, player, name="Erste Kompanie", units=None):
        serializer = ArmyWriteSerializer(
            data=self.army_payload(units=units, name=name), context={"player": player}
        )
        serializer.is_valid(raise_exception=True)
        return serializer.save()

    def create_game(self, army_id=None, points_limit=100, name="spiel"):
        return self.client.post(
            "/api/client/game/",
            {
                "name": name,
                "password": "geheim",
                "points_limit": points_limit,
                "army_id": army_id if army_id is not None else self.army.id,
            },
            format="json",
        )

    def open_game(self, points_limit=100):
        """Ein Spiel eines anderen Spielers, dem self.user beitreten kann."""
        host = Player.objects.create(user=User.objects.create(username="host-user"))
        self.server.is_occupied = True
        self.server.save(update_fields=["is_occupied"])
        return Game.objects.create(
            name="offen", password="geheim", server=self.server, host=host,
            points_limit=points_limit, host_army=self.army_for(host),
        )

    def join(self, game, army_id=None):
        return self.client.post(
            "/api/client/game/join/",
            {
                "name": game.name,
                "password": "geheim",
                "army_id": army_id if army_id is not None else self.army.id,
            },
            format="json",
        )
