from rest_framework import status

from api.models import Army, Unit
from api.tests.army_test_case import ArmyTestCase
from api.tests.helpers import EntityData, EntityPayload, TestResponse, UnitData, UnitPayload


# Jede Testmethode zaehlt als oeffentliche Methode.
# pylint: disable-next=too-many-public-methods
class ArmyValidationTests(ArmyTestCase):
    """
    Die Regeln aus army_write_serializer.ArmyWriteSerializer.validate -- jede
    Verletzung muss mit 400 abgelehnt werden, ohne etwas anzulegen.
    """

    # Benannt wie die assert*-Methoden von unittest.
    # pylint: disable-next=invalid-name
    def assertRejected(self, units: list[UnitPayload]) -> None:
        response: TestResponse = self.create_army(self.army_payload(units=units))

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST, response.data)
        self.assertFalse(Army.objects.exists())

    def test_unit_without_entities(self) -> None:
        self.assertRejected([self.squad_payload(entities=[])])

    def test_too_few_entities(self) -> None:
        self.assertRejected([self.squad_payload(soldiers=0)])

    def test_more_than_ten_entities(self) -> None:
        self.assertRejected([self.squad_payload(soldiers=10)])

    def test_ten_entities_are_allowed(self) -> None:
        response: TestResponse = self.create_army(self.army_payload(units=[self.squad_payload(soldiers=9)]))

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)

    def test_more_entities_than_unit_type_allows(self) -> None:
        scouts: UnitPayload = {
            "unit_type_id": self.scout_team.id,
            "entities": [{"profile_id": self.scout.id}] * 4,
        }
        self.assertRejected([scouts])

    def test_profile_count_limits(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"].append(unit["entities"][0].copy())  # zweiter Anfuehrer
        self.assertRejected([unit])

    def test_profile_of_other_unit_type(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["profile_id"] = self.scout.id
        self.assertRejected([unit])

    def test_too_many_weapons(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.rifle.id] * 3
        self.assertRejected([unit])

    def test_too_much_equipment(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["equipment_ids"] = [self.medkit.id] * 2
        self.assertRejected([unit])

    def test_heavy_weapon_in_entity_slot(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.cannon.id]
        self.assertRejected([unit])

    def test_heavy_weapon_on_entity(self) -> None:
        response: TestResponse = self.create_army(
            self.army_payload(units=[self.squad_payload(heavy=[self.cannon.id])])
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        leader: EntityData
        soldier: EntityData
        leader, soldier = response.data["units"][0]["entities"]
        self.assertEqual(leader["heavy_weapon_ids"], [self.cannon.id])
        self.assertEqual(leader["super_heavy_weapon_ids"], [])
        self.assertEqual(leader["weapon_ids"], [self.rifle.id])
        self.assertEqual(soldier["heavy_weapon_ids"], [])
        self.assertNotIn("heavy_weapon_ids", response.data["units"][0])

    def test_heavy_and_super_heavy_weapons_together(self) -> None:
        response: TestResponse = self.create_army(
            self.army_payload(
                units=[
                    self.squad_payload(
                        heavy=[self.cannon.id],
                        super_heavy=[self.battle_cannon.id],
                    )
                ]
            )
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        unit: UnitData = response.data["units"][0]
        leader: EntityData = unit["entities"][0]
        self.assertEqual(leader["heavy_weapon_ids"], [self.cannon.id])
        self.assertEqual(leader["super_heavy_weapon_ids"], [self.battle_cannon.id])
        # Anfuehrer 20 + 1 + 3 + Kanone 15 + Kampfgeschuetz 40, Soldat 10 + 2.
        self.assertEqual(leader["total_cost"], 79)
        self.assertEqual(unit["total_cost"], 91)

    def test_several_heavy_weapon_slots(self) -> None:
        self.leader.heavy_weapon_slots = 2
        self.leader.save(update_fields=["heavy_weapon_slots"])

        response: TestResponse = self.create_army(
            self.army_payload(
                units=[self.squad_payload(heavy=[self.cannon.id, self.cannon.id])]
            )
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(
            response.data["units"][0]["entities"][0]["heavy_weapon_ids"],
            [self.cannon.id, self.cannon.id],
        )

    def test_more_weapons_than_the_game_allows(self) -> None:
        # Ein Profil mit mehr Slots, als das Spiel kennt -- so legt es der
        # Katalog nicht mehr an, aber es kann aus der Zeit davor stammen.
        self.soldier.weapon_slots = 5
        self.soldier.save(update_fields=["weapon_slots"])
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.rifle.id] * 5
        self.assertRejected([unit])

    def test_more_heavy_weapons_than_slots(self) -> None:
        self.assertRejected([self.squad_payload(heavy=[self.cannon.id] * 2)])

    def test_normal_weapon_as_heavy_weapon(self) -> None:
        self.assertRejected([self.squad_payload(heavy=[self.rifle.id])])

    def test_super_heavy_weapon_in_heavy_slot(self) -> None:
        self.assertRejected([self.squad_payload(heavy=[self.battle_cannon.id])])

    def test_heavy_weapon_in_super_heavy_slot(self) -> None:
        self.assertRejected([self.squad_payload(super_heavy=[self.cannon.id])])

    def test_super_heavy_weapon_in_entity_slot(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.battle_cannon.id]
        self.assertRejected([unit])

    def weapon_carrier_squad(self, carriers: int, heavy_per_carrier: int = 1) -> UnitPayload:
        """Trupp mit Anfuehrer und `carriers` Soldaten, die je schwere Waffen tragen."""
        self.soldier.heavy_weapon_slots = 1
        self.soldier.save(update_fields=["heavy_weapon_slots"])
        unit: UnitPayload = self.squad_payload(soldiers=carriers)
        entity: EntityPayload
        for entity in unit["entities"][1:]:
            entity["heavy_weapon_ids"] = [self.cannon.id] * heavy_per_carrier
        return unit

    def test_unit_limit_for_heavy_weapons(self) -> None:
        self.squad.max_heavy_weapons = 2
        self.squad.save(update_fields=["max_heavy_weapons"])

        self.assertRejected([self.weapon_carrier_squad(carriers=3)])

    def test_unit_limit_can_be_reached(self) -> None:
        self.squad.max_heavy_weapons = 2
        self.squad.save(update_fields=["max_heavy_weapons"])

        response: TestResponse = self.create_army(self.army_payload(units=[self.weapon_carrier_squad(carriers=2)]))

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)

    def test_without_unit_limit_only_slots_count(self) -> None:
        response: TestResponse = self.create_army(self.army_payload(units=[self.weapon_carrier_squad(carriers=5)]))

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)

    def test_unit_limit_zero_forbids_super_heavy(self) -> None:
        self.squad.max_super_heavy_weapons = 0
        self.squad.save(update_fields=["max_super_heavy_weapons"])

        self.assertRejected([self.squad_payload(super_heavy=[self.battle_cannon.id])])

    def test_heavy_weapon_on_profile_without_heavy_slot(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["heavy_weapon_ids"] = [self.cannon.id]
        self.assertRejected([unit])

    def test_weapon_of_other_species(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [self.alien_weapon.id]
        self.assertRejected([unit])

    def test_unit_type_of_other_species(self) -> None:
        response: TestResponse = self.create_army(
            {"name": "Mischmasch", "species_id": self.aliens.id, "units": [self.squad_payload()]}
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_unknown_weapon(self) -> None:
        unit: UnitPayload = self.squad_payload()
        unit["entities"][1]["weapon_ids"] = [999999]
        self.assertRejected([unit])

    def test_failed_update_keeps_old_units(self) -> None:
        army_id: int = self.create_army().data["id"]

        response: TestResponse = self.client.put(
            f"/api/client/armies/{army_id}/",
            self.army_payload(units=[self.squad_payload(soldiers=0)]),
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertEqual(Unit.objects.filter(army_id=army_id).count(), 1)
