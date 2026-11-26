# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

from rest_framework import serializers

from api.models import EntityProfile, UnitType, Weapon


class WeaponSerializer(serializers.ModelSerializer):
    """
    Eine Waffe mit ihren Werten und wer sie tragen darf (Weapon.allows):
    unit_type_ids/profile_ids, beide leer = jede Entitaet der Spezies.
    """

    unit_type_ids: serializers.PrimaryKeyRelatedField[UnitType] = serializers.PrimaryKeyRelatedField(
        source="unit_types", many=True, read_only=True
    )
    profile_ids: serializers.PrimaryKeyRelatedField[EntityProfile] = serializers.PrimaryKeyRelatedField(
        source="profiles", many=True, read_only=True
    )

    class Meta:
        model = Weapon
        fields = [
            "id",
            "name",
            "weapon_class",
            "shots",
            "strength",
            "min_range",
            "max_range",
            "armor_penetration",
            "cost",
            "unit_type_ids",
            "profile_ids",
        ]
