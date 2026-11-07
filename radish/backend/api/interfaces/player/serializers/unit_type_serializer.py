# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

from rest_framework import serializers

from ....models import UnitType

from .entity_profile_serializer import EntityProfileSerializer


class UnitTypeSerializer(serializers.ModelSerializer):
    profiles = EntityProfileSerializer(many=True, read_only=True)

    class Meta:
        model = UnitType
        fields = [
            "id",
            "name",
            "min_entities",
            "max_entities",
            "max_heavy_weapons",
            "max_super_heavy_weapons",
            "transport_capacity",
            "can_capture_objectives",
            "movement",
            "sight_range",
            "profiles",
        ]
