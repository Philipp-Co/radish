from django.db import models


class WeaponClass(models.TextChoices):
    """
    Welche Slots eine Waffe belegt: jede Klasse hat am Entitaetsprofil ihre
    eigene Slotzahl (EntityProfile.weapon_slots, heavy_weapon_slots,
    super_heavy_weapon_slots) -- getragen werden alle Waffen von einzelnen
    Entitaeten, nicht von der Einheit.
    """

    STANDARD = "standard", "Waffe"
    HEAVY = "heavy", "Schwere Waffe"
    SUPER_HEAVY = "super_heavy", "Super-schwere Waffe"
