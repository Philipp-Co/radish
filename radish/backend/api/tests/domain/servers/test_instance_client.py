import json
import unittest
from unittest import mock

from api.domain.servers import instance_client
from api.models import (
    Game,
    TerrainCode,
    TerrainType,
)
from api.tests.game_test_case import GameTestCase
from api.tests.helpers import SPIELSTART_SCHEMA

try:
    import jsonschema
except ImportError:  # wie die ctest-Schema-Tests: ohne Werkzeug entfaellt nur der Test
    # Ohne Werkzeug ist das Modul None; die Tests pruefen das per skipUnless.
    jsonschema = None  # type: ignore[assignment]


class InstanceClientTests(GameTestCase):
    """instance_client.start_game: Ziel-URL, Payload und Fehlerfaelle."""

    def setUp(self) -> None:
        super().setUp()
        self.game = self.open_game()
        self.join(self.game)
        self.game.refresh_from_db()

    @mock.patch("api.domain.servers.instance_client.requests.post")
    def test_start_posts_game_start_document(self, post: mock.MagicMock) -> None:
        post.return_value = mock.Mock(status_code=200)

        self.game.host_zucchini_code = "00000000000000aa"
        self.game.second_player_zucchini_code = "00000000000000bb"

        instance_client.start_game(self.game)

        url: str = post.call_args.args[0]
        self.assertEqual(url, "http://instanz:8000/api/instances/game-instance-1/game/start/")
        sent: instance_client.GameStartRequest = post.call_args.kwargs["json"]
        # Die Spieldaten nach dem Schema, die Codes daneben -- nicht darin.
        self.assertEqual(sent["spieldaten"], instance_client.game_start_document(self.game)["spieldaten"])
        self.assertEqual(sent["zugangscodes"], ["00000000000000aa", "00000000000000bb"])

    def test_document_carries_players_and_army_values(self) -> None:
        self.game.host.name = "host-name"
        self.game.host.save()

        host: instance_client.PlayerDocument
        guest: instance_client.PlayerDocument
        host, guest = instance_client.game_start_document(self.game)["spieldaten"]

        self.assertEqual((host["name"], host["kennung"]), ("host-name", self.game.host.identifier))
        # Ohne Keycloak-Namen ersatzweise die Kennung.
        self.assertEqual(guest["name"], self.player.identifier)
        self.assertEqual(guest["kennung"], self.player.identifier)
        army: instance_client.ArmyDocument = guest["armee"]
        self.assertEqual((army["name"], army["spezies"]), ("Erste Kompanie", "Menschen"))
        unit: instance_client.UnitDocument = army["einheiten"][0]
        self.assertEqual(
            (unit["typ"], unit["bewegung"], unit["sichtweite"], unit["kann_ziele_einnehmen"]),
            ("Trupp", 6, 4, True),
        )
        leader: instance_client.EntityDocument = next(e for e in unit["entitaeten"] if e["profil"] == "Anfuehrer")
        self.assertEqual(
            (leader["leben"], leader["ruestung"], leader["widerstand"], leader["staerke"],
             leader["genauigkeit"]),
            (2, 3, 3, 4, 3),
        )

    def test_document_query_count_is_constant(self) -> None:
        game: Game = Game.objects.select_related("host", "second_player").get(pk=self.game.pk)
        # Armeen samt Spezies, Einheiten mit Typ, Entitaeten mit Profil,
        # Waffen, Ausruestung, dazu das Gelaende -- unabhaengig von der
        # Groesse der Armeen.
        with self.assertNumQueries(6):
            instance_client.game_start_document(game)

    @unittest.skipUnless(jsonschema, "jsonschema nicht installiert")
    def test_document_matches_schema(self) -> None:
        self.game.host.name = "host-name"
        self.game.host.save()
        schema: dict[str, object] = json.loads(SPIELSTART_SCHEMA.read_text())

        jsonschema.validate(instance_client.game_start_document(self.game), schema)

    def test_document_carries_the_terrain(self) -> None:
        TerrainType.objects.filter(code="forest").update(movement_modifier=-2, cover=4)

        terrain: list[instance_client.TerrainDocument] = instance_client.game_start_document(self.game)["gelaende"]

        # Jeder Typ genau einmal, in der Reihenfolge des Spiels.
        self.assertEqual([entry["typ"] for entry in terrain], TerrainCode.values)
        forest: instance_client.TerrainDocument = next(entry for entry in terrain if entry["typ"] == "forest")
        self.assertEqual(
            forest, {"typ": "forest", "bewegung": -2, "deckung": 4, "unbegehbar": False, "sichtblockierend": True}
        )
        deep: instance_client.TerrainDocument = next(entry for entry in terrain if entry["typ"] == "water_deep")
        self.assertTrue(deep["unbegehbar"])
        self.assertFalse(deep["sichtblockierend"])

    @mock.patch("api.domain.servers.instance_client.requests.post")
    def test_start_without_terrain_type_fails(self, post: mock.MagicMock) -> None:
        TerrainType.objects.filter(code="road").delete()

        with self.assertRaises(instance_client.InstanceError):
            instance_client.start_game(self.game)
        post.assert_not_called()

    @mock.patch("api.domain.servers.instance_client.requests.post")
    def test_start_without_control_url_fails(self, post: mock.MagicMock) -> None:
        self.server.control_url = ""
        self.server.save()
        self.game.refresh_from_db()

        with self.assertRaises(instance_client.InstanceError):
            instance_client.start_game(self.game)
        post.assert_not_called()

    @mock.patch("api.domain.servers.instance_client.requests.post")
    def test_start_without_instance_name_fails(self, post: mock.MagicMock) -> None:
        self.server.instance_name = ""
        self.server.save()
        self.game.refresh_from_db()

        with self.assertRaises(instance_client.InstanceError):
            instance_client.start_game(self.game)
        post.assert_not_called()

    @mock.patch("api.domain.servers.instance_client.requests.post")
    def test_start_rejected_by_instance_fails(self, post: mock.MagicMock) -> None:
        post.return_value = mock.Mock(status_code=400, text="kaputt")

        with self.assertRaises(instance_client.InstanceError):
            instance_client.start_game(self.game)
