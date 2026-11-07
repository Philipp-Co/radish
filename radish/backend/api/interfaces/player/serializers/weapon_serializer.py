# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

from rest_framework import serializers

from ....models import Weapon


class WeaponSerializer(serializers.ModelSerializer):
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
        ]
