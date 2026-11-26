import json

from django.test import TestCase, override_settings

from api.models import TerrainType, UnitSprite
from api.tests.effect_helpers import create_complete_effect
from api.tests.sprite_helpers import create_complete_sprite
from api.tests.tile_helpers import dress_like_high_grass
from web.tests.helpers import ServingTestCase, TestResponse


class SpriteViewTests(ServingTestCase, TestCase):
    """
    /wasm/assets/manifest.json und /wasm/assets/uploaded/...: das Manifest aus
    dem Build, ergaenzt um die Einheiten-, Tile- und Effekt-Bilder aus der
    Datenbank.
    """

    def setUp(self) -> None:
        super().setUp()
        (self.wasm_dir / "assets").mkdir()
        (self.wasm_dir / "assets" / "manifest.json").write_text(
            json.dumps({"/assets/fonts/michroma/michroma_32.png": "fonts/michroma/michroma_32.0123456789ab.png"})
        )
        self.enterContext(override_settings(RADISH_WASM_DIR=self.wasm_dir))

    def manifest(self) -> dict[str, str]:
        response: TestResponse = self.client.get("/wasm/assets/manifest.json")
        self.assertEqual(response.status_code, 200)
        self.assertIn("no-cache", response["Cache-Control"])
        return json.loads(response.content)

    def test_manifest_without_uploads_is_the_built_one(self) -> None:
        self.assertEqual(
            self.manifest(), {"/assets/fonts/michroma/michroma_32.png": "fonts/michroma/michroma_32.0123456789ab.png"}
        )

    def test_uploaded_sprite_is_added_and_served(self) -> None:
        create_complete_sprite("infantry")

        manifest: dict[str, str] = self.manifest()
        self.assertIn("/assets/fonts/michroma/michroma_32.png", manifest)

        response: TestResponse = self.client.get(f"/wasm/assets/{manifest['/assets/units/infantry/sheet.png']}")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response["Content-Type"], "image/png")
        self.assertIn("immutable", response["Cache-Control"])

        response = self.client.get(f"/wasm/assets/{manifest['/assets/units/infantry/sprite.json']}")
        self.assertEqual(json.loads(response.content)["draw_width"], 96)

    def test_unknown_or_outdated_file_is_404(self) -> None:
        sprite: UnitSprite = create_complete_sprite("infantry")
        old: str = self.manifest()["/assets/units/infantry/sprite.json"]
        sprite.draw_width = 64
        sprite.save()

        self.assertEqual(self.client.get(f"/wasm/assets/{old}").status_code, 404)
        self.assertEqual(self.client.get("/wasm/assets/uploaded/units/none/x.0123456789ab.png").status_code, 404)

    def test_tile_images_are_added_and_served(self) -> None:
        dress_like_high_grass(TerrainType.objects.create(code="high_grass"))

        manifest: dict[str, str] = self.manifest()

        response: TestResponse = self.client.get(f"/wasm/assets/{manifest['/assets/tiles/high_grass/tile.json']}")
        self.assertEqual(response.status_code, 200)
        self.assertIn("immutable", response["Cache-Control"])
        self.assertEqual(json.loads(response.content)["decor"][1]["in_front"], True)

        response = self.client.get(f"/wasm/assets/{manifest['/assets/tiles/high_grass/grass_tile.png']}")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response["Content-Type"], "image/png")
        self.assertEqual(self.client.get("/wasm/assets/uploaded/tiles/mud/tile.0123456789ab.json").status_code, 404)

    def test_effects_are_added_and_served(self) -> None:
        create_complete_effect("explosion_2")

        manifest: dict[str, str] = self.manifest()

        response: TestResponse = self.client.get(f"/wasm/assets/{manifest['/assets/effects/explosion_2/effect.json']}")
        self.assertEqual(response.status_code, 200)
        self.assertIn("immutable", response["Cache-Control"])
        self.assertEqual(json.loads(response.content)["kind"], "explosion")

        response = self.client.get(f"/wasm/assets/{manifest['/assets/effects/explosion_2/sheet.png']}")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response["Content-Type"], "image/png")
        self.assertEqual(
            self.client.get("/wasm/assets/uploaded/effects/none/effect.0123456789ab.json").status_code, 404
        )
