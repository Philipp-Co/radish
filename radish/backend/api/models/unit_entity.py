from django.db import models

from .entity_profile import EntityProfile
from .unit import Unit


class UnitEntity(models.Model):
    """
    Eine einzelne Entitaet einer Einheit. Ihre Werte kommen aus dem Profil;
    Waffen und Ausruestung liegen in nummerierten Slots (UnitEntityWeapon,
    UnitEntityEquipment), damit dieselbe Waffe auch zweimal getragen werden
    kann -- ein ManyToManyField ohne Slot liesse das nicht zu.
    """

    unit = models.ForeignKey(Unit, on_delete=models.CASCADE, related_name="entities")
    position = models.PositiveSmallIntegerField()
    profile = models.ForeignKey(EntityProfile, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["unit", "position"]

    def total_cost(self) -> int:
        return (
            self.profile.cost
            + sum(slot.weapon.cost for slot in self.weapons.all())
            + sum(slot.equipment.cost for slot in self.equipment.all())
        )
