# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

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

    # Die Stubs typisieren attrs als Any; es sind die validierten Daten, ein
    # dict von Feldname auf Wert.
    def validate(self, attrs: dict[str, object]) -> dict[str, object]:
        attrs = super().validate(attrs)
        if _value(self, attrs, "min_range", int) > _value(self, attrs, "max_range", int):
            raise serializers.ValidationError(
                {"min_range": "Darf nicht groesser als die Maximalreichweite sein."}
            )
        return attrs
