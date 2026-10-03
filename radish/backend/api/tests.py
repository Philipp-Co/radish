"""
Tests fuer api/. GameServer-Endpunkt (server_views.py) und die
Matchmaking-Regeln rund um Beitreten (client_views.GameJoinView) --
Authentifizierung laeuft ueber echte Keycloak-Tokens (siehe
authentication.KeycloakJWTAuthentication), fuer Tests deshalb per
force_authenticate umgangen statt ein echtes JWT zu bauen. HasPlayerRole
(permissions.py) liest die Realm-Rolle aus request.auth -- deshalb wird bei
force_authenticate zusaetzlich ein token-Objekt mitgegeben, das sich wie
ein validiertes JWT mit "realm_access.roles" verhaelt.
"""

import asyncio
import base64
import json
import unittest
from unittest import mock

from channels.db import database_sync_to_async
from channels.testing import WebsocketCommunicator

from django.conf import settings
from django.contrib.auth import get_user_model
from rest_framework import status
from django.test import TransactionTestCase
from rest_framework.test import APITestCase

from .models import (
    Army,
    EntityProfile,
    Equipment,
    Game,
    GameServer,
    GameStatus,
    Player,
    Species,
    Unit,
    UnitType,
    Weapon,
)
from .serializers import ArmyWriteSerializer

try:
    import jsonschema
except ImportError:  # wie die ctest-Schema-Tests: ohne Werkzeug entfaellt nur der Test
    jsonschema = None

User = get_user_model()

# Das Schema der Spieldaten beim Start (siehe instance_client.game_start_document).
SPIELSTART_SCHEMA = settings.BASE_DIR.parent / "game" / "schema" / "spielstart.schema.json"

# Reicht als Ersatz fuer ein echtes, validiertes Keycloak-Token in Tests --
# HasPlayerRole liest nur request.auth.get("realm_access", {})["roles"]
# (siehe permissions._realm_roles), ein einfaches dict genuegt dafuer.
PLAYER_TOKEN = {"realm_access": {"roles": ["Player"]}}

# Analog zu PLAYER_TOKEN, aber fuer HasAdminRole (permissions.py) --
# steht fuer ein Token eines Nutzers mit der Realm-Rolle "Radish-Admin"
# (siehe docker/keycloak/realm-radish.json, User "radish-admin").
ADMIN_TOKEN = {"realm_access": {"roles": ["Radish-Admin"]}}


class ServerListTests(APITestCase):
    def setUp(self):
        self.user = User.objects.create(username="technischer-server-user")
        self.client.force_authenticate(user=self.user)

    def test_list_returns_all_registered_servers(self):
        GameServer.objects.create(name="server-a", address="10.0.0.1", port=7000)
        GameServer.objects.create(
            name="server-b", address="10.0.0.2", port=7001, is_occupied=True
        )

        response = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        names = [entry["name"] for entry in response.data["servers"]]
        self.assertEqual(names, ["server-a", "server-b"])
        occupied_by_name = {
            entry["name"]: entry["is_occupied"] for entry in response.data["servers"]
        }
        self.assertEqual(occupied_by_name, {"server-a": False, "server-b": True})

    def test_list_is_empty_without_registered_servers(self):
        response = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["servers"], [])

    def test_get_by_name_returns_single_server(self):
        GameServer.objects.create(name="server-a", address="10.0.0.1", port=7000)

        response = self.client.get("/api/servers/server-a/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["name"], "server-a")
        self.assertEqual(response.data["port"], 7000)

    def test_get_by_unknown_name_returns_404(self):
        response = self.client.get("/api/servers/does-not-exist/")

        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)

    def test_list_requires_authentication(self):
        self.client.force_authenticate(user=None)

        response = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_401_UNAUTHORIZED)


class AdminServerListTests(APITestCase):
    """
    Deckt admin_views.AdminServerListView ab: nur mit der Realm-Rolle
    "Radish-Admin" erreichbar, sonst 401 (kein Token) bzw. 403 (Token ohne
    diese Rolle, z.B. ein normaler Player).
    """

    def setUp(self):
        self.admin_user = User.objects.create(username="radish-admin-user")

    def test_requires_authentication(self):
        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_401_UNAUTHORIZED)

    def test_player_role_is_not_sufficient(self):
        self.client.force_authenticate(user=self.admin_user, token=PLAYER_TOKEN)

        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)

    def test_admin_role_can_list_servers(self):
        GameServer.objects.create(name="server-a", address="10.0.0.1", port=7000)
        GameServer.objects.create(
            name="server-b", address="10.0.0.2", port=7001, is_occupied=True
        )
        self.client.force_authenticate(user=self.admin_user, token=ADMIN_TOKEN)

        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        names = [entry["name"] for entry in response.data["servers"]]
        self.assertEqual(names, ["server-a", "server-b"])


