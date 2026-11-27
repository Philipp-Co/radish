"""
Die Werte einer Entitaet und ihrer Waffen mit den Modifikatoren ihrer
Ausruestung (api/models/equipment.py). Das Spiel kennt keine Modifikatoren:
es bekommt beim Start die fertigen Werte (instance_client.game_start_document).

Dieselben Regeln wie die Anzeige im Armee-Editor (radish/frontend/src/app/
pages/games/armies/edit/army-draft.ts, profileStatCells/modifiedWeapon): die
Modifikatoren aller Gegenstaende addieren sich, kein Wert faellt unter 0, die
Maximalreichweite nicht unter die Mindestreichweite. Dazu hier die Obergrenze
eines Spielwerts ($defs/wert in radish/game/schema/spielstart.schema.json),
sonst lehnte die Instanz den Start ab.
"""

# ProfileValues nennt dieselben Werte wie das Profil in den Test-Helfern
# (api/tests/helpers.py) -- gleiche Fachbegriffe, kein kopierter Code.
# pylint: disable=duplicate-code

from collections.abc import Sequence
from dataclasses import dataclass

from api.models import EntityProfile, Equipment, Weapon

# $defs/wert: ein int16_t im Spiel.
MAX_GAME_VALUE = 32767


@dataclass(frozen=True)
class ProfileValues:
    """Die Werte eines Profils mit Ausruestung."""

    health: int
    armor: int
    toughness: int
    strength: int
    accuracy: int


@dataclass(frozen=True)
class WeaponValues:
    """Die veraenderlichen Werte einer Waffe mit der Ausruestung ihres Traegers."""

    shots: int
    strength: int
    max_range: int
    armor_penetration: int


def modified_profile(profile: EntityProfile, items: Sequence[Equipment]) -> ProfileValues:
    """Die Werte von profile mit den *_modifier aller items."""
    return ProfileValues(
        health=_modified(profile.health, items, "health_modifier"),
        armor=_modified(profile.armor, items, "armor_modifier"),
        toughness=_modified(profile.toughness, items, "toughness_modifier"),
        strength=_modified(profile.strength, items, "strength_modifier"),
        accuracy=_modified(profile.accuracy, items, "accuracy_modifier"),
    )


def modified_weapon(weapon: Weapon, items: Sequence[Equipment]) -> WeaponValues:
    """
    Die Werte von weapon mit den weapon_*_modifier aller items. Die
    Mindestreichweite bleibt, wie sie ist.
    """
    return WeaponValues(
        shots=_modified(weapon.shots, items, "weapon_shots_modifier"),
        strength=_modified(weapon.strength, items, "weapon_strength_modifier"),
        max_range=_modified(weapon.max_range, items, "weapon_range_modifier", minimum=weapon.min_range),
        armor_penetration=_modified(
            weapon.armor_penetration, items, "weapon_armor_penetration_modifier"
        ),
    )


def _modified(value: int, items: Sequence[Equipment], modifier: str, minimum: int = 0) -> int:
    total: int = value + sum(getattr(item, modifier) for item in items)
    return max(minimum, min(MAX_GAME_VALUE, total))
