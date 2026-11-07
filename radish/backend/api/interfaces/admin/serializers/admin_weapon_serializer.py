from rest_framework import serializers

from ....models import (
    Species,
    Weapon,
)

from .catalog_admin_serializer import _CatalogAdminSerializer, _value


class AdminWeaponSerializer(_CatalogAdminSerializer):
    species_id = serializers.PrimaryKeyRelatedField(
        queryset=Species.objects.all(), source="species"
    )
    species_name = serializers.CharField(source="species.name", read_only=True)

    immutable_fields = ("species",)

    class Meta:
        model = Weapon
        fields = [
            "id",
            "species_id",
            "species_name",
            "name",
            "weapon_class",
            "shots",
            "strength",
            "min_range",
            "max_range",
            "armor_penetration",
            "cost",
            "in_use",
        ]

    def validate(self, attrs):
        attrs = super().validate(attrs)
        if _value(self, attrs, "min_range") > _value(self, attrs, "max_range"):
            raise serializers.ValidationError(
                {"min_range": "Darf nicht groesser als die Maximalreichweite sein."}
            )
        return attrs
