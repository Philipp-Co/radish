from typing import Unpack

from rest_framework import status

from api.models import EntityProfile, Equipment, UnitType
from api.tests.army_test_case import ArmyTestCase
from api.tests.helpers import (
    ADMIN_TOKEN,
    PLAYER_TOKEN,
    EquipmentData,
    EquipmentPayload,
    SpeciesCatalogData,
    TestResponse,
    User,
    authenticate,
)


class EquipmentAdminTests(ArmyTestCase):
    """
    admin_views.AdminEquipment*: Modifikatoren eines Gegenstands und wer ihn
    tragen darf (unit_type_ids/profile_ids, nur aus seiner Spezies).
    """

    def setUp(self) -> None:
        super().setUp()
        self.admin_user = User.objects.create(username="equipment-admin")
        authenticate(self.client, self.admin_user, ADMIN_TOKEN)
        alien_type: UnitType = UnitType.objects.create(
            species=self.aliens, name="Schwarm", movement=5, sight_range=3
        )
        self.alien_profile: EntityProfile = EntityProfile.objects.create(
            unit_type=alien_type, name="Drohne", health=1, armor=1, toughness=1, strength=1, accuracy=1,
            equipment_slots=1, weapon_slots=1, cost=5,
        )

    def post(self, **overrides: Unpack[EquipmentPayload]) -> TestResponse:
        payload: EquipmentPayload = {"species_id": self.humans.id, "name": "Panzerweste", "cost": 4}
        payload.update(overrides)
        return self.client.post("/api/admin/catalog/equipment/", payload, format="json")

    def test_create_with_modifiers_and_restrictions(self) -> None:
        response: TestResponse = self.post(
            armor_modifier=1, weapon_shots_modifier=-1, weapon_range_modifier=6,
            unit_type_ids=[self.scout_team.id], profile_ids=[self.leader.id],
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(response.data["armor_modifier"], 1)
        self.assertEqual(response.data["weapon_shots_modifier"], -1)
        self.assertEqual(response.data["weapon_range_modifier"], 6)
        self.assertEqual(response.data["unit_type_ids"], [self.scout_team.id])
        self.assertEqual(response.data["profile_ids"], [self.leader.id])
        self.assertFalse(response.data["in_use"])

    def test_defaults_change_nothing_and_allow_everyone(self) -> None:
        response: TestResponse = self.post()

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(response.data["health_modifier"], 0)
        self.assertEqual(response.data["weapon_armor_penetration_modifier"], 0)
        self.assertEqual(response.data["unit_type_ids"], [])
        self.assertEqual(response.data["profile_ids"], [])

    def test_unit_type_of_other_species_is_rejected(self) -> None:
        alien_type: UnitType = self.alien_profile.unit_type

        response: TestResponse = self.post(unit_type_ids=[alien_type.id])

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("Schwarm", str(response.data))
        self.assertFalse(Equipment.objects.filter(name="Panzerweste").exists())

    def test_profile_of_other_species_is_rejected(self) -> None:
        response: TestResponse = self.post(profile_ids=[self.alien_profile.id])

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("Drohne", str(response.data))

    def test_patch_keeps_restrictions_put_replaces_them(self) -> None:
        self.medkit.profiles.set([self.leader])
        url: str = f"/api/admin/catalog/equipment/{self.medkit.id}/"

        patched: TestResponse = self.client.patch(url, {"health_modifier": 1}, format="json")
        self.assertEqual(patched.status_code, status.HTTP_200_OK, patched.data)
        self.assertEqual(patched.data["profile_ids"], [self.leader.id])

        replaced: TestResponse = self.client.put(
            url,
            {"species_id": self.humans.id, "name": "Medipack", "cost": 3,
             "unit_type_ids": [self.squad.id], "profile_ids": []},
            format="json",
        )
        self.assertEqual(replaced.status_code, status.HTTP_200_OK, replaced.data)
        self.assertEqual(replaced.data["unit_type_ids"], [self.squad.id])
        self.assertEqual(replaced.data["profile_ids"], [])
        self.assertEqual(replaced.data["health_modifier"], 1)

    def test_list_query_count_is_constant(self) -> None:
        self.post(name="Helm", unit_type_ids=[self.squad.id], profile_ids=[self.scout.id])
        self.post(name="Fernglas", profile_ids=[self.leader.id, self.soldier.id])

        # Gegenstaende samt Spezies und in_use, dann je eine Abfrage fuer
        # Einheitentypen und Profile.
        with self.assertNumQueries(3):
            response: TestResponse = self.client.get("/api/admin/catalog/equipment/")

        self.assertEqual(len(response.data["equipment"]), 3)

    def test_player_catalog_carries_modifiers_and_restrictions(self) -> None:
        self.post(armor_modifier=2, unit_type_ids=[self.squad.id])
        authenticate(self.client, self.user, PLAYER_TOKEN)

        response: TestResponse = self.client.get("/api/client/catalog/")

        humans: SpeciesCatalogData = next(s for s in response.data["species"] if s["name"] == "Menschen")
        vest: EquipmentData = next(e for e in humans["equipment"] if e["name"] == "Panzerweste")
        self.assertEqual(vest["armor_modifier"], 2)
        self.assertEqual(vest["unit_type_ids"], [self.squad.id])
        self.assertEqual(vest["profile_ids"], [])
