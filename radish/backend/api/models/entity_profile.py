from django.core.validators import MaxValueValidator
from django.db import models

from .limits import MAX_UNIT_ENTITIES
from .unit_type import UnitType


class EntityProfile(models.Model):
    """
    Die Werte einer Entitaet innerhalb eines Einheitentyps. min_count/
    max_count begrenzen, wie viele Entitaeten mit diesem Profil eine Einheit
    haben darf -- "genau ein Anfuehrer" ist dann min_count = max_count = 1.
    """

    unit_type = models.ForeignKey(
        UnitType, on_delete=models.CASCADE, related_name="profiles"
    )
    name = models.CharField(max_length=255)
    health = models.PositiveSmallIntegerField()
    armor = models.PositiveSmallIntegerField()
    # Widerstand gegen einen Schuss, der die Ruestung durchschlagen hat: der
    # Schuss wirkt, wenn Wurf + Staerke der Waffe ihn erreicht.
    toughness = models.PositiveSmallIntegerField()
    strength = models.PositiveSmallIntegerField()
    accuracy = models.PositiveSmallIntegerField()
    equipment_slots = models.PositiveSmallIntegerField()
    # Waffenslots je Klasse (siehe WeaponClass): weapon_slots fuer
    # Standardwaffen, dazu schwere und super-schwere Waffen -- alle traegt
    # eine einzelne Entitaet, etwa der Soldat mit der schweren Waffe oder
    # ein Panzer mit Hauptkanone und Seitenwaffen.
    weapon_slots = models.PositiveSmallIntegerField()
    heavy_weapon_slots = models.PositiveSmallIntegerField(default=0)
    super_heavy_weapon_slots = models.PositiveSmallIntegerField(default=0)
    cost = models.PositiveIntegerField()
    min_count = models.PositiveSmallIntegerField(
        default=0, validators=[MaxValueValidator(MAX_UNIT_ENTITIES)]
    )
    max_count = models.PositiveSmallIntegerField(
        default=MAX_UNIT_ENTITIES, validators=[MaxValueValidator(MAX_UNIT_ENTITIES)]
    )

    class Meta:
        ordering = ["unit_type", "id"]
        constraints = [
            models.UniqueConstraint(
                fields=["unit_type", "name"], name="unique_profile_per_unit_type"
            ),
        ]

    def __str__(self):
        return f"{self.name} ({self.unit_type.name})"
