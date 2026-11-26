from rest_framework import serializers

from api.models import EntityProfile, Equipment, Weapon


# Mit den Einschraenkungen fuer Weapon.allows in der Validierung.
_WEAPONS = Weapon.objects.prefetch_related("unit_types", "profiles")


class UnitEntityWriteSerializer(serializers.Serializer):
    profile_id = serializers.PrimaryKeyRelatedField(
        queryset=EntityProfile.objects.select_related("unit_type"), source="profile"
    )
    weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=_WEAPONS),
        source="weapons",
        required=False,
        default=list,
    )
    heavy_weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=_WEAPONS),
        source="heavy_weapons",
        required=False,
        default=list,
    )
    super_heavy_weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=_WEAPONS),
        source="super_heavy_weapons",
        required=False,
        default=list,
    )
    equipment_ids = serializers.ListField(
        # Mit den Einschraenkungen fuer Equipment.allows in der Validierung.
        child=serializers.PrimaryKeyRelatedField(
            queryset=Equipment.objects.prefetch_related("unit_types", "profiles")
        ),
        source="equipment",
        required=False,
        default=list,
    )
