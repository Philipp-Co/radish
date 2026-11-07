from django.db import models

from .player import Player
from .species import Species


class Army(models.Model):
    """
    Eine Armee eines Spielers. Der Name ist nur innerhalb der Armeen
    desselben Spielers eindeutig.
    """

    player = models.ForeignKey(Player, on_delete=models.CASCADE, related_name="armies")
    name = models.CharField(max_length=255)
    species = models.ForeignKey(Species, on_delete=models.PROTECT, related_name="armies")

    class Meta:
        ordering = ["name"]
        constraints = [
            models.UniqueConstraint(
                fields=["player", "name"], name="unique_army_name_per_player"
            ),
        ]

    def __str__(self) -> str:
        return f"{self.name} ({self.player})"

    def total_cost(self) -> int:
        return sum(unit.total_cost() for unit in self.units.all())
