import json
import unittest

from django.conf import settings
from django.test import TestCase

from api.domain.effects.document import effect_document
from api.domain.effects.problems import effect_problems
from api.domain.effects.publishing import published_effect_file, uploaded_effect_manifest
from api.models import Effect, EffectAtlas
from api.tests.effect_helpers import EFFECT_SCHEMA_PATH, add_effect_image, create_complete_effect

try:
    import jsonschema
except ImportError:  # wie in test_sprites: ohne Werkzeug entfaellt nur der Test
    jsonschema = None  # type: ignore[assignment]


def atlases_of(effect: Effect) -> list[EffectAtlas]:
    return list(effect.atlases.select_related("image"))


class EffectProblemTests(TestCase):
    def test_complete_effect_has_no_problems(self) -> None:
        self.assertEqual(effect_problems(create_complete_effect()), [])

    def test_missing_facing_is_a_problem(self) -> None:
        effect: Effect = create_complete_effect()
        effect.atlases.filter(facing="ne").delete()

        self.assertEqual(effect_problems(effect), ["Atlas fehlt: rechts oben."])

    def test_sequence_must_fit_into_its_png(self) -> None:
        effect: Effect = create_complete_effect()
        effect.atlases.filter(facing="nw").update(first_frame=13)

        problems: list[str] = effect_problems(effect)

        self.assertEqual(len(problems), 1)
        self.assertIn("16 Zellen", problems[0])
        self.assertIn("gebraucht werden 17", problems[0])


class EffectDocumentTests(TestCase):
    def test_document_of_a_complete_effect(self) -> None:
        effect: Effect = create_complete_effect()

        document: dict[str, object] = effect_document(effect, atlases_of(effect))

        self.assertEqual(
            {key: value for key, value in document.items() if key != "facings"},
            {"version": 1, "kind": "explosion", "draw_width": 256, "duration_ms": 2500.0},
        )
        facings = document["facings"]
        assert isinstance(facings, dict)
        self.assertEqual(list(facings), ["sw", "se", "ne", "nw"])
        self.assertEqual(
            facings["ne"],
            {
                "file": "sheet.png",
                "frame_width": 128,
                "frame_height": 128,
                "first_frame": 8,
                "frame_count": 4,
                "anchor": {"x": 0.25, "y": 0.75},
            },
        )

    @unittest.skipUnless(jsonschema is not None, "jsonschema nicht installiert")
    def test_document_follows_the_schema(self) -> None:
        schema: dict[str, object] = json.loads((settings.BASE_DIR.parent / EFFECT_SCHEMA_PATH).read_text())
        effect: Effect = create_complete_effect()

        jsonschema.validate(effect_document(effect, atlases_of(effect)), schema)


class EffectPublishingTests(TestCase):
    def test_manifest_has_json_and_shared_png_once(self) -> None:
        create_complete_effect()

        manifest: dict[str, str] = uploaded_effect_manifest()

        self.assertEqual(
            sorted(manifest), ["/assets/effects/explosion_2/effect.json", "/assets/effects/explosion_2/sheet.png"]
        )
        self.assertRegex(
            manifest["/assets/effects/explosion_2/sheet.png"],
            r"^uploaded/effects/explosion_2/sheet\.[0-9a-f]{12}\.png$",
        )

    def test_draft_is_not_published(self) -> None:
        effect: Effect = create_complete_effect()
        name: str = uploaded_effect_manifest()["/assets/effects/explosion_2/sheet.png"]
        effect.atlases.filter(facing="sw").delete()

        self.assertEqual(uploaded_effect_manifest(), {})
        self.assertIsNone(published_effect_file(name))

    def test_files_are_served_by_their_hash(self) -> None:
        create_complete_effect()
        manifest: dict[str, str] = uploaded_effect_manifest()

        document = published_effect_file(manifest["/assets/effects/explosion_2/effect.json"])
        image = published_effect_file(manifest["/assets/effects/explosion_2/sheet.png"])

        assert document is not None and image is not None
        self.assertEqual(json.loads(document.content)["facings"]["nw"]["first_frame"], 12)
        self.assertEqual(image.content_type, "image/png")
        self.assertIsNone(published_effect_file("uploaded/effects/explosion_2/effect.0123456789ab.json"))

    def test_unused_png_is_not_published(self) -> None:
        effect: Effect = create_complete_effect()
        unused = add_effect_image(effect, "unused.png")

        self.assertNotIn("/assets/effects/explosion_2/unused.png", uploaded_effect_manifest())
        self.assertIsNone(published_effect_file(f"uploaded/effects/explosion_2/unused.{unused.sha256[:12]}.png"))
