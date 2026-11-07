from collections.abc import Sequence
from typing import Unpack

from rest_framework.test import APITestCase

from api.models import (
    EntityProfile,
    Equipment,
    Species,
    UnitType,
    Weapon,
)

from api.tests.helpers import (
    authenticate,
    ArmyPayload,
    EntityPayload,
    TestResponse,
    UnitPayload,
    UnitPayloadOverrides,
    PLAYER_TOKEN,
    User,
    create_test_catalog,
)


# Die Attribute sind der Testkatalog, den die Tests direkt ansprechen.
# pylint: disable-next=too-many-instance-attributes
class ArmyTestCase(APITestCase):
    """
    Gemeinsamer Katalog fuer die Armee-Tests: die Spezies "Menschen" (aus
    create_test_catalog()) mit einem Trupp aus Soldaten und genau einem
    Anfuehrer, dazu eine zweite Spezies, deren Eintraege in einer Armee der
    Menschen nichts verloren haben.
    """

    def setUp(self) -> None:
        create_test_catalog()
        self.user = User.objects.create(username="army-user")
        authenticate(self.client, self.user, PLAYER_TOKEN)

        self.humans = Species.objects.get(name="Menschen")
        self.squad = UnitType.objects.create(
            species=self.humans,
            name="Trupp",
            min_entities=2,
            max_entities=10,
            can_capture_objectives=True,
            movement=6,
            sight_range=4,
        )
        self.soldier = EntityProfile.objects.create(
            unit_type=self.squad, name="Soldat", health=1, armor=3, toughness=3, strength=3, accuracy=4,
            equipment_slots=1, weapon_slots=2, cost=10, min_count=1, max_count=9,
        )
        self.leader = EntityProfile.objects.create(
            unit_type=self.squad, name="Anfuehrer", health=2, armor=3, toughness=3, strength=4, accuracy=3,
            equipment_slots=2, weapon_slots=1, heavy_weapon_slots=1, super_heavy_weapon_slots=1,
            cost=20, min_count=1, max_count=1,
        )
        self.scout_team = UnitType.objects.create(
            species=self.humans, name="Spaeher", min_entities=1, max_entities=3, movement=8, sight_range=10
        )
        self.scout = EntityProfile.objects.create(
            unit_type=self.scout_team, name="Spaeher", health=1, armor=2, toughness=3, strength=3, accuracy=3,
            equipment_slots=0, weapon_slots=1, cost=8,
        )
        self.truck_type = UnitType.objects.create(
            species=self.humans, name="Transporter", min_entities=1, max_entities=1,
            transport_capacity=2, movement=12, sight_range=3,
        )
        self.truck = EntityProfile.objects.create(
            unit_type=self.truck_type, name="Lastwagen", health=8, armor=5, toughness=3, strength=5, accuracy=5,
            equipment_slots=0, weapon_slots=0, cost=50,
        )
        self.rifle = Weapon.objects.create(
            species=self.humans, name="Gewehr", shots=1, strength=3, max_range=24,
            armor_penetration=0, cost=1,
        )
        self.cannon = Weapon.objects.create(
            species=self.humans, name="Kanone", weapon_class="heavy", shots=1,
            strength=9, max_range=48, armor_penetration=3, cost=15,
        )
        self.battle_cannon = Weapon.objects.create(
            species=self.humans, name="Kampfgeschuetz", weapon_class="super_heavy", shots=1,
            strength=12, min_range=6, max_range=72, armor_penetration=5, cost=40,
        )
        self.medkit = Equipment.objects.create(species=self.humans, name="Medipack", cost=3)

        self.aliens = Species.objects.create(name="Fremde")
        self.alien_weapon = Weapon.objects.create(
            species=self.aliens, name="Strahler", shots=2, strength=4, max_range=18,
            armor_penetration=1, cost=2,
        )

    def squad_payload(
        self, soldiers: int = 1, heavy: Sequence[int] = (), super_heavy: Sequence[int] = (),
        **overrides: Unpack[UnitPayloadOverrides],
    ) -> UnitPayload:
        """Trupp aus Anfuehrer (traegt heavy/super_heavy) und soldiers Soldaten."""
        # Erst die Liste mit dem Anfuehrer, dann die Soldaten anhaengen: bei
        # "[...] + [...]" leitet mypy den linken Operanden ohne den Typ
        # EntityPayload her.
        entities: list[EntityPayload] = [
            {
                "profile_id": self.leader.id,
                "weapon_ids": [self.rifle.id],
                "heavy_weapon_ids": list(heavy),
                "super_heavy_weapon_ids": list(super_heavy),
                "equipment_ids": [self.medkit.id],
            },
        ]
        entities += [
            {"profile_id": self.soldier.id, "weapon_ids": [self.rifle.id, self.rifle.id]}
            for _ in range(soldiers)
        ]
        unit: UnitPayload = {
            "unit_type_id": self.squad.id,
            "entities": entities,
        }
        unit.update(overrides)
        return unit

    def army_payload(
        self, units: list[UnitPayload] | None = None, name: str = "Erste Kompanie"
    ) -> ArmyPayload:
        return {
            "name": name,
            "species_id": self.humans.id,
            "units": [self.squad_payload()] if units is None else units,
        }

    def create_army(self, payload: ArmyPayload | None = None) -> TestResponse:
        return self.client.post(
            "/api/client/armies/", payload or self.army_payload(), format="json"
        )
