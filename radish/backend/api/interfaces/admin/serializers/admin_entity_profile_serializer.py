# Admin- und Spieler-API geben bewusst dieselben Felder aus, duerfen sich
# aber unabhaengig aendern.
# pylint: disable=duplicate-code

from django.db.models import QuerySet
from rest_framework import serializers

from api.interfaces.admin.serializers.catalog_admin_serializer import _CatalogAdminSerializer, _current_or_new, _value
from api.models import EntityProfile, UnitType


class AdminEntityProfileSerializer(_CatalogAdminSerializer):
    unit_type_id = serializers.PrimaryKeyRelatedField(
        queryset=UnitType.objects.all(), source="unit_type"
    )
    # Nur zur Anzeige (Kontext in der Suche) -- geschrieben wird unit_type_id.
    unit_type_name = serializers.CharField(source="unit_type.name", read_only=True)
    species_id = serializers.IntegerField(source="unit_type.species_id", read_only=True)
    species_name = serializers.CharField(source="unit_type.species.name", read_only=True)

    immutable_fields = ("unit_type",)

    class Meta:
        model = EntityProfile
        fields = [
            "id",
            "unit_type_id",
            "unit_type_name",
            "species_id",
            "species_name",
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
            "in_use",
        ]

    # Die Stubs typisieren attrs als Any; es sind die validierten Daten, ein
    # dict von Feldname auf Wert.
    def validate(self, attrs: dict[str, object]) -> dict[str, object]:
        attrs = super().validate(attrs)
        # Bisheriger Stand bzw. ein neues Profil mit den Defaults des Modells.
        entry: EntityProfile = _current_or_new(self.instance, EntityProfile)
        unit_type: UnitType = _value(attrs, "unit_type", lambda: entry.unit_type, UnitType)
        min_count: int = _value(attrs, "min_count", lambda: entry.min_count, int)
        max_count: int = _value(attrs, "max_count", lambda: entry.max_count, int)

        if min_count > max_count:
            raise serializers.ValidationError(
                "Mindestanzahl darf nicht groesser als die Hoechstanzahl sein."
            )

        others: QuerySet[EntityProfile] = unit_type.profiles.all()
        if self.instance is not None:
            others = others.exclude(pk=self.instance.pk)
        required: int = min_count + sum(profile.min_count for profile in others)
        if required > unit_type.max_entities:
            raise serializers.ValidationError(
                f"Die Mindestanzahlen aller Profile ({required}) uebersteigen die "
                f"Hoechstzahl an Entitaeten von {unit_type.name} ({unit_type.max_entities})."
            )
        return attrs