class ArmyTestCase(APITestCase):
    """
    Gemeinsamer Katalog fuer die Armee-Tests: die Spezies "Menschen" (aus
    der Daten-Migration 0009) mit einem Trupp aus Soldaten und genau einem
    Anfuehrer, dazu eine zweite Spezies, deren Eintraege in einer Armee der
    Menschen nichts verloren haben.
    """

    def setUp(self):
        self.user = User.objects.create(username="army-user")
        self.client.force_authenticate(user=self.user, token=PLAYER_TOKEN)

        self.humans = Species.objects.get(name="Menschen")
        self.squad = UnitType.objects.create(
            species=self.humans,
            name="Trupp",
            min_entities=2,
            max_entities=10,
            can_capture_objectives=True,
            movement=6,
        )
        self.soldier = EntityProfile.objects.create(
            unit_type=self.squad, name="Soldat", health=1, armor=3, strength=3, accuracy=4,
            equipment_slots=1, weapon_slots=2, cost=10, min_count=1, max_count=9,
        )
        self.leader = EntityProfile.objects.create(
            unit_type=self.squad, name="Anfuehrer", health=2, armor=3, strength=4, accuracy=3,
            equipment_slots=2, weapon_slots=1, heavy_weapon_slots=1, super_heavy_weapon_slots=1,
            cost=20, min_count=1, max_count=1,
        )
        self.scout_team = UnitType.objects.create(
            species=self.humans, name="Spaeher", min_entities=1, max_entities=3, movement=8
        )
        self.scout = EntityProfile.objects.create(
            unit_type=self.scout_team, name="Spaeher", health=1, armor=2, strength=3, accuracy=3,
            equipment_slots=0, weapon_slots=1, cost=8,
        )
        self.truck_type = UnitType.objects.create(
            species=self.humans, name="Transporter", min_entities=1, max_entities=1,
            transport_capacity=2, movement=12,
        )
        self.truck = EntityProfile.objects.create(
            unit_type=self.truck_type, name="Lastwagen", health=8, armor=5, strength=5, accuracy=5,
            equipment_slots=0, weapon_slots=0, cost=50,
        )
        self.rifle = Weapon.objects.create(
            species=self.humans, name="Gewehr", shots=1, strength=3, max_range=24,
            armor_penetration=0, cost=1,
        )
        self.cannon = Weapon.objects.create(
            species=self.humans, name="Kanone", weapon_class="heavy", shots=1,
            strength=9, max_range=48, armor_penetration=3, cost=15,
        )
        self.battle_cannon = Weapon.objects.create(
            species=self.humans, name="Kampfgeschuetz", weapon_class="super_heavy", shots=1,
            strength=12, min_range=6, max_range=72, armor_penetration=5, cost=40,
        )
        self.medkit = Equipment.objects.create(species=self.humans, name="Medipack", cost=3)

        self.aliens = Species.objects.create(name="Fremde")
        self.alien_weapon = Weapon.objects.create(
            species=self.aliens, name="Strahler", shots=2, strength=4, max_range=18,
            armor_penetration=1, cost=2,
        )

    def squad_payload(self, soldiers=1, heavy=(), super_heavy=(), **overrides):
        """Trupp aus Anfuehrer (traegt heavy/super_heavy) und soldiers Soldaten."""
        unit = {
            "unit_type_id": self.squad.id,
            "entities": [
                {
                    "profile_id": self.leader.id,
                    "weapon_ids": [self.rifle.id],
                    "heavy_weapon_ids": list(heavy),
                    "super_heavy_weapon_ids": list(super_heavy),
                    "equipment_ids": [self.medkit.id],
                },
            ]
            + [
                {"profile_id": self.soldier.id, "weapon_ids": [self.rifle.id, self.rifle.id]}
                for _ in range(soldiers)
            ],
        }
        unit.update(overrides)
        return unit

    def army_payload(self, units=None, name="Erste Kompanie"):
        return {
            "name": name,
            "species_id": self.humans.id,
            "units": [self.squad_payload()] if units is None else units,
        }

    def create_army(self, payload=None):
        return self.client.post(
            "/api/client/armies/", payload or self.army_payload(), format="json"
        )


class ArmyCrudTests(ArmyTestCase):
    def test_catalog_lists_species_with_entries(self):
        response = self.client.get("/api/client/catalog/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        humans = next(s for s in response.data["species"] if s["name"] == "Menschen")
        self.assertEqual(
            sorted(t["name"] for t in humans["unit_types"]), ["Spaeher", "Transporter", "Trupp"]
        )
        squad = next(t for t in humans["unit_types"] if t["name"] == "Trupp")
        self.assertEqual(len(squad["profiles"]), 2)
        leader = next(p for p in squad["profiles"] if p["name"] == "Anfuehrer")
        self.assertEqual(leader["accuracy"], 3)
        self.assertEqual(len(humans["weapons"]), 3)
        battle_cannon = next(w for w in humans["weapons"] if w["name"] == "Kampfgeschuetz")
        self.assertEqual(battle_cannon["weapon_class"], "super_heavy")
        self.assertEqual((battle_cannon["min_range"], battle_cannon["max_range"]), (6, 72))
        self.assertEqual((leader["heavy_weapon_slots"], leader["super_heavy_weapon_slots"]), (1, 1))
        self.assertNotIn("heavy_weapon_slots", squad)
        truck = next(t for t in humans["unit_types"] if t["name"] == "Transporter")
        self.assertEqual(truck["transport_capacity"], 2)
        self.assertEqual(squad["transport_capacity"], 0)
        self.assertTrue(squad["can_capture_objectives"])
        self.assertEqual((squad["movement"], truck["movement"]), (6, 12))
        self.assertFalse(truck["can_capture_objectives"])
        self.assertEqual(humans["equipment"][0]["name"], "Medipack")

    def test_create_and_read_army(self):
        response = self.create_army()

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        army_id = response.data["id"]
        response = self.client.get(f"/api/client/armies/{army_id}/")
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["name"], "Erste Kompanie")
        self.assertEqual(response.data["species_name"], "Menschen")
        entities = response.data["units"][0]["entities"]
        self.assertEqual(entities[0]["profile_id"], self.leader.id)
        self.assertEqual(entities[0]["equipment_ids"], [self.medkit.id])
        self.assertEqual(entities[1]["weapon_ids"], [self.rifle.id, self.rifle.id])

    def test_costs_are_summed_from_catalog(self):
        # Anfuehrer 20 + Gewehr 1 + Medipack 3 + seine Kanone 15, zwei Soldaten
        # je 10 + 2x Gewehr 1.
        response = self.create_army(
            self.army_payload(
                units=[self.squad_payload(soldiers=2, heavy=[self.cannon.id])]
            )
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        unit = response.data["units"][0]
        self.assertEqual([e["total_cost"] for e in unit["entities"]], [39, 12, 12])
        self.assertEqual(unit["total_cost"], 63)
        self.assertEqual(response.data["total_cost"], 63)
        listed = self.client.get("/api/client/armies/").data["armies"]
        self.assertEqual(listed[0]["total_cost"], 63)

    def test_cost_follows_catalog_price(self):
        army_id = self.create_army().data["id"]
        self.rifle.cost = 5
        self.rifle.save(update_fields=["cost"])

        response = self.client.get(f"/api/client/armies/{army_id}/")

        # Anfuehrer 20 + 5 + 3, Soldat 10 + 2x 5.
        self.assertEqual(response.data["total_cost"], 48)

    def test_list_query_count_does_not_grow_with_armies(self):
        for number in range(3):
            self.create_army(
                self.army_payload(name=f"Kompanie {number}", units=[self.squad_payload(soldiers=3)])
            )

        # Armeen, Einheiten, Entitaeten, Waffen- und Ausruestungsslots -- je
        # eine Abfrage, egal wie viele Armeen es gibt.
        with self.assertNumQueries(5):
            response = self.client.get("/api/client/armies/")

        self.assertEqual(len(response.data["armies"]), 3)

    def test_create_army_without_units(self):
        response = self.create_army(self.army_payload(units=[]))

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data["units"], [])

    def test_list_contains_only_own_armies(self):
        self.create_army()
        other_player = Player.objects.create(user=User.objects.create(username="other"))
        Army.objects.create(player=other_player, name="Fremde Armee", species=self.humans)

        response = self.client.get("/api/client/armies/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(
            [(a["name"], a["unit_count"]) for a in response.data["armies"]],
            [("Erste Kompanie", 1)],
        )

    def test_list_without_player_is_empty_and_creates_no_player(self):
        response = self.client.get("/api/client/armies/")

        self.assertEqual(response.data["armies"], [])
        self.assertFalse(Player.objects.filter(user=self.user).exists())

    def test_update_replaces_units(self):
        army_id = self.create_army().data["id"]
        scouts = {
            "unit_type_id": self.scout_team.id,
            "entities": [{"profile_id": self.scout.id}],
        }

        response = self.client.put(
            f"/api/client/armies/{army_id}/",
            self.army_payload(units=[scouts, self.squad_payload(soldiers=3)], name="Neu"),
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["name"], "Neu")
        self.assertEqual(
            [u["unit_type_id"] for u in response.data["units"]],
            [self.scout_team.id, self.squad.id],
        )
        self.assertEqual(Unit.objects.filter(army_id=army_id).count(), 2)

    def test_delete_army(self):
        army_id = self.create_army().data["id"]

        response = self.client.delete(f"/api/client/armies/{army_id}/")

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        self.assertFalse(Army.objects.filter(pk=army_id).exists())
        self.assertFalse(Unit.objects.filter(army_id=army_id).exists())

    def test_foreign_army_is_not_found(self):
        other_player = Player.objects.create(user=User.objects.create(username="other"))
        army = Army.objects.create(player=other_player, name="Fremde Armee", species=self.humans)
        url = f"/api/client/armies/{army.id}/"

        self.assertEqual(self.client.get(url).status_code, status.HTTP_404_NOT_FOUND)
        self.assertEqual(
            self.client.put(url, self.army_payload(), format="json").status_code,
            status.HTTP_404_NOT_FOUND,
        )
        self.assertEqual(self.client.delete(url).status_code, status.HTTP_404_NOT_FOUND)
        self.assertTrue(Army.objects.filter(pk=army.id).exists())

    def test_duplicate_name_is_rejected_only_for_same_player(self):
        self.create_army()
        other_player = Player.objects.create(user=User.objects.create(username="other"))
        Army.objects.create(player=other_player, name="Zweite Kompanie", species=self.humans)

        self.assertEqual(self.create_army().status_code, status.HTTP_400_BAD_REQUEST)
        self.assertEqual(
            self.create_army(self.army_payload(name="Zweite Kompanie")).status_code,
            status.HTTP_201_CREATED,
        )

    def test_update_may_keep_own_name(self):
        army_id = self.create_army().data["id"]

        response = self.client.put(
            f"/api/client/armies/{army_id}/", self.army_payload(), format="json"
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK)

    def test_requires_player_role(self):
        self.client.force_authenticate(user=self.user, token=ADMIN_TOKEN)

        self.assertEqual(
            self.client.get("/api/client/armies/").status_code, status.HTTP_403_FORBIDDEN
        )


