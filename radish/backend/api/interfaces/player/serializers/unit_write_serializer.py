from rest_framework import serializers

from ....models import UnitType

from .unit_entity_write_serializer import UnitEntityWriteSerializer


class UnitWriteSerializer(serializers.Serializer):
    unit_type_id = serializers.PrimaryKeyRelatedField(
        queryset=UnitType.objects.all(), source="unit_type"
    )
    entities = UnitEntityWriteSerializer(many=True, allow_empty=False)
