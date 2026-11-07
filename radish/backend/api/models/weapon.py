from django.db import models

from .species import Species
from .weapon_class import WeaponClass


class Weapon(models.Model):
    """
    Eine Waffe im Katalog einer Spezies. Alle Klassen (siehe WeaponClass)
    haben dieselben Werte und unterscheiden sich nur darin, wer sie traegt
    -- deshalb ein Modell mit weapon_class statt eines je Klasse.
    """

    species = models.ForeignKey(
        Species, on_delete=models.PROTECT, related_name="weapons"
    )
    name = models.CharField(max_length=255)
    weapon_class = models.CharField(
        max_length=20, choices=WeaponClass.choices, default=WeaponClass.STANDARD
    )
    shots = models.PositiveSmallIntegerField()
    strength = models.PositiveSmallIntegerField()
    # Reichweite als Spanne: unterhalb von min_range kann die Waffe nicht
    # feuern (0 = keine Mindestreichweite).
    min_range = models.PositiveSmallIntegerField(default=0)
    max_range = models.PositiveSmallIntegerField()
    armor_penetration = models.PositiveSmallIntegerField()
    cost = models.PositiveIntegerField()

    class Meta:
        ordering = ["species", "name"]
        constraints = [
            models.UniqueConstraint(
                fields=["species", "name"], name="unique_weapon_per_species"
            ),
        ]

    def __str__(self):
        return f"{self.name} ({self.species})"
