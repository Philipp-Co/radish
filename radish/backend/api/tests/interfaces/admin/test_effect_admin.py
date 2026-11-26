from django.core.files.uploadedfile import SimpleUploadedFile
from rest_framework import status
from rest_framework.test import APITestCase

from api.models import Effect, EffectImage
from api.tests.effect_helpers import add_effect_image, create_complete_effect
from api.tests.helpers import ADMIN_TOKEN, PLAYER_TOKEN, TestResponse, User, authenticate
from api.tests.sprite_helpers import png_bytes

URL = "/api/admin/effects/"


def upload(name: str, width: int = 512, height: int = 512) -> SimpleUploadedFile:
    return SimpleUploadedFile(name, png_bytes(width, height), content_type="image/png")


def atlas(facing: str, image: EffectImage, first_frame: int = 0) -> dict[str, object]:
    return {
        "facing": facing,
        "image_id": image.pk,
        "frame_width": 128,
        "frame_height": 128,
        "first_frame": first_frame,
        "frame_count": 4,
        "anchor_x": 0.5,
        "anchor_y": 0.7,
    }


class EffectAdminTests(APITestCase):
    """admin_effect_views: Effekte mit PNGs und Atlanten -- nur als Admin."""

    def setUp(self) -> None:
        authenticate(self.client, User.objects.create(username="effect-admin"), ADMIN_TOKEN)

    def test_player_role_is_not_sufficient(self) -> None:
        authenticate(self.client, User.objects.create(username="player"), PLAYER_TOKEN)
        self.assertEqual(self.client.get(URL).status_code, status.HTTP_403_FORBIDDEN)

    def test_create_and_list(self) -> None:
        created: TestResponse = self.client.post(URL, {"key": "explosion_9"}, format="json")

        self.assertEqual(created.status_code, status.HTTP_201_CREATED, created.data)
        self.assertEqual(created.data["kind"], "explosion")
        self.assertEqual(created.data["draw_width"], 256)
        self.assertFalse(created.data["complete"])
        self.assertEqual(len(created.data["problems"]), 4)
        listed: TestResponse = self.client.get(URL)
        self.assertEqual([effect["key"] for effect in listed.data["effects"]], ["explosion_9"])

    def test_key_is_fixed_after_creation(self) -> None:
        effect: Effect = create_complete_effect()

        response: TestResponse = self.client.patch(f"{URL}{effect.pk}/", {"key": "other"}, format="json")
        changed: TestResponse = self.client.patch(f"{URL}{effect.pk}/", {"duration_ms": 1200}, format="json")

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertEqual(changed.status_code, status.HTTP_200_OK, changed.data)
        self.assertEqual(changed.data["duration_ms"], 1200.0)

    def test_upload_and_set_all_atlases_completes_the_effect(self) -> None:
        effect: Effect = Effect.objects.create(key="explosion_9")
        uploaded: TestResponse = self.client.post(
            f"{URL}{effect.pk}/images/", {"file": upload("Sheet.PNG")}, format="multipart"
        )
        self.assertEqual(uploaded.status_code, status.HTTP_201_CREATED, uploaded.data)
        image: EffectImage = effect.images.get(name="sheet.png")

        response: TestResponse = self.client.put(
            f"{URL}{effect.pk}/atlases/",
            [atlas(facing, image, 4 * index) for index, facing in enumerate(["sw", "se", "ne", "nw"])],
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertTrue(response.data["complete"], response.data["problems"])

    def test_atlases_reject_duplicate_facing_and_foreign_png(self) -> None:
        effect: Effect = Effect.objects.create(key="explosion_9")
        image: EffectImage = add_effect_image(effect, "sheet.png")
        foreign: EffectImage = add_effect_image(create_complete_effect(), "foreign.png")

        duplicate: TestResponse = self.client.put(
            f"{URL}{effect.pk}/atlases/", [atlas("sw", image), atlas("sw", image)], format="json"
        )
        stranger: TestResponse = self.client.put(f"{URL}{effect.pk}/atlases/", [atlas("sw", foreign)], format="json")
        no_facing: TestResponse = self.client.put(f"{URL}{effect.pk}/atlases/", [atlas("", image)], format="json")

        self.assertEqual(duplicate.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertEqual(stranger.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertEqual(no_facing.status_code, status.HTTP_400_BAD_REQUEST)

    def test_shown_png_cannot_be_deleted_but_replaced(self) -> None:
        effect: Effect = create_complete_effect()
        image: EffectImage = effect.images.get(name="sheet.png")

        deleted: TestResponse = self.client.delete(f"{URL}{effect.pk}/images/{image.pk}/")
        replaced: TestResponse = self.client.put(
            f"{URL}{effect.pk}/images/{image.pk}/", {"file": upload("other.png", 256, 256)}, format="multipart"
        )

        self.assertEqual(deleted.status_code, status.HTTP_409_CONFLICT)
        self.assertEqual(replaced.status_code, status.HTTP_200_OK, replaced.data)
        self.assertEqual(replaced.data["images"][0]["name"], "sheet.png")
        self.assertFalse(replaced.data["complete"])

    def test_file_and_delete_effect(self) -> None:
        effect: Effect = create_complete_effect()
        image: EffectImage = effect.images.get(name="sheet.png")

        file: TestResponse = self.client.get(f"{URL}{effect.pk}/images/{image.pk}/file/")
        deleted: TestResponse = self.client.delete(f"{URL}{effect.pk}/")

        self.assertEqual(file.status_code, status.HTTP_200_OK)
        self.assertEqual(file["Content-Type"], "image/png")
        self.assertEqual(deleted.status_code, status.HTTP_204_NO_CONTENT)
        self.assertFalse(Effect.objects.exists())
        self.assertFalse(EffectImage.objects.exists())
