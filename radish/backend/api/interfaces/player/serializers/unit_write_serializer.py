from rest_framework import serializers

from api.interfaces.player.serializers.unit_entity_write_serializer import UnitEntityWriteSerializer
from api.models import UnitType


class UnitWriteSerializer(serializers.Serializer):
    unit_type_id = serializers.PrimaryKeyRelatedField(
        queryset=UnitType.objects.all(), source="unit_type"
    )
    entities = UnitEntityWriteSerializer(many=True, allow_empty=False)
