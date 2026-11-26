# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

from rest_framework import serializers

from api.models import EntityProfile, Equipment, UnitType


class EquipmentSerializer(serializers.ModelSerializer):
    """
    Ein Gegenstand mit seinen Modifikatoren und wer ihn tragen darf
    (Equipment.allows): unit_type_ids/profile_ids, beide leer = jede
    Entitaet der Spezies.
    """

    unit_type_ids: serializers.PrimaryKeyRelatedField[UnitType] = serializers.PrimaryKeyRelatedField(
        source="unit_types", many=True, read_only=True
    )
    profile_ids: serializers.PrimaryKeyRelatedField[EntityProfile] = serializers.PrimaryKeyRelatedField(
        source="profiles", many=True, read_only=True
    )

    class Meta:
        model = Equipment
        fields = [
            "id",
            "name",
            "cost",
            "health_modifier",
            "armor_modifier",
            "toughness_modifier",
            "strength_modifier",
            "accuracy_modifier",
            "weapon_shots_modifier",
            "weapon_strength_modifier",
            "weapon_range_modifier",
            "weapon_armor_penetration_modifier",
            "unit_type_ids",
            "profile_ids",
        ]
