import json
import unittest

from django.conf import settings
from django.test import TestCase

from api.domain.sprites.builtin import built_sprite_keys
from api.domain.sprites.document import sprite_document
from api.domain.sprites.png import InvalidPng, png_size
from api.domain.sprites.problems import REQUIRED_ATLASES, sprite_problems
from api.domain.sprites.publishing import published_file, uploaded_manifest
from api.models import SpriteAnimation, SpriteFacing, UnitSprite, UnitSpriteAtlas
from api.tests.sprite_helpers import (
    SPRITE_SCHEMA_PATH,
    add_image,
    create_complete_sprite,
    png_bytes,
    use_built_manifest,
)

try:
    import jsonschema
except ImportError:  # wie in test_instance_client: ohne Werkzeug entfaellt nur der Test
    jsonschema = None  # type: ignore[assignment]


class PngSizeTests(unittest.TestCase):
    def test_reads_width_and_height(self) -> None:
        self.assertEqual(png_size(png_bytes(640, 256)), (640, 256))

    def test_rejects_what_is_no_png(self) -> None:
        for data in (b"", b"GIF89a" + b"\x00" * 30, png_bytes(4, 4)[:20]):
            with self.subTest(data=data[:8]):
                with self.assertRaises(InvalidPng):
                    png_size(data)


class SpriteProblemsTests(TestCase):
    def test_complete_sprite_has_no_problems(self) -> None:
        self.assertEqual(sprite_problems(create_complete_sprite()), [])
        self.assertEqual(len(REQUIRED_ATLASES), 17)

    def test_missing_atlas_is_named(self) -> None:
        sprite: UnitSprite = create_complete_sprite()
        sprite.atlases.filter(animation=SpriteAnimation.FIRE, facing=SpriteFacing.NW).delete()

        self.assertEqual(sprite_problems(sprite), ["Atlas fehlt: Schiessen, links oben."])

    def test_key_frames_must_ascend_and_fit(self) -> None:
        sprite: UnitSprite = create_complete_sprite()
        turn: UnitSpriteAtlas = sprite.atlases.get(animation=SpriteAnimation.TURN)
        for key_frames, expected in (
            ([0, 8, 4, 12], "streng aufsteigen"),
            ([0, 4, 8, 16], "hinter dem letzten"),
            ([0, 4, 8], "vier Schluesselbilder"),
            (None, "vier Schluesselbilder"),
        ):
            with self.subTest(key_frames=key_frames):
                turn.key_frames = key_frames
                turn.save()
                self.assertIn(expected, " ".join(sprite_problems(sprite)))

    def test_sequence_needs_fps_and_no_key_frames(self) -> None:
        sprite: UnitSprite = create_complete_sprite()
        move: UnitSpriteAtlas = sprite.atlases.get(animation=SpriteAnimation.MOVE, facing=SpriteFacing.SW)
        move.fps = None
        move.key_frames = [0, 1, 2, 3]
        move.save()

        problems: str = " ".join(sprite_problems(sprite))
        self.assertIn("Bilder je Sekunde", problems)
        self.assertIn("keine Schluesselbilder", problems)

    def test_image_must_hold_all_frames(self) -> None:
        # 512 x 256 hat Platz fuer 8 Zellen zu 128 x 128, die Drehung braucht 16.
        sprite: UnitSprite = create_complete_sprite()
        small = add_image(sprite, "small.png", 512, 256)
        sprite.atlases.filter(animation=SpriteAnimation.TURN).update(image=small)

        self.assertIn("Platz fuer 8 Zellen", " ".join(sprite_problems(sprite)))


@unittest.skipUnless(jsonschema is not None, "jsonschema nicht installiert")
class SpriteDocumentTests(TestCase):
    """Die erzeugte sprite.json gegen das Schema des Clients."""

    def test_document_matches_client_schema(self) -> None:
        sprite: UnitSprite = create_complete_sprite()
        schema: dict[str, object] = json.loads((settings.BASE_DIR.parent / SPRITE_SCHEMA_PATH).read_text())

        document: dict[str, object] = sprite_document(sprite, list(sprite.atlases.select_related("image")))

        jsonschema.validate(document, schema)
        self.assertEqual(document["draw_width"], 96)
        self.assertEqual(document["timing_ms"], {"step": 300.0})


class PublishingTests(TestCase):
    def setUp(self) -> None:
        use_built_manifest(self)

    def test_built_keys_come_from_the_manifest(self) -> None:
        self.assertEqual(built_sprite_keys(), {"heavy_tank"})

    def test_complete_sprite_is_in_the_manifest_and_served(self) -> None:
        create_complete_sprite("infantry")

        manifest: dict[str, str] = uploaded_manifest()

        self.assertEqual(
            sorted(manifest), ["/assets/units/infantry/sheet.png", "/assets/units/infantry/sprite.json"]
        )
        sprite_json = published_file(manifest["/assets/units/infantry/sprite.json"])
        assert sprite_json is not None
        self.assertEqual(json.loads(sprite_json.content)["animations"]["turn"]["file"], "sheet.png")
        sheet = published_file(manifest["/assets/units/infantry/sheet.png"])
        assert sheet is not None
        self.assertEqual(png_size(sheet.content), (512, 512))

    def test_incomplete_or_built_key_is_not_published(self) -> None:
        draft: UnitSprite = create_complete_sprite("draft")
        names: dict[str, str] = uploaded_manifest()
        draft.atlases.filter(animation=SpriteAnimation.TURN).delete()
        # Schon vergeben vom Build: auch vollstaendig nicht im Manifest.
        create_complete_sprite("heavy_tank")

        self.assertEqual(uploaded_manifest(), {})
        self.assertIsNone(published_file(names["/assets/units/draft/sprite.json"]))

    def test_new_version_gets_a_new_name(self) -> None:
        sprite: UnitSprite = create_complete_sprite("infantry")
        before: dict[str, str] = uploaded_manifest()
        sprite.draw_width = 128
        sprite.save()

        after: dict[str, str] = uploaded_manifest()

        path: str = "/assets/units/infantry/sprite.json"
        self.assertNotEqual(before[path], after[path])
        self.assertIsNone(published_file(before[path]))
        self.assertIsNotNone(published_file(after[path]))
        self.assertIsNone(published_file("uploaded/units/infantry/../../secret.0123456789ab.png"))
