import json
import unittest

from django.conf import settings
from django.test import TestCase

from api.domain.tiles.document import tile_document
from api.domain.tiles.publishing import published_tile_file, uploaded_tile_manifest
from api.models import TerrainDecor, TerrainImage, TerrainType
from api.tests.tile_helpers import TILE_SCHEMA_PATH, add_terrain_image, dress_like_high_grass

try:
    import jsonschema
except ImportError:  # wie in test_sprites: ohne Werkzeug entfaellt nur der Test
    jsonschema = None  # type: ignore[assignment]


def base_of(terrain: TerrainType) -> TerrainImage | None:
    return terrain.images.filter(is_base=True).first()


def high_grass() -> TerrainType:
    return dress_like_high_grass(TerrainType.objects.create(code="high_grass"))


class TileDocumentTests(TestCase):
    def test_document_of_high_grass(self) -> None:
        terrain: TerrainType = high_grass()

        self.assertEqual(
            tile_document(base_of(terrain), terrain.decor.all()),
            {
                "version": 1,
                "image": "grass_tile.png",
                "decor": [
                    {"image": "high_grass_back.png", "offset_x": 6, "offset_y": -66, "in_front": False},
                    {"image": "high_grass_front.png", "offset_x": 6, "offset_y": -66, "in_front": True},
                ],
            },
        )

    def test_no_document_without_image_and_decor(self) -> None:
        terrain: TerrainType = TerrainType.objects.create(code="mud")
        add_terrain_image(terrain, "unused.png")

        self.assertIsNone(tile_document(base_of(terrain), []))

    def test_decor_only(self) -> None:
        terrain: TerrainType = TerrainType.objects.create(code="forest")
        TerrainDecor.objects.create(terrain=terrain, layer=0, image=add_terrain_image(terrain, "forest.png"))

        self.assertEqual(
            tile_document(base_of(terrain), terrain.decor.all()),
            {"version": 1, "decor": [{"image": "forest.png", "offset_x": 0, "offset_y": 0, "in_front": False}]},
        )

    @unittest.skipUnless(jsonschema is not None, "jsonschema nicht installiert")
    def test_document_follows_the_schema(self) -> None:
        schema: dict[str, object] = json.loads((settings.BASE_DIR.parent / TILE_SCHEMA_PATH).read_text())
        terrain: TerrainType = high_grass()

        jsonschema.validate(tile_document(base_of(terrain), terrain.decor.all()), schema)


class TilePublishingTests(TestCase):
    def test_manifest_lists_json_and_shown_images_only(self) -> None:
        terrain: TerrainType = high_grass()
        add_terrain_image(terrain, "unused.png")
        TerrainType.objects.create(code="mud")

        manifest: dict[str, str] = uploaded_tile_manifest()

        self.assertEqual(
            sorted(manifest),
            [
                "/assets/tiles/high_grass/grass_tile.png",
                "/assets/tiles/high_grass/high_grass_back.png",
                "/assets/tiles/high_grass/high_grass_front.png",
                "/assets/tiles/high_grass/tile.json",
            ],
        )
        self.assertRegex(
            manifest["/assets/tiles/high_grass/tile.json"], r"^uploaded/tiles/high_grass/tile\.[0-9a-f]{12}\.json$"
        )

    def test_published_files_match_the_manifest(self) -> None:
        high_grass()
        manifest: dict[str, str] = uploaded_tile_manifest()

        document = published_tile_file(manifest["/assets/tiles/high_grass/tile.json"])
        assert document is not None
        self.assertEqual(document.content_type, "application/json")
        self.assertEqual(json.loads(document.content)["image"], "grass_tile.png")

        image = published_tile_file(manifest["/assets/tiles/high_grass/high_grass_front.png"])
        assert image is not None
        self.assertEqual(image.content_type, "image/png")

    def test_old_version_and_unknown_names_are_gone(self) -> None:
        terrain: TerrainType = high_grass()
        old: str = uploaded_tile_manifest()["/assets/tiles/high_grass/tile.json"]

        TerrainDecor.objects.filter(terrain=terrain, layer=1).update(offset_x=0)

        self.assertIsNone(published_tile_file(old))
        self.assertIsNotNone(published_tile_file(uploaded_tile_manifest()["/assets/tiles/high_grass/tile.json"]))
        self.assertIsNone(published_tile_file("uploaded/tiles/high_grass/unused.0123456789ab.png"))
        self.assertIsNone(published_tile_file("uploaded/tiles/../units/x.0123456789ab.png"))
        self.assertIsNone(published_tile_file("uploaded/units/high_grass/tile.0123456789ab.json"))
