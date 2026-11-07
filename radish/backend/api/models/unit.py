from django.db import models

from .army import Army
from .unit_type import UnitType


class Unit(models.Model):
    """
    Eine Einheit in einer Armee: ein Einheitentyp und 1 bis MAX_UNIT_ENTITIES
    Entitaeten. Alle Waffen, auch schwere und super-schwere, tragen die
    Entitaeten (siehe UnitEntityWeapon).
    """

    army = models.ForeignKey(Army, on_delete=models.CASCADE, related_name="units")
    position = models.PositiveSmallIntegerField()
    unit_type = models.ForeignKey(UnitType, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["army", "position"]

    def total_cost(self):
        return sum(entity.total_cost() for entity in self.entities.all())
