# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

from rest_framework import serializers

from ....models import (
    Species,
    UnitType,
)

from .admin_entity_profile_serializer import AdminEntityProfileSerializer
from .catalog_admin_serializer import _CatalogAdminSerializer, _value


class AdminUnitTypeSerializer(_CatalogAdminSerializer):
    species_id = serializers.PrimaryKeyRelatedField(
        queryset=Species.objects.all(), source="species"
    )
    species_name = serializers.CharField(source="species.name", read_only=True)
    profiles = AdminEntityProfileSerializer(many=True, read_only=True)

    immutable_fields = ("species",)

    class Meta:
        model = UnitType
        fields = [
            "id",
            "species_id",
            "species_name",
            "name",
            "min_entities",
            "max_entities",
            "max_heavy_weapons",
            "max_super_heavy_weapons",
            "transport_capacity",
            "can_capture_objectives",
            "movement",
            "sight_range",
            "in_use",
            "profiles",
        ]

    # Die Stubs typisieren attrs als Any; es sind die validierten Daten, ein
    # dict von Feldname auf Wert.
    def validate(self, attrs: dict[str, object]) -> dict[str, object]:
        attrs = super().validate(attrs)
        max_entities: int = _value(self, attrs, "max_entities", int)
        if _value(self, attrs, "min_entities", int) > max_entities:
            raise serializers.ValidationError(
                "Mindestzahl an Entitaeten darf nicht groesser als die Hoechstzahl sein."
            )
        if self.instance is not None:
            required: int = sum(profile.min_count for profile in self.instance.profiles.all())
            if required > max_entities:
                raise serializers.ValidationError(
                    {"max_entities": f"Die Profile verlangen zusammen mindestens "
                                     f"{required} Entitaeten."}
                )
        return attrs
