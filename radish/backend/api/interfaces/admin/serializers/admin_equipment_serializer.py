# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

from rest_framework import serializers

from api.interfaces.admin.serializers.catalog_admin_serializer import _RestrictedCatalogAdminSerializer
from api.models import Equipment, Species

# Modifikatoren eines Gegenstands -- dieselben Felder wie in der Spieler-API
# (player/serializers/equipment_serializer.py).
EQUIPMENT_MODIFIER_FIELDS: list[str] = [
    "health_modifier",
    "armor_modifier",
    "toughness_modifier",
    "strength_modifier",
    "accuracy_modifier",
    "weapon_shots_modifier",
    "weapon_strength_modifier",
    "weapon_range_modifier",
    "weapon_armor_penetration_modifier",
]


class AdminEquipmentSerializer(_RestrictedCatalogAdminSerializer):
    species_id = serializers.PrimaryKeyRelatedField(
        queryset=Species.objects.all(), source="species"
    )
    species_name = serializers.CharField(source="species.name", read_only=True)

    immutable_fields = ("species",)

    class Meta:
        model = Equipment
        fields = [
            "id",
            "species_id",
            "species_name",
            "name",
            "cost",
            *EQUIPMENT_MODIFIER_FIELDS,
            "unit_type_ids",
            "profile_ids",
            "in_use",
        ]