class ArmyValidationTests(ArmyTestCase):
    """
    Die Regeln aus serializers.ArmyWriteSerializer.validate -- jede
    Verletzung muss mit 400 abgelehnt werden, ohne etwas anzulegen.
    """

    def assertRejected(self, units):
        response = self.create_army(self.army_payload(units=units))

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST, response.data)
        self.assertFalse(Army.objects.exists())

    def test_unit_without_entities(self):
        self.assertRejected([self.squad_payload(entities=[])])

    def test_too_few_entities(self):
        self.assertRejected([self.squad_payload(soldiers=0)])

    def test_more_than_ten_entities(self):
        self.assertRejected([self.squad_payload(soldiers=10)])

    def test_ten_entities_are_allowed(self):
        response = self.create_army(self.army_payload(units=[self.squad_payload(soldiers=9)]))

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)

    def test_more_entities_than_unit_type_allows(self):
        scouts = {
            "unit_type_id": self.scout_team.id,
            "entities": [{"profile_id": self.scout.id}] * 4,
        }
        self.assertRejected([scouts])

    def test_profile_count_limits(self):
        unit = self.squad_payload()
        unit["entities"].append(dict(unit["entities"][0]))  # zweiter Anfuehrer
        self.assertRejected([unit])

    def test_profile_of_other_unit_type(self):
        unit = self.squad_payload()
        unit["entities"][1]["profile_id"] = self.scout.id
        self.assertRejected([unit])

    def test_too_many_weapons(self):
        unit = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.rifle.id] * 3
        self.assertRejected([unit])

    def test_too_much_equipment(self):
        unit = self.squad_payload()
        unit["entities"][1]["equipment_ids"] = [self.medkit.id] * 2
        self.assertRejected([unit])

    def test_heavy_weapon_in_entity_slot(self):
        unit = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.cannon.id]
        self.assertRejected([unit])

    def test_heavy_weapon_on_entity(self):
        response = self.create_army(
            self.army_payload(units=[self.squad_payload(heavy=[self.cannon.id])])
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        leader, soldier = response.data["units"][0]["entities"]
        self.assertEqual(leader["heavy_weapon_ids"], [self.cannon.id])
        self.assertEqual(leader["super_heavy_weapon_ids"], [])
        self.assertEqual(leader["weapon_ids"], [self.rifle.id])
        self.assertEqual(soldier["heavy_weapon_ids"], [])
        self.assertNotIn("heavy_weapon_ids", response.data["units"][0])

    def test_heavy_and_super_heavy_weapons_together(self):
        response = self.create_army(
            self.army_payload(
                units=[
                    self.squad_payload(
                        heavy=[self.cannon.id],
                        super_heavy=[self.battle_cannon.id],
                    )
                ]
            )
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        unit = response.data["units"][0]
        leader = unit["entities"][0]
        self.assertEqual(leader["heavy_weapon_ids"], [self.cannon.id])
        self.assertEqual(leader["super_heavy_weapon_ids"], [self.battle_cannon.id])
        # Anfuehrer 20 + 1 + 3 + Kanone 15 + Kampfgeschuetz 40, Soldat 10 + 2.
        self.assertEqual(leader["total_cost"], 79)
        self.assertEqual(unit["total_cost"], 91)

    def test_several_heavy_weapon_slots(self):
        self.leader.heavy_weapon_slots = 2
        self.leader.save(update_fields=["heavy_weapon_slots"])

        response = self.create_army(
            self.army_payload(
                units=[self.squad_payload(heavy=[self.cannon.id, self.cannon.id])]
            )
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(
            response.data["units"][0]["entities"][0]["heavy_weapon_ids"],
            [self.cannon.id, self.cannon.id],
        )

    def test_more_heavy_weapons_than_slots(self):
        self.assertRejected([self.squad_payload(heavy=[self.cannon.id] * 2)])

    def test_normal_weapon_as_heavy_weapon(self):
        self.assertRejected([self.squad_payload(heavy=[self.rifle.id])])

    def test_super_heavy_weapon_in_heavy_slot(self):
        self.assertRejected([self.squad_payload(heavy=[self.battle_cannon.id])])

    def test_heavy_weapon_in_super_heavy_slot(self):
        self.assertRejected([self.squad_payload(super_heavy=[self.cannon.id])])

    def test_super_heavy_weapon_in_entity_slot(self):
        unit = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.battle_cannon.id]
        self.assertRejected([unit])

    def weapon_carrier_squad(self, carriers, heavy_per_carrier=1):
        """Trupp mit Anfuehrer und `carriers` Soldaten, die je schwere Waffen tragen."""
        self.soldier.heavy_weapon_slots = 1
        self.soldier.save(update_fields=["heavy_weapon_slots"])
        unit = self.squad_payload(soldiers=carriers)
        for entity in unit["entities"][1:]:
            entity["heavy_weapon_ids"] = [self.cannon.id] * heavy_per_carrier
        return unit

    def test_unit_limit_for_heavy_weapons(self):
        self.squad.max_heavy_weapons = 2
        self.squad.save(update_fields=["max_heavy_weapons"])

        self.assertRejected([self.weapon_carrier_squad(carriers=3)])

    def test_unit_limit_can_be_reached(self):
        self.squad.max_heavy_weapons = 2
        self.squad.save(update_fields=["max_heavy_weapons"])

        response = self.create_army(self.army_payload(units=[self.weapon_carrier_squad(carriers=2)]))

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)

    def test_without_unit_limit_only_slots_count(self):
        response = self.create_army(self.army_payload(units=[self.weapon_carrier_squad(carriers=5)]))

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)

    def test_unit_limit_zero_forbids_super_heavy(self):
        self.squad.max_super_heavy_weapons = 0
        self.squad.save(update_fields=["max_super_heavy_weapons"])

        self.assertRejected([self.squad_payload(super_heavy=[self.battle_cannon.id])])

    def test_heavy_weapon_on_profile_without_heavy_slot(self):
        unit = self.squad_payload()
        unit["entities"][1]["heavy_weapon_ids"] = [self.cannon.id]
        self.assertRejected([unit])

    def test_weapon_of_other_species(self):
        unit = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.alien_weapon.id]
        self.assertRejected([unit])

    def test_unit_type_of_other_species(self):
        response = self.create_army(
            {"name": "Mischmasch", "species_id": self.aliens.id, "units": [self.squad_payload()]}
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_unknown_weapon(self):
        unit = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [999999]
        self.assertRejected([unit])

    def test_failed_update_keeps_old_units(self):
        army_id = self.create_army().data["id"]

        response = self.client.put(
            f"/api/client/armies/{army_id}/",
            self.army_payload(units=[self.squad_payload(soldiers=0)]),
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertEqual(Unit.objects.filter(army_id=army_id).count(), 1)


class CatalogAdminTests(ArmyTestCase):
    """
    admin_views: Katalog-Pflege nur mit "Radish-Admin"; verwendete Eintraege
    koennen weder geloescht noch in ihren Strukturfeldern geaendert werden,
    damit bestehende Armeen gueltig bleiben.
    """

    def setUp(self):
        super().setUp()
        self.admin_user = User.objects.create(username="catalog-admin")
        self.client.force_authenticate(user=self.admin_user, token=ADMIN_TOKEN)

    def weapon_payload(self, **overrides):
        payload = {
            "species_id": self.humans.id,
            "name": "Pistole",
            "weapon_class": "standard",
            "shots": 1,
            "strength": 3,
            "min_range": 0,
            "max_range": 12,
            "armor_penetration": 0,
            "cost": 1,
        }
        payload.update(overrides)
        return payload

    def put_weapon(self, weapon, **overrides):
        payload = self.weapon_payload(
            name=weapon.name, weapon_class=weapon.weapon_class, shots=weapon.shots,
            strength=weapon.strength, min_range=weapon.min_range, max_range=weapon.max_range,
            armor_penetration=weapon.armor_penetration, cost=weapon.cost,
        )
        payload.update(overrides)
        return self.client.put(
            f"/api/admin/catalog/weapons/{weapon.id}/", payload, format="json"
        )

    def use_catalog_in_army(self):
        """Eine Armee mit Trupp, Gewehr, Medipack und Kanone -- alles in Verwendung."""
        player = Player.objects.create(user=self.user)
        army = Army.objects.create(player=player, name="Belegt", species=self.humans)
        serializer = ArmyWriteSerializer(
            army,
            data=self.army_payload(
                units=[self.squad_payload(heavy=[self.cannon.id])], name="Belegt"
            ),
            context={"player": player},
        )
        serializer.is_valid(raise_exception=True)
        return serializer.save()

    def test_player_role_is_not_sufficient(self):
        self.client.force_authenticate(user=self.user, token=PLAYER_TOKEN)

        self.assertEqual(
            self.client.get("/api/admin/catalog/weapons/").status_code,
            status.HTTP_403_FORBIDDEN,
        )
        self.assertEqual(
            self.client.post(
                "/api/admin/catalog/weapons/", self.weapon_payload(), format="json"
            ).status_code,
            status.HTTP_403_FORBIDDEN,
        )
        self.assertFalse(Weapon.objects.filter(name="Pistole").exists())

    def test_create_species_and_reject_duplicate(self):
        response = self.client.post("/api/admin/catalog/species/", {"name": "Orks"}, format="json")
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)

        response = self.client.post("/api/admin/catalog/species/", {"name": "Orks"}, format="json")
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_create_unit_type_with_profiles(self):
        response = self.client.post(
            "/api/admin/catalog/unit-types/",
            {"species_id": self.humans.id, "name": "Garde", "min_entities": 1,
             "max_entities": 5, "movement": 6},
            format="json",
        )
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data["profiles"], [])
        self.assertFalse(response.data["in_use"])
        unit_type_id = response.data["id"]

        response = self.client.post(
            "/api/admin/catalog/profiles/",
            {"unit_type_id": unit_type_id, "name": "Gardist", "health": 1, "armor": 4,
             "strength": 3, "accuracy": 4, "equipment_slots": 1, "weapon_slots": 1, "cost": 12,
             "min_count": 1, "max_count": 5},
            format="json",
        )
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)

        response = self.client.get(f"/api/admin/catalog/unit-types/{unit_type_id}/")
        self.assertEqual([p["name"] for p in response.data["profiles"]], ["Gardist"])

    def test_list_filters_by_species(self):
        response = self.client.get(f"/api/admin/catalog/weapons/?species={self.aliens.id}")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual([w["name"] for w in response.data["weapons"]], ["Strahler"])

    def test_search_by_name_ignores_case(self):
        response = self.client.get("/api/admin/catalog/weapons/?search=GEW")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual([w["name"] for w in response.data["weapons"]], ["Gewehr"])
        self.assertEqual(response.data["weapons"][0]["species_name"], "Menschen")

    def test_search_combines_with_species_filter(self):
        Weapon.objects.create(
            species=self.aliens, name="Gewehr der Fremden", shots=1, strength=3,
            max_range=24, armor_penetration=0, cost=1,
        )

        response = self.client.get(
            f"/api/admin/catalog/weapons/?search=gewehr&species={self.aliens.id}"
        )

        self.assertEqual([w["name"] for w in response.data["weapons"]], ["Gewehr der Fremden"])

    def test_profiles_carry_context_and_filter_by_species(self):
        response = self.client.get(
            f"/api/admin/catalog/profiles/?species={self.humans.id}&search=anf"
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        [profile] = response.data["profiles"]
        self.assertEqual(profile["name"], "Anfuehrer")
        self.assertEqual(profile["unit_type_name"], "Trupp")
        self.assertEqual(profile["species_name"], "Menschen")

        response = self.client.get(f"/api/admin/catalog/profiles/?species={self.aliens.id}")
        self.assertEqual(response.data["profiles"], [])

    def test_unit_type_list_query_count_is_constant(self):
        # Unit types, ihre Profile (samt Einheitentyp und Spezies) -- zwei
        # Abfragen, egal wie viele Typen und Profile es gibt.
        with self.assertNumQueries(2):
            response = self.client.get("/api/admin/catalog/unit-types/")

        self.assertEqual(len(response.data["unit_types"]), 3)

    def test_weapon_min_range_above_max_range(self):
        response = self.client.post(
            "/api/admin/catalog/weapons/",
            self.weapon_payload(min_range=20, max_range=12),
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("min_range", response.data)

    def test_weapon_min_range_defaults_to_zero(self):
        payload = self.weapon_payload()
        del payload["min_range"]

        response = self.client.post("/api/admin/catalog/weapons/", payload, format="json")

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(response.data["min_range"], 0)

    def test_unknown_weapon_class_is_rejected(self):
        response = self.client.post(
            "/api/admin/catalog/weapons/",
            self.weapon_payload(weapon_class="ultra"),
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_filter_weapons_by_class(self):
        response = self.client.get("/api/admin/catalog/weapons/?weapon_class=super_heavy")

        self.assertEqual([w["name"] for w in response.data["weapons"]], ["Kampfgeschuetz"])

    def test_invalid_filter_is_rejected(self):
        response = self.client.get("/api/admin/catalog/weapons/?species=abc")

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_duplicate_weapon_name_within_species(self):
        response = self.client.post(
            "/api/admin/catalog/weapons/", self.weapon_payload(name="Gewehr"), format="json"
        )
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

        response = self.client.post(
            "/api/admin/catalog/weapons/",
            self.weapon_payload(name="Gewehr", species_id=self.aliens.id),
            format="json",
        )
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)

    def test_species_of_entry_cannot_change(self):
        response = self.put_weapon(self.rifle, species_id=self.aliens.id)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.rifle.refresh_from_db()
        self.assertEqual(self.rifle.species, self.humans)

    def test_used_weapon_can_be_changed(self):
        self.use_catalog_in_army()

        response = self.put_weapon(self.rifle, cost=4, strength=5, name="Sturmgewehr")

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertTrue(response.data["in_use"])
        self.assertEqual(response.data["cost"], 4)

    def test_heavy_weapon_on_unit_counts_as_used(self):
        self.use_catalog_in_army()

        response = self.client.get(f"/api/admin/catalog/weapons/{self.cannon.id}/")

        self.assertTrue(response.data["in_use"])

    def test_unused_weapon_can_change_structure(self):
        response = self.put_weapon(self.rifle, weapon_class="super_heavy")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertFalse(response.data["in_use"])

    def test_delete_used_entries_is_conflict(self):
        self.use_catalog_in_army()

        for url in [
            f"/api/admin/catalog/weapons/{self.rifle.id}/",
            f"/api/admin/catalog/equipment/{self.medkit.id}/",
            f"/api/admin/catalog/profiles/{self.leader.id}/",
            f"/api/admin/catalog/unit-types/{self.squad.id}/",
            f"/api/admin/catalog/species/{self.humans.id}/",
        ]:
            response = self.client.delete(url)
            self.assertEqual(response.status_code, status.HTTP_409_CONFLICT, url)

        self.assertTrue(Weapon.objects.filter(pk=self.rifle.pk).exists())
        self.assertTrue(UnitType.objects.filter(pk=self.squad.pk).exists())

    def test_delete_unused_entry(self):
        response = self.client.delete(f"/api/admin/catalog/weapons/{self.alien_weapon.id}/")

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        self.assertFalse(Weapon.objects.filter(pk=self.alien_weapon.pk).exists())

    def test_species_with_entries_cannot_be_deleted(self):
        response = self.client.delete(f"/api/admin/catalog/species/{self.aliens.id}/")

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)

    def test_used_unit_type_can_be_changed(self):
        self.use_catalog_in_army()

        response = self.client.put(
            f"/api/admin/catalog/unit-types/{self.squad.id}/",
            {"species_id": self.humans.id, "name": "Grosser Trupp", "min_entities": 2,
             "max_entities": 10, "max_heavy_weapons": 2, "movement": 5},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertTrue(response.data["in_use"])
        self.assertTrue(all(p["in_use"] for p in response.data["profiles"]))
        self.assertEqual(response.data["max_heavy_weapons"], 2)

    def test_unit_type_min_above_max(self):
        response = self.client.post(
            "/api/admin/catalog/unit-types/",
            {"species_id": self.humans.id, "name": "Kaputt", "min_entities": 5, "max_entities": 2,
             "movement": 6},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_unit_type_rejects_more_than_ten_entities(self):
        response = self.client.post(
            "/api/admin/catalog/unit-types/",
            {"species_id": self.humans.id, "name": "Horde", "max_entities": 11, "movement": 6},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def profile_payload(self, **overrides):
        payload = {"unit_type_id": self.squad.id, "name": "Sanitaeter", "health": 1,
                   "armor": 3, "strength": 3, "accuracy": 4, "equipment_slots": 2,
                   "weapon_slots": 1,
                   "cost": 15, "min_count": 0, "max_count": 1}
        payload.update(overrides)
        return payload

    def test_profile_requires_accuracy(self):
        payload = self.profile_payload()
        del payload["accuracy"]

        response = self.client.post("/api/admin/catalog/profiles/", payload, format="json")

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("accuracy", response.data)

    def test_profile_min_above_max(self):
        response = self.client.post(
            "/api/admin/catalog/profiles/",
            self.profile_payload(min_count=2, max_count=1),
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_profile_minimums_must_fit_unit_type(self):
        # Soldat 1 + Anfuehrer 1 + 9 > max_entities 10.
        response = self.client.post(
            "/api/admin/catalog/profiles/",
            self.profile_payload(min_count=9, max_count=9),
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_new_required_profile_on_used_unit_type(self):
        self.use_catalog_in_army()

        response = self.client.post(
            "/api/admin/catalog/profiles/", self.profile_payload(min_count=1), format="json"
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertTrue(response.data["in_use"])

    def test_unit_type_can_capture_objectives(self):
        url = "/api/admin/catalog/unit-types/"
        payload = {"species_id": self.humans.id, "name": "Besatzer", "min_entities": 1,
                   "max_entities": 5, "movement": 6}

        response = self.client.post(url, payload, format="json")
        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertFalse(response.data["can_capture_objectives"])

        response = self.client.put(
            f"{url}{response.data['id']}/",
            dict(payload, can_capture_objectives=True),
            format="json",
        )
        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertTrue(response.data["can_capture_objectives"])

    def test_unit_type_requires_movement(self):
        response = self.client.post(
            "/api/admin/catalog/unit-types/",
            {"species_id": self.humans.id, "name": "Ohne", "min_entities": 1, "max_entities": 3},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("movement", response.data)

    def test_unit_limits_default_to_none(self):
        response = self.client.post(
            "/api/admin/catalog/unit-types/",
            {"species_id": self.humans.id, "name": "Neu", "min_entities": 1, "max_entities": 3,
             "movement": 6},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertIsNone(response.data["max_heavy_weapons"])
        self.assertIsNone(response.data["max_super_heavy_weapons"])

    def test_catalog_changes_reach_player_catalog(self):
        self.client.post(
            "/api/admin/catalog/equipment/",
            {"species_id": self.humans.id, "name": "Granate", "cost": 2},
            format="json",
        )
        self.client.force_authenticate(user=self.user, token=PLAYER_TOKEN)

        response = self.client.get("/api/client/catalog/")

        humans = next(s for s in response.data["species"] if s["name"] == "Menschen")
        self.assertIn("Granate", [e["name"] for e in humans["equipment"]])


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
            name="server-a", address="10.0.0.1", port=7000, control_url="http://instanz:8000"
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


class GameArmyTests(GameTestCase):
    """
    Erstellen und Beitreten verlangen eine eigene Armee mit mindestens einer
    Einheit, die hoechstens das Punktelimit des Spiels kostet (siehe
    client_views._army_for_game). Dazu die Matchmaking-Regel aus
    GameJoinView.already_in_game: wer schon in einem Spiel ist, darf keinem
    weiteren beitreten -- auch nicht dem eigenen.

    Die Standard-Armee (army_payload: Anfuehrer + Soldat) kostet 36 Punkte.
    """

    def test_create_game_with_army(self):
        response = self.create_game(points_limit=500)

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(response.data["points_limit"], 500)
        self.assertEqual(response.data["host_army_name"], "Erste Kompanie")
        game = Game.objects.get(name="spiel")
        self.assertEqual(game.host_army, self.army)

    def test_create_game_requires_points_limit_and_army(self):
        response = self.client.post(
            "/api/client/game/", {"name": "spiel", "password": "geheim"}, format="json"
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("points_limit", response.data)
        self.assertIn("army_id", response.data)

    def test_create_game_rejects_army_over_limit(self):
        response = self.create_game(points_limit=35)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertFalse(Game.objects.exists())
        self.server.refresh_from_db()
        self.assertFalse(self.server.is_occupied)

    def test_army_exactly_at_limit_is_allowed(self):
        self.assertEqual(self.create_game(points_limit=36).status_code, status.HTTP_201_CREATED)

    def test_create_game_rejects_foreign_army(self):
        other = Player.objects.create(user=User.objects.create(username="other"))

        response = self.create_game(army_id=self.army_for(other).id)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_create_game_rejects_empty_army(self):
        empty = self.army_for(self.player, name="Leer", units=[])

        response = self.create_game(army_id=empty.id)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_join_with_army(self):
        game = self.open_game()

        response = self.join(game)

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        game.refresh_from_db()
        self.assertEqual(game.second_player, self.player)
        self.assertEqual(game.second_player_army, self.army)

    def test_join_rejects_army_over_game_limit(self):
        game = self.open_game(points_limit=30)

        response = self.join(game)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        game.refresh_from_db()
        self.assertIsNone(game.second_player_id)

    def test_join_rejects_foreign_army(self):
        game = self.open_game()

        response = self.join(game, army_id=game.host_army_id)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_host_cannot_join_own_game(self):
        self.create_game()
        game = Game.objects.get(name="spiel")

        response = self.join(game)

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        game.refresh_from_db()
        self.assertIsNone(game.second_player_id)

    def test_second_player_cannot_join_a_second_game(self):
        game = self.open_game()
        self.join(game)
        other_server = GameServer.objects.create(name="server-b", address="10.0.0.2", port=7001)
        other_host = Player.objects.create(user=User.objects.create(username="other-host"))
        other_game = Game.objects.create(
            name="anderes", password="geheim", server=other_server, host=other_host,
            points_limit=100, host_army=self.army_for(other_host),
        )

        response = self.join(other_game)

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        other_game.refresh_from_db()
        self.assertIsNone(other_game.second_player_id)

    def test_game_list_shows_points_limit(self):
        self.open_game(points_limit=750)

        response = self.client.get("/api/client/games/")

        self.assertEqual(response.data["games"][0]["points_limit"], 750)

    def test_second_player_takes_army_when_host_leaves(self):
        game = self.open_game()
        self.join(game)
        host_user = game.host.user
        self.client.force_authenticate(user=host_user, token=PLAYER_TOKEN)

        response = self.client.post("/api/client/game/leave/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        game.refresh_from_db()
        self.assertEqual((game.host, game.host_army), (self.player, self.army))
        self.assertIsNone(game.second_player_army)

    def test_second_player_leaving_clears_army(self):
        game = self.open_game()
        self.join(game)

        self.client.post("/api/client/game/leave/")

        game.refresh_from_db()
        self.assertIsNone(game.second_player_army)

    def test_army_in_game_cannot_be_changed_or_deleted(self):
        self.create_game()
        url = f"/api/client/armies/{self.army.id}/"

        response = self.client.put(url, self.army_payload(), format="json")
        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)

        response = self.client.delete(url)
        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        self.assertTrue(Army.objects.filter(pk=self.army.pk).exists())

    def test_army_is_free_again_after_game_ends(self):
        self.create_game()
        self.client.post("/api/client/game/leave/")

        response = self.client.delete(f"/api/client/armies/{self.army.id}/")

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)


@mock.patch("api.instance_client.cancel_game")
@mock.patch("api.instance_client.start_game")
class GameLobbyTests(GameTestCase):
    """
    Lobby vor dem Spielstart (siehe models.GameStatus): der Host startet
    von Hand, sobald ein zweiter Spieler da ist; die Instanz setzt das Spiel
    noch im selben Aufruf auf, danach laeuft es. Die Aufrufe an die Instanz
    (instance_client) sind gemockt.
    """

    def setUp(self):
        super().setUp()
        self.create_game()
        self.game = Game.objects.get(name="spiel")
        self.guest = Player.objects.create(user=User.objects.create(username="gast"))
        self.game.second_player = self.guest
        self.game.second_player_army = self.army_for(self.guest)
        self.game.save()

    def start(self):
        return self.client.post("/api/client/game/start/")

    def as_guest(self):
        self.client.force_authenticate(user=self.guest.user, token=PLAYER_TOKEN)

    def current_status(self):
        return self.client.get("/api/client/game/current/").data["game"]["status"]

    def test_keycloak_username_is_taken_from_token(self, start_game, cancel_game):
        token = {**PLAYER_TOKEN, "preferred_username": "test-user"}
        self.client.force_authenticate(user=self.user, token=token)

        self.start()

        self.player.refresh_from_db()
        self.assertEqual(self.player.name, "test-user")

    def test_new_game_waits_in_lobby(self, start_game, cancel_game):
        self.assertEqual(self.current_status(), "lobby")
        start_game.assert_not_called()

    def test_is_host_tells_players_apart(self, start_game, cancel_game):
        self.assertTrue(self.client.get("/api/client/game/current/").data["game"]["is_host"])
        self.as_guest()
        self.assertFalse(self.client.get("/api/client/game/current/").data["game"]["is_host"])

    def test_host_starts_game(self, start_game, cancel_game):
        response = self.start()

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertEqual(response.data["status"], "running")
        start_game.assert_called_once()
        self.assertEqual(start_game.call_args.args[0].pk, self.game.pk)

    def test_guest_sees_running_game(self, start_game, cancel_game):
        self.start()
        self.as_guest()

        self.assertEqual(self.current_status(), "running")

    def test_only_host_can_start(self, start_game, cancel_game):
        self.as_guest()

        self.assertEqual(self.start().status_code, status.HTTP_403_FORBIDDEN)
        start_game.assert_not_called()

    def test_start_needs_second_player(self, start_game, cancel_game):
        self.game.second_player = None
        self.game.second_player_army = None
        self.game.save()

        self.assertEqual(self.start().status_code, status.HTTP_409_CONFLICT)
        start_game.assert_not_called()

    def test_start_only_once(self, start_game, cancel_game):
        self.start()

        self.assertEqual(self.start().status_code, status.HTTP_409_CONFLICT)
        start_game.assert_called_once()

    def test_failed_instance_call_stays_in_lobby(self, start_game, cancel_game):
        from .instance_client import InstanceError

        start_game.side_effect = InstanceError("nicht erreichbar")

        response = self.start()

        self.assertEqual(response.status_code, status.HTTP_502_BAD_GATEWAY)
        self.game.refresh_from_db()
        self.assertEqual(self.game.status, GameStatus.LOBBY)
        # Ohne aufgesetztes Spiel keine Codes.
        self.assertEqual(self.game.host_zucchini_code, "")
        self.assertEqual(self.game.second_player_zucchini_code, "")

    def test_start_hands_out_secret_codes(self, start_game, cancel_game):
        response = self.start()

        self.game.refresh_from_db()
        host_code = self.game.host_zucchini_code
        guest_code = self.game.second_player_zucchini_code
        self.assertRegex(host_code, r"^[0-9a-f]{16}$")
        self.assertRegex(guest_code, r"^[0-9a-f]{16}$")
        self.assertNotEqual(host_code, guest_code)

        # Die Instanz bekam sie schon gesetzt.
        started = start_game.call_args.args[0]
        self.assertEqual(started.host_zucchini_code, host_code)

        # Und kein Client bekommt sie zu sehen.
        self.assertNotIn(host_code, json.dumps(response.data))
        self.assertNotIn(guest_code, json.dumps(response.data))
        current = self.client.get("/api/client/game/current/").data
        self.assertNotIn(host_code, json.dumps(current))
        self.assertNotIn(guest_code, json.dumps(current))

    def test_leaving_in_lobby_keeps_game(self, start_game, cancel_game):
        self.as_guest()

        self.client.post("/api/client/game/leave/")

        self.game.refresh_from_db()
        self.assertIsNone(self.game.second_player)
        cancel_game.assert_not_called()

    def test_leaving_started_game_ends_it_for_both(self, start_game, cancel_game):
        self.start()
        self.as_guest()

        with self.captureOnCommitCallbacks(execute=True):
            response = self.client.post("/api/client/game/leave/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertFalse(Game.objects.exists())
        self.server.refresh_from_db()
        self.assertFalse(self.server.is_occupied)
        cancel_game.assert_called_once()
        self.client.force_authenticate(user=self.user, token=PLAYER_TOKEN)
        self.assertIsNone(self.client.get("/api/client/game/current/").data["game"])

    def test_cannot_join_started_game(self, start_game, cancel_game):
        self.game.second_player = None
        self.game.second_player_army = None
        self.game.status = GameStatus.RUNNING
        self.game.save()
        newcomer = Player.objects.create(user=User.objects.create(username="neu"))
        self.client.force_authenticate(user=newcomer.user, token=PLAYER_TOKEN)

        response = self.client.post(
            "/api/client/game/join/",
            {"name": "spiel", "password": "geheim", "army_id": self.army_for(newcomer).id},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)

    def test_game_list_only_shows_lobby_games(self, start_game, cancel_game):
        self.start()

        self.assertEqual(self.client.get("/api/client/games/").data["games"], [])


class InstanceClientTests(GameTestCase):
    """instance_client.start_game: Ziel-URL, Payload und Fehlerfaelle."""

    def setUp(self):
        super().setUp()
        self.game = self.open_game()
        self.join(self.game)
        self.game.refresh_from_db()

    @mock.patch("api.instance_client.requests.post")
    def test_start_posts_game_start_document(self, post):
        from . import instance_client

        post.return_value = mock.Mock(status_code=200)

        self.game.host_zucchini_code = "00000000000000aa"
        self.game.second_player_zucchini_code = "00000000000000bb"

        instance_client.start_game(self.game)

        url = post.call_args.args[0]
        self.assertEqual(url, "http://instanz:8000/api/instances/server-a/game/start/")
        sent = post.call_args.kwargs["json"]
        # Die Spieldaten nach dem Schema, die Codes daneben -- nicht darin.
        self.assertEqual(sent["spieldaten"], instance_client.game_start_document(self.game)["spieldaten"])
        self.assertEqual(sent["zugangscodes"], ["00000000000000aa", "00000000000000bb"])

    def test_document_carries_players_and_army_values(self):
        from . import instance_client

        self.game.host.name = "host-name"
        self.game.host.save()

        host, guest = instance_client.game_start_document(self.game)["spieldaten"]

        self.assertEqual((host["name"], host["kennung"]), ("host-name", self.game.host.identifier))
        # Ohne Keycloak-Namen ersatzweise die Kennung.
        self.assertEqual(guest["name"], self.player.identifier)
        self.assertEqual(guest["kennung"], self.player.identifier)
        army = guest["armee"]
        self.assertEqual((army["name"], army["spezies"]), ("Erste Kompanie", "Menschen"))
        unit = army["einheiten"][0]
        self.assertEqual(
            (unit["typ"], unit["bewegung"], unit["kann_ziele_einnehmen"]), ("Trupp", 6, True)
        )
        leader = next(e for e in unit["entitaeten"] if e["profil"] == "Anfuehrer")
        self.assertEqual(
            (leader["leben"], leader["ruestung"], leader["staerke"], leader["genauigkeit"]),
            (2, 3, 4, 3),
        )

    def test_document_query_count_is_constant(self):
        from . import instance_client

        game = Game.objects.select_related("host", "second_player").get(pk=self.game.pk)
        # Armeen samt Spezies, Einheiten mit Typ, Entitaeten mit Profil,
        # Waffen, Ausruestung -- unabhaengig von der Groesse der Armeen.
        with self.assertNumQueries(5):
            instance_client.game_start_document(game)

    @unittest.skipUnless(jsonschema, "jsonschema nicht installiert")
    def test_document_matches_schema(self):
        from . import instance_client

        self.game.host.name = "host-name"
        self.game.host.save()
        schema = json.loads(SPIELSTART_SCHEMA.read_text())

        jsonschema.validate(instance_client.game_start_document(self.game), schema)

    @mock.patch("api.instance_client.requests.post")
    def test_start_without_control_url_fails(self, post):
        from . import instance_client

        self.server.control_url = ""
        self.server.save()
        self.game.refresh_from_db()

        with self.assertRaises(instance_client.InstanceError):
            instance_client.start_game(self.game)
        post.assert_not_called()

    @mock.patch("api.instance_client.requests.post")
    def test_start_rejected_by_instance_fails(self, post):
        from . import instance_client

        post.return_value = mock.Mock(status_code=400, text="kaputt")

        with self.assertRaises(instance_client.InstanceError):
            instance_client.start_game(self.game)


class AccessTests(unittest.TestCase):
    """access.py: Codes, Spieler-Id und wie eine Nachricht an den Spielserver aussieht."""

    def test_player_id_packs_the_identifier(self):
        from . import access

        # Dieselbe Zahl wie RAD_ControlUserIdFromIdentifier im Spielserver
        # (game-server-core/test/control/test_game_start.c).
        self.assertEqual(access.player_id("aB3xK9pQ"), 0x614233784B397051)
        with self.assertRaises(ValueError):
            access.player_id("kurz")

    def test_new_codes_are_hex_and_never_reserved(self):
        from . import access

        with mock.patch("api.access.secrets.randbits", side_effect=[0, 1, 0xABC]):
            self.assertEqual(access.new_zucchini_code(), "0000000000000abc")
        self.assertRegex(access.new_zucchini_code(), r"^[0-9a-f]{16}$")

    def test_frame_puts_code_and_sender_in_front(self):
        from . import access

        framed = access.frame("00000000000000ab", "aB3xK9pQ", b"\x0a\x02")

        self.assertEqual(framed[:8], bytes.fromhex("00000000000000ab"))
        self.assertEqual(framed[8:16], bytes.fromhex("614233784b397051"))
        self.assertEqual(framed[16:], b"\x0a\x02")


class ConsumerTests(TransactionTestCase):
    """
    Der WebSocket-Consumer gegen einen echten UDP-Empfaenger an Stelle der
    Instanz: was beim Verbinden zum Client geht und was an der Instanz ankommt.
    TransactionTestCase, weil der Consumer die Datenbank aus einem anderen
    Thread liest (database_sync_to_async).
    """

    def setUp(self):
        self.user = User.objects.create(username="spieler")
        self.player = Player.objects.create(user=self.user)
        guest = Player.objects.create(user=User.objects.create(username="gast"))
        self.server = GameServer.objects.create(
            name="server-ws", address="127.0.0.1", port=1, control_url="http://instanz:8000"
        )
        self.game = Game.objects.create(
            name="laeuft", password="geheim", server=self.server, host=self.player,
            second_player=guest, points_limit=100, status=GameStatus.RUNNING,
            host_zucchini_code="1122334455667788", second_player_zucchini_code="99aabbccddeeff00",
        )

    async def _listen(self):
        """Ein UDP-Empfaenger auf einem freien Port; der Server zeigt dorthin."""
        received = asyncio.Queue()

        class Receiver(asyncio.DatagramProtocol):
            def datagram_received(self, data, addr):
                received.put_nowait(data)

        loop = asyncio.get_running_loop()
        transport, _ = await loop.create_datagram_endpoint(Receiver, local_addr=("127.0.0.1", 0))
        port = transport.get_extra_info("sockname")[1]
        await database_sync_to_async(GameServer.objects.filter(pk=self.server.pk).update)(port=port)
        return transport, received

    async def _connect(self):
        from .consumers import EchoConsumer

        communicator = WebsocketCommunicator(EchoConsumer.as_asgi(), "/ws/echo/")
        communicator.scope["user"] = self.user
        connected, _ = await communicator.connect()
        self.assertTrue(connected)
        return communicator

    async def test_client_learns_its_public_id_but_not_the_code(self):
        transport, _ = await self._listen()
        communicator = await self._connect()

        identity = json.loads(await communicator.receive_from())
        self.assertEqual(identity, {"type": "identity", "data": {"player_id": self.player.identifier}})
        self.assertNotIn("1122334455667788", json.dumps(identity))

        await communicator.disconnect()
        transport.close()

    async def test_command_goes_out_with_code_and_sender(self):
        from . import access

        transport, received = await self._listen()
        communicator = await self._connect()
        await communicator.receive_from()

        await communicator.send_to(text_data=json.dumps(
            {"type": "command", "data": base64.b64encode(b"\x0a\x00").decode()}
        ))
        datagram = await asyncio.wait_for(received.get(), timeout=2)

        self.assertEqual(datagram[:8], bytes.fromhex("1122334455667788"))
        self.assertEqual(datagram[8:16], access.player_id(self.player.identifier).to_bytes(8, "big"))
        self.assertEqual(datagram[16:], b"\x0a\x00")

        await communicator.disconnect()
        transport.close()

    async def test_game_without_code_is_refused(self):
        await database_sync_to_async(Game.objects.filter(pk=self.game.pk).update)(host_zucchini_code="")

        from .consumers import EchoConsumer

        communicator = WebsocketCommunicator(EchoConsumer.as_asgi(), "/ws/echo/")
        communicator.scope["user"] = self.user
        connected, code = await communicator.connect()

        self.assertFalse(connected)
        self.assertEqual(code, 4409)
