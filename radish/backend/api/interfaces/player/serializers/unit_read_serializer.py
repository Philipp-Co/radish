from rest_framework import serializers

from api.interfaces.player.serializers.unit_entity_read_serializer import UnitEntityReadSerializer
from api.models import Unit


class UnitReadSerializer(serializers.ModelSerializer):
    entities = UnitEntityReadSerializer(many=True, read_only=True)
    total_cost = serializers.IntegerField(read_only=True)

    class Meta:
        model = Unit
        fields = ["unit_type_id", "entities", "total_cost"]
