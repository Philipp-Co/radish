from django.core.validators import MaxValueValidator, MinValueValidator
from django.db import models

from .terrain_code import TerrainCode


# Wie weit MV und CV reichen duerfen: in beide Richtungen fuer die Bewegung,
# bis 10 fuer die Deckung. Die Deckung wirkt im Kampf derzeit nicht
# (game/src/control/attack_unit.c).
MAX_TERRAIN_MODIFIER = 10


class TerrainType(models.Model):
    """
    Was ein Tile-Typ fuer das Spiel bedeutet: MV, um wie viele Felder er die
    Bewegungsreichweite aendert (Betreten kostet 1 - MV, mindestens 1), CV,
    die Deckung, die er einer Einheit darauf gibt, ob er unbegehbar ist und
    ob er die Sicht auf die Felder dahinter versperrt. Geht mit jedem Spielstart an die Instanz (instance_client.
    game_start_document).

    Genau eine Zeile je TerrainCode, angelegt aus dem Dump
    api/fixtures/seed.json (siehe management/commands/seed.py). Der Admin
    aendert nur die Werte -- anlegen und loeschen gibt es nicht, weil das
    Spiel jeden Typ braucht.
    """

    code = models.CharField(max_length=20, choices=TerrainCode.choices, unique=True)
    movement_modifier = models.SmallIntegerField(
        default=0,
        validators=[
            MinValueValidator(-MAX_TERRAIN_MODIFIER),
            MaxValueValidator(MAX_TERRAIN_MODIFIER),
        ],
    )
    cover = models.PositiveSmallIntegerField(
        default=0, validators=[MaxValueValidator(MAX_TERRAIN_MODIFIER)]
    )
    impassable = models.BooleanField(default=False)
    blocks_sight = models.BooleanField(default=False)

    class Meta:
        # In der Reihenfolge der Typen im Spiel -- so stehen sie im Dump.
        ordering = ["id"]

    def __str__(self):
        return self.get_code_display()
