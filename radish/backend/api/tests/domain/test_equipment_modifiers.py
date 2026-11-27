import unittest

from api.domain.equipment_modifiers import (
    MAX_GAME_VALUE,
    ProfileValues,
    WeaponValues,
    modified_profile,
    modified_weapon,
)
from api.models import EntityProfile, Equipment, Weapon


class EquipmentModifierTests(unittest.TestCase):
    """equipment_modifiers: wie die Ausruestung die Werte im Spiel veraendert."""

    # Ungespeichert: die Rechnung liest nur Felder.
    profile = EntityProfile(health=2, armor=3, toughness=3, strength=4, accuracy=3)
    rifle = Weapon(shots=1, strength=3, min_range=6, max_range=24, armor_penetration=1)

    def test_without_equipment_the_values_stay(self) -> None:
        self.assertEqual(modified_profile(self.profile, []), ProfileValues(2, 3, 3, 4, 3))
        self.assertEqual(modified_weapon(self.rifle, []), WeaponValues(1, 3, 24, 1))

    def test_modifiers_of_all_items_add_up(self) -> None:
        armor = Equipment(armor_modifier=2, accuracy_modifier=-1, weapon_shots_modifier=1)
        scope = Equipment(accuracy_modifier=2, weapon_range_modifier=6, weapon_armor_penetration_modifier=1)

        self.assertEqual(modified_profile(self.profile, [armor, scope]), ProfileValues(2, 5, 3, 4, 4))
        self.assertEqual(modified_weapon(self.rifle, [armor, scope]), WeaponValues(2, 3, 30, 2))

    def test_profile_modifiers_leave_weapons_alone(self) -> None:
        stims = Equipment(strength_modifier=3)

        # Die Staerke des Profils, nicht die der Waffe.
        self.assertEqual(modified_profile(self.profile, [stims]).strength, 7)
        self.assertEqual(modified_weapon(self.rifle, [stims]).strength, 3)

    def test_no_value_falls_below_zero(self) -> None:
        heavy = Equipment(health_modifier=-5, weapon_shots_modifier=-3, weapon_armor_penetration_modifier=-2)

        self.assertEqual(modified_profile(self.profile, [heavy]).health, 0)
        weapon: WeaponValues = modified_weapon(self.rifle, [heavy])
        self.assertEqual((weapon.shots, weapon.armor_penetration), (0, 0))

    def test_max_range_stays_at_least_min_range(self) -> None:
        short = Equipment(weapon_range_modifier=-30)

        self.assertEqual(modified_weapon(self.rifle, [short]).max_range, 6)

    def test_no_value_exceeds_what_the_game_holds(self) -> None:
        # Zwei Gegenstaende mit dem groessten SmallIntegerField reichen darueber.
        big = Equipment(toughness_modifier=32767)
        bigger = Equipment(toughness_modifier=32767)

        self.assertEqual(modified_profile(self.profile, [big, bigger]).toughness, MAX_GAME_VALUE)
