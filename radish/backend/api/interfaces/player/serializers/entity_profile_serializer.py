from rest_framework import serializers

from ....models import EntityProfile


class EntityProfileSerializer(serializers.ModelSerializer):
    class Meta:
        model = EntityProfile
        fields = [
            "id",
            "name",
            "health",
            "armor",
            "toughness",
            "strength",
            "accuracy",
            "equipment_slots",
            "weapon_slots",
            "heavy_weapon_slots",
            "super_heavy_weapon_slots",
            "cost",
            "min_count",
            "max_count",
        ]
