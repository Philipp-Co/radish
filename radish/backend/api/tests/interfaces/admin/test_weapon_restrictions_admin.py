from typing import Unpack

from rest_framework import status

from api.models import EntityProfile, UnitType
from api.tests.army_test_case import ArmyTestCase
from api.tests.helpers import (
    ADMIN_TOKEN,
    PLAYER_TOKEN,
    SpeciesCatalogData,
    TestResponse,
    User,
    WeaponData,
    WeaponPayload,
    authenticate,
)


class WeaponRestrictionsAdminTests(ArmyTestCase):
    """
    admin_views.AdminWeapon*: wer eine Waffe tragen darf (unit_type_ids/
    profile_ids, nur aus ihrer Spezies) -- dieselbe Regel wie bei Ausruestung.
    """

    def setUp(self) -> None:
        super().setUp()
        self.admin_user = User.objects.create(username="weapon-admin")
        authenticate(self.client, self.admin_user, ADMIN_TOKEN)

    def post(self, **overrides: Unpack[WeaponPayload]) -> TestResponse:
        payload: WeaponPayload = {
            "species_id": self.humans.id, "name": "Pistole", "shots": 1, "strength": 3,
            "max_range": 12, "armor_penetration": 0, "cost": 1,
        }
        payload.update(overrides)
        return self.client.post("/api/admin/catalog/weapons/", payload, format="json")

    def test_create_with_restrictions(self) -> None:
        response: TestResponse = self.post(unit_type_ids=[self.scout_team.id], profile_ids=[self.leader.id])

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(response.data["unit_type_ids"], [self.scout_team.id])
        self.assertEqual(response.data["profile_ids"], [self.leader.id])

    def test_without_restrictions_everyone_may_carry(self) -> None:
        response: TestResponse = self.post()

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(response.data["unit_type_ids"], [])
        self.assertEqual(response.data["profile_ids"], [])

    def test_entries_of_other_species_are_rejected(self) -> None:
        alien_type: UnitType = UnitType.objects.create(
            species=self.aliens, name="Schwarm", movement=5, sight_range=3
        )
        drone: EntityProfile = EntityProfile.objects.create(
            unit_type=alien_type, name="Drohne", health=1, armor=1, toughness=1, strength=1, accuracy=1,
            equipment_slots=0, weapon_slots=1, cost=5,
        )

        response: TestResponse = self.post(unit_type_ids=[alien_type.id], profile_ids=[drone.id])

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("Schwarm", str(response.data))
        self.assertIn("Drohne", str(response.data))

    def test_patch_restrictions_of_used_weapon(self) -> None:
        url: str = f"/api/admin/catalog/weapons/{self.cannon.id}/"

        response: TestResponse = self.client.patch(url, {"profile_ids": [self.leader.id]}, format="json")

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertEqual(response.data["profile_ids"], [self.leader.id])
        self.assertEqual(response.data["strength"], 9)

    def test_list_query_count_is_constant(self) -> None:
        self.post(unit_type_ids=[self.squad.id], profile_ids=[self.scout.id])
        self.rifle.profiles.set([self.soldier, self.leader])

        # Waffen samt Spezies und in_use, dann je eine Abfrage fuer
        # Einheitentypen und Profile.
        with self.assertNumQueries(3):
            response: TestResponse = self.client.get("/api/admin/catalog/weapons/")

        self.assertEqual(len(response.data["weapons"]), 5)

    def test_player_catalog_carries_restrictions(self) -> None:
        self.cannon.unit_types.set([self.squad])
        authenticate(self.client, self.user, PLAYER_TOKEN)

        response: TestResponse = self.client.get("/api/client/catalog/")

        humans: SpeciesCatalogData = next(s for s in response.data["species"] if s["name"] == "Menschen")
        cannon: WeaponData = next(w for w in humans["weapons"] if w["name"] == "Kanone")
        self.assertEqual(cannon["unit_type_ids"], [self.squad.id])
        self.assertEqual(cannon["profile_ids"], [])
