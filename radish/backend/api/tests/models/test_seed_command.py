import hashlib
import io
from pathlib import Path
import tempfile
from unittest import mock

from django.core.management import call_command
from django.core.management.base import CommandError
from rest_framework.test import APITestCase

from api.domain.effects.problems import effect_problems
from api.domain.effects.publishing import uploaded_effect_manifest
from api.domain.sprites.problems import sprite_problems
from api.domain.sprites.publishing import uploaded_manifest
from api.domain.tiles.publishing import uploaded_tile_manifest
from api.management.commands import seed
from api.models import (
    Effect,
    Species,
    TerrainCode,
    TerrainDecor,
    TerrainImage,
    TerrainType,
    UnitSprite,
    UnitSpriteImage,
    UnitType,
)

SPRITE_TABLES: list[str] = ["api_unitsprite", "api_unitspriteimage", "api_unitspriteatlas"]
TILE_TABLES: list[str] = ["api_terrainimage", "api_terraindecor"]
EFFECT_TABLES: list[str] = ["api_effect", "api_effectimage", "api_effectatlas"]

# Die Explosionen, die vor der Umstellung im Client-Build lagen
# (radish/assets/effects/) -- jetzt im Dump.
EXPLOSIONS: list[str] = ["explosion_0", "explosion_1", "explosion_2"]

# Die Typen, die vor der Umstellung im Client-Code ein Bild hatten
# (client/src/view/rendering/iso_tile/) -- jetzt im Dump.
TYPES_WITH_IMAGES: list[str] = ["meadow", "forest", "dragons_teeth", "high_grass"]


