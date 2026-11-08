import io
import json
from typing import TextIO

from django.core.management import call_command
from rest_framework.test import APITestCase

from api.management.commands.seed import FIXTURE
from api.models import Species, TerrainCode, TerrainType


class SeedCommandTests(APITestCase):
    """management/commands/seed.py: wann der Dump api/fixtures/seed.json geladen wird."""

    def seed(self, mode: str) -> None:
        call_command("seed", mode=mode, stdout=io.StringIO(), verbosity=0)

    def fixture_count(self, label: str) -> int:
        f: TextIO
        with FIXTURE.open(encoding="utf-8") as f:
            return sum(1 for entry in json.load(f) if entry["model"] == label)

    def test_dump_brings_every_terrain_type(self) -> None:
        # Sonst scheitert in einer frisch befuellten Datenbank jeder Spielstart
        # (instance_client._terrain_document).
        self.seed("always")

        self.assertEqual(list(TerrainType.objects.values_list("code", flat=True)), TerrainCode.values)

    def test_if_empty_loads_into_a_new_database(self) -> None:
        self.seed("if-empty")

        self.assertEqual(TerrainType.objects.count(), self.fixture_count("api.terraintype"))
        self.assertEqual(Species.objects.count(), self.fixture_count("api.species"))

    def test_if_empty_leaves_existing_content_alone(self) -> None:
        Species.objects.create(name="Gepflegt")

        self.seed("if-empty")

        self.assertEqual(list(Species.objects.values_list("name", flat=True)), ["Gepflegt"])
        self.assertEqual(TerrainType.objects.count(), 0)

    def test_never_loads_nothing(self) -> None:
        self.seed("never")

        self.assertEqual(TerrainType.objects.count(), 0)

    def test_always_overwrites_with_the_dump(self) -> None:
        self.seed("always")
        forest: TerrainType = TerrainType.objects.get(code="forest")
        dumped_cover: int = forest.cover
        forest.cover = dumped_cover + 1
        forest.save()

        self.seed("always")

        self.assertEqual(TerrainType.objects.get(code="forest").cover, dumped_cover)
