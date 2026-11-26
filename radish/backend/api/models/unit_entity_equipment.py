from django.db import models

from api.models.equipment import Equipment
from api.models.unit_entity import UnitEntity


class UnitEntityEquipment(models.Model):
    """
    Ein Ausruestungsgegenstand einer Entitaet in einem nummerierten Slot.
    Anders als Waffen traegt eine Entitaet jeden Gegenstand hoechstens
    einmal -- seine Modifikatoren sollen sich nicht mit sich selbst stapeln.
    """

    entity = models.ForeignKey(UnitEntity, on_delete=models.CASCADE, related_name="equipment")
    slot = models.PositiveSmallIntegerField()
    equipment = models.ForeignKey(Equipment, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["entity", "slot"]
        constraints = [
            models.UniqueConstraint(
                fields=["entity", "slot"], name="unique_equipment_slot_per_entity"
            ),
            models.UniqueConstraint(
                fields=["entity", "equipment"], name="unique_equipment_per_entity"
            ),
        ]
