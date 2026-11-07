# Katalogeintraege einer Spezies sind gleich aufgebaut (Meta, Constraint);
# eine gemeinsame Basis braeuchte eine Migration.
# pylint: disable=duplicate-code

from django.db import models

from .species import Species


class Equipment(models.Model):
    """
    Ein Ausruestungsgegenstand im Katalog einer Spezies. Soll spaeter Werte
    der Einheit veraendern -- die Modifikatoren dafuer kommen erst mit
    diesem Schritt dazu, bis dahin traegt er nur seinen Namen.
    """

    species = models.ForeignKey(
        Species, on_delete=models.PROTECT, related_name="equipment"
    )
    name = models.CharField(max_length=255)
    cost = models.PositiveIntegerField()

    class Meta:
        ordering = ["species", "name"]
        constraints = [
            models.UniqueConstraint(
                fields=["species", "name"], name="unique_equipment_per_species"
            ),
        ]

    def __str__(self) -> str:
        return f"{self.name} ({self.species})"
