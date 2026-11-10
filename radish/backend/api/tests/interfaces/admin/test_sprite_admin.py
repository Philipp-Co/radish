from django.core.files.uploadedfile import SimpleUploadedFile
from rest_framework import status
from rest_framework.test import APITestCase

from api.domain.sprites.problems import REQUIRED_ATLASES
from api.models import ROTATIONS, Species, UnitSprite, UnitSpriteImage, UnitType
from api.tests.helpers import ADMIN_TOKEN, PLAYER_TOKEN, TestResponse, User, authenticate
from api.tests.sprite_helpers import create_complete_sprite, png_bytes, use_built_manifest

URL = "/api/admin/sprites/"


def upload(name: str, width: int = 512, height: int = 512) -> SimpleUploadedFile:
    return SimpleUploadedFile(name, png_bytes(width, height), content_type="image/png")


def all_atlases(image_id: int) -> list[dict[str, object]]:
    """Alle 17 Atlanten auf eine PNG von 512 x 512."""
    return [
        {
            "animation": animation,
            "facing": facing,
            "image_id": image_id,
            "frame_width": 128,
            "frame_height": 128,
            "frame_count": 16 if animation in ROTATIONS else 4,
            "key_frames": [0, 4, 8, 12] if animation in ROTATIONS else None,
            "fps": None if animation in ROTATIONS else 12,
        }
        for animation, facing in REQUIRED_ATLASES
    ]


class SpriteAdminTests(APITestCase):
    """admin_sprite_views: Bilder anlegen, PNGs hochladen, Atlanten setzen -- nur als Admin."""

    def setUp(self) -> None:
        authenticate(self.client, User.objects.create(username="sprite-admin"), ADMIN_TOKEN)
        use_built_manifest(self)

    def create(self, key: str = "infantry") -> int:
        response: TestResponse = self.client.post(URL, {"key": key}, format="json")
        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        sprite_id: int = response.data["id"]
        return sprite_id

    def test_player_role_is_not_sufficient(self) -> None:
        authenticate(self.client, User.objects.create(username="player"), PLAYER_TOKEN)
        self.assertEqual(self.client.get(URL).status_code, status.HTTP_403_FORBIDDEN)

    def test_new_sprite_is_a_draft(self) -> None:
        sprite_id: int = self.create()

        response: TestResponse = self.client.get(f"{URL}{sprite_id}/")

        self.assertFalse(response.data["complete"])
        self.assertEqual(len(response.data["problems"]), 17)
        self.assertEqual(response.data["draw_width"], 128)

    def test_key_of_a_built_sprite_is_rejected(self) -> None:
        response: TestResponse = self.client.post(URL, {"key": "heavy_tank"}, format="json")

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("key", response.data)

    def test_key_cannot_change(self) -> None:
        sprite_id: int = self.create()

        response: TestResponse = self.client.patch(f"{URL}{sprite_id}/", {"key": "other"}, format="json")

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_upload_and_complete_a_sprite(self) -> None:
        sprite_id: int = self.create()

        response: TestResponse = self.client.post(f"{URL}{sprite_id}/images/", {"file": upload("Sheet.PNG")})
        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        image: dict[str, object] = response.data["images"][0]
        self.assertEqual((image["name"], image["width"], image["height"]), ("sheet.png", 512, 512))

        response = self.client.put(f"{URL}{sprite_id}/atlases/", all_atlases(int(str(image["id"]))), format="json")

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertTrue(response.data["complete"], response.data["problems"])
        self.assertEqual(len(response.data["atlases"]), 17)

    def test_upload_rejects_no_png_and_bad_names(self) -> None:
        sprite_id: int = self.create()
        for data, expected in (
            ({"file": SimpleUploadedFile("a.png", b"not a png")}, "file"),
            ({"file": upload("Turm 1.png")}, "name"),
            ({"file": upload("a.png"), "name": "../a.png"}, "name"),
        ):
            with self.subTest(expected=expected):
                response: TestResponse = self.client.post(f"{URL}{sprite_id}/images/", data)
                self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
                self.assertIn(expected, response.data)

    def test_same_name_twice_is_rejected_replace_keeps_name(self) -> None:
        sprite_id: int = self.create()
        image_id: int = self.client.post(f"{URL}{sprite_id}/images/", {"file": upload("a.png")}).data["images"][0]["id"]

        response: TestResponse = self.client.post(f"{URL}{sprite_id}/images/", {"file": upload("a.png")})
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

        response = self.client.put(f"{URL}{sprite_id}/images/{image_id}/", {"file": upload("other.png", 256, 128)})
        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertEqual(response.data["images"][0]["name"], "a.png")
        self.assertEqual(response.data["images"][0]["width"], 256)

    def test_image_file_for_preview(self) -> None:
        sprite: UnitSprite = create_complete_sprite()
        image: UnitSpriteImage = sprite.images.get()

        response = self.client.get(f"{URL}{sprite.pk}/images/{image.pk}/file/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response["Content-Type"], "image/png")
        self.assertEqual(response.content, bytes(image.data))

    def test_image_shown_by_an_atlas_cannot_be_deleted(self) -> None:
        sprite: UnitSprite = create_complete_sprite()
        image: UnitSpriteImage = sprite.images.get()

        response: TestResponse = self.client.delete(f"{URL}{sprite.pk}/images/{image.pk}/")

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)

    def test_atlases_are_validated_and_unique(self) -> None:
        sprite: UnitSprite = create_complete_sprite()
        image_id: int = sprite.images.get().pk
        atlases: list[dict[str, object]] = all_atlases(image_id)

        for broken, expected in (
            ([{**atlases[0], "facing": "sw"}], 400),  # Drehung mit Richtung
            ([{**atlases[1], "facing": ""}], 400),  # Zielen ohne Richtung
            ([atlases[0], atlases[0]], 400),  # doppelt
            ([{**atlases[0], "frame_count": 2000}], 400),
        ):
            with self.subTest(broken=broken[0]):
                response: TestResponse = self.client.put(f"{URL}{sprite.pk}/atlases/", broken, format="json")
                self.assertEqual(response.status_code, expected)
        # Nichts davon hat den vollstaendigen Satz angetastet.
        self.assertEqual(sprite.atlases.count(), 17)

    def test_image_of_other_sprite_cannot_be_used(self) -> None:
        other: UnitSprite = create_complete_sprite("other")
        sprite_id: int = self.create()

        response: TestResponse = self.client.put(
            f"{URL}{sprite_id}/atlases/", all_atlases(other.images.get().pk)[:1], format="json"
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_sprite_used_by_a_unit_type_cannot_be_deleted(self) -> None:
        sprite: UnitSprite = create_complete_sprite()
        UnitType.objects.create(
            species=Species.objects.create(name="Menschen"), name="Trupp", movement=6, sight_range=4, sprite=sprite.key
        )

        response: TestResponse = self.client.get(f"{URL}{sprite.pk}/")
        self.assertEqual(response.data["used_by"], ["Trupp (Menschen)"])
        self.assertEqual(self.client.delete(f"{URL}{sprite.pk}/").status_code, status.HTTP_409_CONFLICT)

    def test_unused_sprite_is_deleted_with_its_files(self) -> None:
        sprite: UnitSprite = create_complete_sprite()

        response: TestResponse = self.client.delete(f"{URL}{sprite.pk}/")

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        self.assertFalse(UnitSpriteImage.objects.exists())
