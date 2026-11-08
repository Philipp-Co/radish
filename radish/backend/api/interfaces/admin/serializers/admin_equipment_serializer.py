# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

from rest_framework import serializers

from api.interfaces.admin.serializers.catalog_admin_serializer import _CatalogAdminSerializer
from api.models import Equipment, Species


class AdminEquipmentSerializer(_CatalogAdminSerializer):
    species_id = serializers.PrimaryKeyRelatedField(
        queryset=Species.objects.all(), source="species"
    )
    species_name = serializers.CharField(source="species.name", read_only=True)

    immutable_fields = ("species",)

    class Meta:
        model = Equipment
        fields = ["id", "species_id", "species_name", "name", "cost", "in_use"]