class SeedCommandTests(APITestCase):
    """management/commands/seed.py: wann und wie der Dump api/fixtures/seed.sql geladen wird."""

    def seed(self, **options: object) -> None:
        call_command("seed", stdout=io.StringIO(), verbosity=0, **options)

    def fixture_count(self, table: str) -> int:
        return sum(len(statement.rows) for statement in seed.read_fixture() if statement.table == table)

    def test_dump_brings_every_terrain_type(self) -> None:
        # Sonst scheitert in einer frisch befuellten Datenbank jeder Spielstart
        # (instance_client._terrain_document).
        self.seed()

        self.assertEqual(list(TerrainType.objects.values_list("code", flat=True)), TerrainCode.values)

    def test_dump_brings_a_complete_image_for_every_unit_type(self) -> None:
        # Die Bilder gibt es nur in der Datenbank: der Client-Build bringt
        # keine mit. Fehlt eines, zeichnet der Client ein Rechteck.
        self.seed()
        manifest: dict[str, str] = uploaded_manifest()

        key: str
        for key in UnitType.objects.exclude(sprite="").values_list("sprite", flat=True):
            with self.subTest(sprite=key):
                sprite: UnitSprite = UnitSprite.objects.get(key=key)
                self.assertEqual(sprite_problems(sprite), [])
                self.assertIn(f"/assets/units/{key}/sprite.json", manifest)

    def test_dump_brings_the_tile_images(self) -> None:
        # Auch die Tile-Bilder gibt es nur in der Datenbank.
        self.seed()
        manifest: dict[str, str] = uploaded_tile_manifest()

        code: str
        for code in TYPES_WITH_IMAGES:
            with self.subTest(terrain=code):
                self.assertIn(f"/assets/tiles/{code}/tile.json", manifest)
                self.assertIn(f"/assets/tiles/{code}/grass_tile.png", manifest)
        self.assertEqual(TerrainDecor.objects.filter(terrain__code="high_grass", in_front=True).count(), 1)

    def test_png_bytes_survive_the_dump(self) -> None:
        self.seed()

        images: list[UnitSpriteImage | TerrainImage] = [*UnitSpriteImage.objects.all(), *TerrainImage.objects.all()]
        for image in images:
            with self.subTest(image=str(image)):
                self.assertEqual(hashlib.sha256(bytes(image.data)).hexdigest(), image.sha256)

    def test_loads_into_a_new_database(self) -> None:
        self.seed()

        self.assertEqual(TerrainType.objects.count(), self.fixture_count("api_terraintype"))
        self.assertEqual(Species.objects.count(), self.fixture_count("api_species"))
        self.assertEqual(UnitSpriteImage.objects.count(), self.fixture_count("api_unitspriteimage"))

    def test_sequences_continue_after_the_dump(self) -> None:
        # setval aus dem Dump: ein neuer Datensatz bekommt keine schon vergebene ID.
        self.seed()

        Species.objects.create(name="Neu")

        self.assertEqual(Species.objects.count(), self.fixture_count("api_species") + 1)

    def test_existing_content_is_left_alone(self) -> None:
        Species.objects.create(name="Gepflegt")

        with self.assertRaisesMessage(CommandError, "Nichts geladen: schon Inhalt in api_species"):
            self.seed()

        self.assertEqual(list(Species.objects.values_list("name", flat=True)), ["Gepflegt"])
        self.assertEqual(TerrainType.objects.count(), 0)

    def test_tables_bring_the_images_into_a_maintained_database(self) -> None:
        Species.objects.create(name="Gepflegt")

        self.seed(table=SPRITE_TABLES)

        self.assertEqual(list(Species.objects.values_list("name", flat=True)), ["Gepflegt"])
        self.assertEqual(UnitSprite.objects.count(), self.fixture_count("api_unitsprite"))
        self.assertEqual(UnitSpriteImage.objects.count(), self.fixture_count("api_unitspriteimage"))
        # Auch deren Sequenzen: ein neues Bild bekommt eine freie ID.
        UnitSprite.objects.create(key="neu")

    def test_tables_bring_the_tile_images_into_a_maintained_database(self) -> None:
        # Die Typen stehen schon da; PNGs und Dekorationen verweisen nur auf sie.
        self.seed(table=["api_terraintype"])

        self.seed(table=TILE_TABLES)

        self.assertEqual(TerrainImage.objects.filter(is_base=True).count(), len(TYPES_WITH_IMAGES))
        self.assertIn("/assets/tiles/forest/tile.json", uploaded_tile_manifest())

    def test_tables_bring_the_effects_into_a_maintained_database(self) -> None:
        self.seed(table=EFFECT_TABLES)

        effects: list[Effect] = list(Effect.objects.all())
        self.assertEqual([effect.key for effect in effects], EXPLOSIONS)
        self.assertEqual([effect_problems(effect) for effect in effects], [[], [], []])
        self.assertIn("/assets/effects/explosion_1/0.png", uploaded_effect_manifest())

    def test_tables_are_skipped_when_they_have_rows(self) -> None:
        UnitSprite.objects.create(key="eigenes")

        with self.assertRaisesMessage(CommandError, "schon Inhalt in api_unitsprite"):
            self.seed(table=SPRITE_TABLES)

        self.assertEqual(list(UnitSprite.objects.values_list("key", flat=True)), ["eigenes"])

    def test_unknown_table_is_an_error(self) -> None:
        with self.assertRaisesMessage(CommandError, "Nicht im Dump: api_army"):
            self.seed(table=["api_army"])

    def test_there_are_no_modes_any_more(self) -> None:
        # Laden nur von Hand: kein --mode, mit dem der Migrator es steuern koennte.
        with self.assertRaises(TypeError):
            self.seed(mode="if-empty")

    def test_dump_may_only_set_copy_and_setval(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fixture: Path = Path(directory) / "seed.sql"
            fixture.write_text(
                "SET client_encoding = 'UTF8';\n"
                "COPY api_species (id, name) FROM stdin;\n"
                "1\tMenschen\n"
                "\\.\n"
                "DROP TABLE api_species;\n",
                encoding="utf-8",
            )
            with mock.patch.object(seed, "FIXTURE", fixture):
                with self.assertRaisesMessage(CommandError, "seed.sql:5: unerwartete Anweisung"):
                    self.seed()

        self.assertEqual(Species.objects.count(), 0)
