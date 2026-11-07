from rest_framework import status
from rest_framework.test import APITestCase

from api.models import (
    TerrainCode,
    TerrainType,
)

from api.tests.helpers import (
    ADMIN_TOKEN,
    PLAYER_TOKEN,
    User,
    create_test_catalog,
)


class AdminTerrainTests(APITestCase):
    """
    admin_views.AdminTerrainTypeListView/DetailView: die Werte der Tile-Typen,
    nur mit "Radish-Admin", aenderbar, aber nicht anzulegen oder zu loeschen.
    Die Zeilen kommen aus create_test_catalog().
    """

    def setUp(self):
        create_test_catalog()
        self.admin_user = User.objects.create(username="radish-admin-user")
        self.client.force_authenticate(user=self.admin_user, token=ADMIN_TOKEN)

    def test_lists_every_type_with_the_seeded_values(self):
        response = self.client.get("/api/admin/terrain/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        terrain = response.data["terrain"]
        self.assertEqual([entry["code"] for entry in terrain], TerrainCode.values)
        forest = next(entry for entry in terrain if entry["code"] == "forest")
        self.assertEqual(
            forest,
            {"code": "forest", "label": "Wald", "movement_modifier": -1, "cover": 2, "impassable": False,
             "blocks_sight": True},
        )
        self.assertTrue(next(e for e in terrain if e["code"] == "water_deep")["impassable"])
        self.assertEqual(
            next(e for e in terrain if e["code"] == "dragons_teeth"),
            {"code": "dragons_teeth", "label": "Drachenzaehne", "movement_modifier": 0, "cover": 0,
             "impassable": True, "blocks_sight": False},
        )
        self.assertEqual(
            [e["code"] for e in terrain if e["blocks_sight"]], ["forest", "city", "fortification", "high_grass"]
        )

    def test_player_role_is_not_sufficient(self):
        self.client.force_authenticate(user=self.admin_user, token=PLAYER_TOKEN)

        self.assertEqual(self.client.get("/api/admin/terrain/").status_code, status.HTTP_403_FORBIDDEN)
        response = self.client.patch("/api/admin/terrain/forest/", {"cover": 5}, format="json")
        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)

    def test_admin_changes_the_values(self):
        response = self.client.put(
            "/api/admin/terrain/forest/",
            {"movement_modifier": -2, "cover": 3, "impassable": True, "blocks_sight": False},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        forest = TerrainType.objects.get(code="forest")
        self.assertEqual(
            (forest.movement_modifier, forest.cover, forest.impassable, forest.blocks_sight), (-2, 3, True, False)
        )

    def test_code_cannot_be_changed(self):
        response = self.client.patch("/api/admin/terrain/forest/", {"code": "road", "cover": 1}, format="json")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["code"], "forest")
        self.assertEqual(TerrainType.objects.filter(code="forest").count(), 1)
        self.assertEqual(TerrainType.objects.get(code="road").cover, 0)

    def test_values_outside_the_limits_are_rejected(self):
        for values in ({"cover": 11}, {"cover": -1}, {"movement_modifier": 11},
                       {"movement_modifier": -11}, {"impassable": "vielleicht"},
                       {"blocks_sight": "vielleicht"}):
            response = self.client.patch("/api/admin/terrain/forest/", values, format="json")
            self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST, values)

    def test_cannot_create_or_delete(self):
        response = self.client.post(
            "/api/admin/terrain/", {"code": "lava", "cover": 1}, format="json"
        )
        self.assertEqual(response.status_code, status.HTTP_405_METHOD_NOT_ALLOWED)
        response = self.client.delete("/api/admin/terrain/forest/")
        self.assertEqual(response.status_code, status.HTTP_405_METHOD_NOT_ALLOWED)
        self.assertEqual(TerrainType.objects.count(), len(TerrainCode.values))

    def test_unknown_type_is_not_found(self):
        self.assertEqual(self.client.get("/api/admin/terrain/lava/").status_code, status.HTTP_404_NOT_FOUND)
