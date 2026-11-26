from django.core.files.uploadedfile import SimpleUploadedFile
from rest_framework import status
from rest_framework.test import APITestCase

from api.models import TerrainDecor, TerrainImage, TerrainType
from api.tests.helpers import ADMIN_TOKEN, PLAYER_TOKEN, TestResponse, User, authenticate, create_test_catalog
from api.tests.sprite_helpers import png_bytes
from api.tests.tile_helpers import add_terrain_image

URL = "/api/admin/terrain/forest/"


def upload(name: str) -> SimpleUploadedFile:
    return SimpleUploadedFile(name, png_bytes(64, 32), content_type="image/png")


class TerrainLookAdminTests(APITestCase):
    """admin_terrain_views: PNGs je Typ, Bild und Dekorationen -- nur als Admin."""

    def setUp(self) -> None:
        create_test_catalog()
        authenticate(self.client, User.objects.create(username="terrain-admin"), ADMIN_TOKEN)
        self.forest: TerrainType = TerrainType.objects.get(code="forest")

    def put_look(self, look: dict[str, object]) -> TestResponse:
        response: TestResponse = self.client.put(f"{URL}look/", look, format="json")
        return response

    def test_player_role_is_not_sufficient(self) -> None:
        authenticate(self.client, User.objects.create(username="player"), PLAYER_TOKEN)
        response: TestResponse = self.client.post(f"{URL}images/", {"file": upload("forest.png")}, format="multipart")
        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)

    def test_upload_lists_the_image(self) -> None:
        response: TestResponse = self.client.post(
            f"{URL}images/", {"file": upload("Forest.PNG")}, format="multipart"
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(
            [(image["name"], image["width"], image["height"]) for image in response.data["images"]],
            [("forest.png", 64, 32)],
        )
        self.assertIsNone(response.data["image_id"])
        self.assertEqual(response.data["decor"], [])

    def test_upload_rejects_duplicate_names_and_no_png(self) -> None:
        add_terrain_image(self.forest, "forest.png")

        duplicate: TestResponse = self.client.post(f"{URL}images/", {"file": upload("forest.png")}, format="multipart")
        no_png: TestResponse = self.client.post(
            f"{URL}images/", {"file": SimpleUploadedFile("x.png", b"GIF89a" + b"\x00" * 30)}, format="multipart"
        )

        self.assertEqual(duplicate.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("name", duplicate.data)
        self.assertEqual(no_png.status_code, status.HTTP_400_BAD_REQUEST)

    def test_set_image_and_decor(self) -> None:
        grass: TerrainImage = add_terrain_image(self.forest, "grass_tile.png")
        trees: TerrainImage = add_terrain_image(self.forest, "forest.png")

        response: TestResponse = self.put_look(
            {"image_id": grass.pk, "decor": [{"image_id": trees.pk, "offset_x": 0, "offset_y": -128}]}
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertEqual(response.data["image_id"], grass.pk)
        self.assertEqual(
            response.data["decor"], [{"image_id": trees.pk, "offset_x": 0, "offset_y": -128, "in_front": False}]
        )

        # Was fehlt, gibt es danach nicht mehr.
        response = self.put_look({"image_id": None, "decor": []})
        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertIsNone(response.data["image_id"])
        self.assertFalse(TerrainDecor.objects.exists())

    def test_look_is_part_of_the_terrain_list(self) -> None:
        grass: TerrainImage = add_terrain_image(self.forest, "grass_tile.png")
        self.put_look({"image_id": grass.pk, "decor": []})

        response: TestResponse = self.client.get("/api/admin/terrain/")

        forest = next(entry for entry in response.data["terrain"] if entry["code"] == "forest")
        self.assertEqual(forest["image_id"], grass.pk)
        self.assertEqual(forest["images"][0]["name"], "grass_tile.png")

    def test_look_rejects_foreign_images_offsets_and_too_much_decor(self) -> None:
        own: TerrainImage = add_terrain_image(self.forest, "forest.png")
        foreign: TerrainImage = add_terrain_image(TerrainType.objects.get(code="meadow"), "grass.png")

        cases: list[dict[str, object]] = [
            {"image_id": foreign.pk, "decor": []},
            {"image_id": None, "decor": [{"image_id": foreign.pk}]},
            {"image_id": None, "decor": [{"image_id": own.pk, "offset_y": -129}]},
            {"image_id": None, "decor": [{"image_id": own.pk}] * 3},
        ]
        for look in cases:
            with self.subTest(look=look):
                self.assertEqual(self.put_look(look).status_code, status.HTTP_400_BAD_REQUEST)
        self.assertFalse(TerrainDecor.objects.exists())

    def test_shown_image_cannot_be_deleted(self) -> None:
        base: TerrainImage = add_terrain_image(self.forest, "grass_tile.png")
        shown: TerrainImage = add_terrain_image(self.forest, "forest.png")
        spare: TerrainImage = add_terrain_image(self.forest, "spare.png")
        self.put_look({"image_id": base.pk, "decor": [{"image_id": shown.pk}]})

        self.assertEqual(self.client.delete(f"{URL}images/{base.pk}/").status_code, status.HTTP_409_CONFLICT)
        self.assertEqual(self.client.delete(f"{URL}images/{shown.pk}/").status_code, status.HTTP_409_CONFLICT)
        self.assertEqual(self.client.delete(f"{URL}images/{spare.pk}/").status_code, status.HTTP_204_NO_CONTENT)

    def test_switching_the_base_image_keeps_one(self) -> None:
        first: TerrainImage = add_terrain_image(self.forest, "first.png")
        second: TerrainImage = add_terrain_image(self.forest, "second.png")
        self.put_look({"image_id": first.pk, "decor": []})

        response: TestResponse = self.put_look({"image_id": second.pk, "decor": []})

        self.assertEqual(response.data["image_id"], second.pk)
        self.assertEqual(list(TerrainImage.objects.filter(is_base=True)), [second])

    def test_replace_keeps_the_name_and_file_returns_bytes(self) -> None:
        image: TerrainImage = add_terrain_image(self.forest, "forest.png")
        old_sha: str = image.sha256

        response: TestResponse = self.client.put(
            f"{URL}images/{image.pk}/", {"file": upload("other.png")}, format="multipart"
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        image.refresh_from_db()
        self.assertEqual(image.name, "forest.png")
        self.assertNotEqual(image.sha256, old_sha)
        file: TestResponse = self.client.get(f"{URL}images/{image.pk}/file/")
        self.assertEqual(file["Content-Type"], "image/png")
        self.assertEqual(bytes(file.content), bytes(image.data))

    def test_images_of_another_type_are_not_found(self) -> None:
        foreign: TerrainImage = add_terrain_image(TerrainType.objects.get(code="meadow"), "grass.png")

        self.assertEqual(self.client.get(f"{URL}images/{foreign.pk}/file/").status_code, status.HTTP_404_NOT_FOUND)
        self.assertEqual(self.client.delete(f"{URL}images/{foreign.pk}/").status_code, status.HTTP_404_NOT_FOUND)
