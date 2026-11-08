from rest_framework import serializers

from api.models import EntityProfile, Equipment, Weapon


class UnitEntityWriteSerializer(serializers.Serializer):
    profile_id = serializers.PrimaryKeyRelatedField(
        queryset=EntityProfile.objects.select_related("unit_type"), source="profile"
    )
    weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=Weapon.objects.all()),
        source="weapons",
        required=False,
        default=list,
    )
    heavy_weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=Weapon.objects.all()),
        source="heavy_weapons",
        required=False,
        default=list,
    )
    super_heavy_weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=Weapon.objects.all()),
        source="super_heavy_weapons",
        required=False,
        default=list,
    )
    equipment_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=Equipment.objects.all()),
        source="equipment",
        required=False,
        default=list,
    )
