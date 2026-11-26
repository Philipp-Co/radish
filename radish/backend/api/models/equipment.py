# Katalogeintraege einer Spezies sind gleich aufgebaut (Meta, Constraint);
# eine gemeinsame Basis braeuchte eine Migration.
# pylint: disable=duplicate-code

from django.db import models

from api.models.profile_restricted import ProfileRestricted
from api.models.species import Species


class Equipment(ProfileRestricted):
    """
    Ein Ausruestungsgegenstand im Katalog einer Spezies. Er veraendert die
    Werte der Entitaet, die ihn traegt, um feste Betraege (auch negativ):
    die *_modifier ihres Profils und die weapon_*_modifier jeder ihrer Waffen,
    gleich welcher Klasse. Eine Entitaet traegt jeden Gegenstand hoechstens
    einmal (UnitEntityEquipment), Modifikatoren verschiedener Gegenstaende
    addieren sich.

    Wer ihn tragen darf, legen unit_types/profiles fest (ProfileRestricted).
    """

    species = models.ForeignKey(
        Species, on_delete=models.PROTECT, related_name="equipment"
    )
    name = models.CharField(max_length=255)
    cost = models.PositiveIntegerField()

    # Auf die Werte des Entitaetsprofils.
    health_modifier = models.SmallIntegerField(default=0)
    armor_modifier = models.SmallIntegerField(default=0)
    toughness_modifier = models.SmallIntegerField(default=0)
    strength_modifier = models.SmallIntegerField(default=0)
    accuracy_modifier = models.SmallIntegerField(default=0)

    # Auf jede Waffe der Entitaet; die Reichweite meint die Maximalreichweite.
    weapon_shots_modifier = models.SmallIntegerField(default=0)
    weapon_strength_modifier = models.SmallIntegerField(default=0)
    weapon_range_modifier = models.SmallIntegerField(default=0)
    weapon_armor_penetration_modifier = models.SmallIntegerField(default=0)

    class Meta(ProfileRestricted.Meta):
        ordering = ["species", "name"]
        constraints = [
            models.UniqueConstraint(
                fields=["species", "name"], name="unique_equipment_per_species"
            ),
        ]

    def __str__(self) -> str:
        return f"{self.name} ({self.species})"
