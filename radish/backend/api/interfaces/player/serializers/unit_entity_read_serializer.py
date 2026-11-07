from rest_framework import serializers

from ....models import (
    UnitEntity,
    WeaponClass,
)


class UnitEntityReadSerializer(serializers.ModelSerializer):
    """weapon_ids/heavy_weapon_ids/super_heavy_weapon_ids: Waffen je Slotklasse, nach Slot."""

    weapon_ids = serializers.SerializerMethodField()
    heavy_weapon_ids = serializers.SerializerMethodField()
    super_heavy_weapon_ids = serializers.SerializerMethodField()
    equipment_ids = serializers.SerializerMethodField()
    total_cost = serializers.IntegerField(read_only=True)

    class Meta:
        model = UnitEntity
        fields = [
            "profile_id",
            "weapon_ids",
            "heavy_weapon_ids",
            "super_heavy_weapon_ids",
            "equipment_ids",
            "total_cost",
        ]

    @staticmethod
    def _ids(obj: UnitEntity, weapon_class: WeaponClass) -> list[int]:
        return [slot.weapon_id for slot in obj.weapons.all() if slot.weapon_class == weapon_class]

    def get_weapon_ids(self, obj: UnitEntity) -> list[int]:
        return self._ids(obj, WeaponClass.STANDARD)

    def get_heavy_weapon_ids(self, obj: UnitEntity) -> list[int]:
        return self._ids(obj, WeaponClass.HEAVY)

    def get_super_heavy_weapon_ids(self, obj: UnitEntity) -> list[int]:
        return self._ids(obj, WeaponClass.SUPER_HEAVY)

    def get_equipment_ids(self, obj: UnitEntity) -> list[int]:
        return [slot.equipment_id for slot in obj.equipment.all()]
