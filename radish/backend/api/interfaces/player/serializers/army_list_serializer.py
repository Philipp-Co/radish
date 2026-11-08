from rest_framework import serializers

from api.models import Army


class ArmyListSerializer(serializers.ModelSerializer):
    species_name = serializers.CharField(source="species.name", read_only=True)
    unit_count = serializers.IntegerField(read_only=True)
    total_cost = serializers.IntegerField(read_only=True)

    class Meta:
        model = Army
        fields = ["id", "name", "species_id", "species_name", "unit_count", "total_cost"]
