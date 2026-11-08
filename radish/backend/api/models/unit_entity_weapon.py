from django.db import models

from api.models.unit_entity import UnitEntity
from api.models.weapon import Weapon
from api.models.weapon_class import WeaponClass


class UnitEntityWeapon(models.Model):
    """
    Eine Waffe einer Entitaet in einem nummerierten Slot ihrer Klasse.
    weapon_class ist die Klasse des Slots, nicht der Waffe: Slot 0 der
    Standard- und Slot 0 der schweren Waffen sind zwei verschiedene Slots.
    """

    entity = models.ForeignKey(UnitEntity, on_delete=models.CASCADE, related_name="weapons")
    weapon_class = models.CharField(
        max_length=20, choices=WeaponClass.choices, default=WeaponClass.STANDARD
    )
    slot = models.PositiveSmallIntegerField()
    weapon = models.ForeignKey(Weapon, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["entity", "weapon_class", "slot"]
        constraints = [
            models.UniqueConstraint(
                fields=["entity", "weapon_class", "slot"], name="unique_weapon_slot_per_entity"
            ),
        ]
