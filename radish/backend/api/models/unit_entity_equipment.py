from django.db import models

from .equipment import Equipment
from .unit_entity import UnitEntity


class UnitEntityEquipment(models.Model):
    entity = models.ForeignKey(UnitEntity, on_delete=models.CASCADE, related_name="equipment")
    slot = models.PositiveSmallIntegerField()
    equipment = models.ForeignKey(Equipment, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["entity", "slot"]
        constraints = [
            models.UniqueConstraint(
                fields=["entity", "slot"], name="unique_equipment_slot_per_entity"
            ),
        ]
