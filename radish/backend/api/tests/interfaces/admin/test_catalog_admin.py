from rest_framework import status

from api.models import (
    Army,
    Player,
    UnitType,
    Weapon,
)
from api.interfaces.player.serializers.army_write_serializer import ArmyWriteSerializer

from api.tests.helpers import (
    ADMIN_TOKEN,
    PLAYER_TOKEN,
    User,
)
from api.tests.army_test_case import ArmyTestCase


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
             "max_entities": 5, "movement": 6, "sight_range": 4},
            format="json",
        )
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data["profiles"], [])
        self.assertFalse(response.data["in_use"])
        unit_type_id = response.data["id"]

        response = self.client.post(
            "/api/admin/catalog/profiles/",
            {"unit_type_id": unit_type_id, "name": "Gardist", "health": 1, "armor": 4, "toughness": 3,
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
             "max_entities": 10, "max_heavy_weapons": 2, "movement": 5, "sight_range": 4},
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
             "movement": 6, "sight_range": 4},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_unit_type_rejects_more_than_ten_entities(self):
        response = self.client.post(
            "/api/admin/catalog/unit-types/",
            {"species_id": self.humans.id, "name": "Horde", "max_entities": 11, "movement": 6,
             "sight_range": 4},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def profile_payload(self, **overrides):
        payload = {"unit_type_id": self.squad.id, "name": "Sanitaeter", "health": 1,
                   "armor": 3, "toughness": 3, "strength": 3, "accuracy": 4, "equipment_slots": 2,
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

    def test_profile_requires_toughness(self):
        payload = self.profile_payload()
        del payload["toughness"]

        response = self.client.post("/api/admin/catalog/profiles/", payload, format="json")

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("toughness", response.data)

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
                   "max_entities": 5, "movement": 6, "sight_range": 4}

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

    def test_unit_type_requires_sight_range(self):
        response = self.client.post(
            "/api/admin/catalog/unit-types/",
            {"species_id": self.humans.id, "name": "Ohne", "min_entities": 1, "max_entities": 3,
             "movement": 6},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("sight_range", response.data)

    def test_unit_type_sight_range_fits_a_byte(self):
        # Im Spiel ist die Sichtweite ein Byte (RAD_Unit_t.sight_range): 255
        # geht noch, 256 nimmt schon der Katalog nicht an.
        url = "/api/admin/catalog/unit-types/"
        payload = {"species_id": self.humans.id, "name": "Weitblick", "min_entities": 1,
                   "max_entities": 3, "movement": 6}

        response = self.client.post(url, dict(payload, sight_range=256), format="json")
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("sight_range", response.data)

        response = self.client.post(url, dict(payload, sight_range=255), format="json")
        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(response.data["sight_range"], 255)

    def test_unit_limits_default_to_none(self):
        response = self.client.post(
            "/api/admin/catalog/unit-types/",
            {"species_id": self.humans.id, "name": "Neu", "min_entities": 1, "max_entities": 3,
             "movement": 6, "sight_range": 4},
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
