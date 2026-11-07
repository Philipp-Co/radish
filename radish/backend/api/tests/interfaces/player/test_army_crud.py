from rest_framework import status

from api.models import (
    Army,
    Player,
    Unit,
)

from api.tests.helpers import (
    authenticate,
    find_by,
    ADMIN_TOKEN,
    ArmyListData,
    EntityData,
    SpeciesCatalogData,
    TestResponse,
    UnitData,
    UnitPayload,
    UnitTypeData,
    WeaponData,
    ProfileData,
    User,
)
from api.tests.army_test_case import ArmyTestCase


class ArmyCrudTests(ArmyTestCase):
    def test_catalog_lists_species_with_entries(self) -> None:
        response: TestResponse = self.client.get("/api/client/catalog/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        humans: SpeciesCatalogData = find_by(response.data["species"], name="Menschen")
        self.assertEqual(
            sorted(t["name"] for t in humans["unit_types"]), ["Spaeher", "Transporter", "Trupp"]
        )
        squad: UnitTypeData = find_by(humans["unit_types"], name="Trupp")
        self.assertEqual(len(squad["profiles"]), 2)
        leader: ProfileData = find_by(squad["profiles"], name="Anfuehrer")
        self.assertEqual(leader["accuracy"], 3)
        self.assertEqual(len(humans["weapons"]), 3)
        battle_cannon: WeaponData = find_by(humans["weapons"], name="Kampfgeschuetz")
        self.assertEqual(battle_cannon["weapon_class"], "super_heavy")
        self.assertEqual((battle_cannon["min_range"], battle_cannon["max_range"]), (6, 72))
        self.assertEqual((leader["heavy_weapon_slots"], leader["super_heavy_weapon_slots"]), (1, 1))
        self.assertNotIn("heavy_weapon_slots", squad)
        truck: UnitTypeData = find_by(humans["unit_types"], name="Transporter")
        self.assertEqual(truck["transport_capacity"], 2)
        self.assertEqual(squad["transport_capacity"], 0)
        self.assertTrue(squad["can_capture_objectives"])
        self.assertEqual((squad["movement"], truck["movement"]), (6, 12))
        self.assertEqual((squad["sight_range"], truck["sight_range"]), (4, 3))
        self.assertFalse(truck["can_capture_objectives"])
        self.assertEqual(humans["equipment"][0]["name"], "Medipack")

    def test_create_and_read_army(self) -> None:
        response: TestResponse = self.create_army()

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        army_id: int = response.data["id"]
        response = self.client.get(f"/api/client/armies/{army_id}/")
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["name"], "Erste Kompanie")
        self.assertEqual(response.data["species_name"], "Menschen")
        entities: list[EntityData] = response.data["units"][0]["entities"]
        self.assertEqual(entities[0]["profile_id"], self.leader.id)
        self.assertEqual(entities[0]["equipment_ids"], [self.medkit.id])
        self.assertEqual(entities[1]["weapon_ids"], [self.rifle.id, self.rifle.id])

    def test_costs_are_summed_from_catalog(self) -> None:
        # Anfuehrer 20 + Gewehr 1 + Medipack 3 + seine Kanone 15, zwei Soldaten
        # je 10 + 2x Gewehr 1.
        response: TestResponse = self.create_army(
            self.army_payload(
                units=[self.squad_payload(soldiers=2, heavy=[self.cannon.id])]
            )
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        unit: UnitData = response.data["units"][0]
        self.assertEqual([e["total_cost"] for e in unit["entities"]], [39, 12, 12])
        self.assertEqual(unit["total_cost"], 63)
        self.assertEqual(response.data["total_cost"], 63)
        listed: list[ArmyListData] = self.client.get("/api/client/armies/").data["armies"]
        self.assertEqual(listed[0]["total_cost"], 63)

    def test_cost_follows_catalog_price(self) -> None:
        army_id: int = self.create_army().data["id"]
        self.rifle.cost = 5
        self.rifle.save(update_fields=["cost"])

        response: TestResponse = self.client.get(f"/api/client/armies/{army_id}/")

        # Anfuehrer 20 + 5 + 3, Soldat 10 + 2x 5.
        self.assertEqual(response.data["total_cost"], 48)

    def test_list_query_count_does_not_grow_with_armies(self) -> None:
        number: int
        for number in range(3):
            self.create_army(
                self.army_payload(name=f"Kompanie {number}", units=[self.squad_payload(soldiers=3)])
            )

        # Armeen, Einheiten, Entitaeten, Waffen- und Ausruestungsslots -- je
        # eine Abfrage, egal wie viele Armeen es gibt.
        with self.assertNumQueries(5):
            response: TestResponse = self.client.get("/api/client/armies/")

        self.assertEqual(len(response.data["armies"]), 3)

    def test_create_army_without_units(self) -> None:
        response: TestResponse = self.create_army(self.army_payload(units=[]))

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data["units"], [])

    def test_list_contains_only_own_armies(self) -> None:
        self.create_army()
        other_player: Player = Player.objects.create(user=User.objects.create(username="other"))
        Army.objects.create(player=other_player, name="Fremde Armee", species=self.humans)

        response: TestResponse = self.client.get("/api/client/armies/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(
            [(a["name"], a["unit_count"]) for a in response.data["armies"]],
            [("Erste Kompanie", 1)],
        )

    def test_list_without_player_is_empty_and_creates_no_player(self) -> None:
        response: TestResponse = self.client.get("/api/client/armies/")

        self.assertEqual(response.data["armies"], [])
        self.assertFalse(Player.objects.filter(user=self.user).exists())

    def test_update_replaces_units(self) -> None:
        army_id: int = self.create_army().data["id"]
        scouts: UnitPayload = {
            "unit_type_id": self.scout_team.id,
            "entities": [{"profile_id": self.scout.id}],
        }

        response: TestResponse = self.client.put(
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

    def test_delete_army(self) -> None:
        army_id: int = self.create_army().data["id"]

        response: TestResponse = self.client.delete(f"/api/client/armies/{army_id}/")

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        self.assertFalse(Army.objects.filter(pk=army_id).exists())
        self.assertFalse(Unit.objects.filter(army_id=army_id).exists())

    def test_foreign_army_is_not_found(self) -> None:
        other_player: Player = Player.objects.create(user=User.objects.create(username="other"))
        army: Army = Army.objects.create(player=other_player, name="Fremde Armee", species=self.humans)
        url: str = f"/api/client/armies/{army.id}/"

        self.assertEqual(self.client.get(url).status_code, status.HTTP_404_NOT_FOUND)
        self.assertEqual(
            self.client.put(url, self.army_payload(), format="json").status_code,
            status.HTTP_404_NOT_FOUND,
        )
        self.assertEqual(self.client.delete(url).status_code, status.HTTP_404_NOT_FOUND)
        self.assertTrue(Army.objects.filter(pk=army.id).exists())

    def test_duplicate_name_is_rejected_only_for_same_player(self) -> None:
        self.create_army()
        other_player: Player = Player.objects.create(user=User.objects.create(username="other"))
        Army.objects.create(player=other_player, name="Zweite Kompanie", species=self.humans)

        self.assertEqual(self.create_army().status_code, status.HTTP_400_BAD_REQUEST)
        self.assertEqual(
            self.create_army(self.army_payload(name="Zweite Kompanie")).status_code,
            status.HTTP_201_CREATED,
        )

    def test_update_may_keep_own_name(self) -> None:
        army_id: int = self.create_army().data["id"]

        response: TestResponse = self.client.put(
            f"/api/client/armies/{army_id}/", self.army_payload(), format="json"
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK)

    def test_requires_player_role(self) -> None:
        authenticate(self.client, self.user, ADMIN_TOKEN)

        self.assertEqual(
            self.client.get("/api/client/armies/").status_code, status.HTTP_403_FORBIDDEN
        )
