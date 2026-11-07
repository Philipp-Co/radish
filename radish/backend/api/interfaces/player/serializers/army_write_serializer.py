from collections import Counter

from django.db import transaction
from rest_framework import serializers

from ....models import (
    MAX_UNIT_ENTITIES,
    Army,
    Species,
    Unit,
    UnitEntity,
    UnitEntityEquipment,
    UnitEntityWeapon,
    WeaponClass,
)

from .unit_write_serializer import UnitWriteSerializer


# Waffenklassen einer Entitaet: Feld (ohne "_ids") -> (Klasse, Slotfeld am
# Entitaetsprofil, Bezeichnung fuer Meldungen). Alle Waffen, auch schwere
# und super-schwere, traegt eine einzelne Entitaet.
ENTITY_WEAPON_CLASSES = {
    "weapons": (WeaponClass.STANDARD, "weapon_slots", "Waffe"),
    "heavy_weapons": (WeaponClass.HEAVY, "heavy_weapon_slots", "schwere Waffe"),
    "super_heavy_weapons": (
        WeaponClass.SUPER_HEAVY,
        "super_heavy_weapon_slots",
        "super-schwere Waffe",
    ),
}

# Hoechstzahl je Einheit (Feld am Einheitentyp) fuer die Klassen, die eine
# haben -- ueber alle Entitaeten zusammen, zusaetzlich zu deren Slots.
UNIT_WEAPON_LIMITS = {
    "heavy_weapons": "max_heavy_weapons",
    "super_heavy_weapons": "max_super_heavy_weapons",
}


class ArmyWriteSerializer(serializers.Serializer):
    """
    Eingabe fuer das Anlegen (POST) und Speichern (PUT) einer Armee. Der
    Editor schickt immer den vollstaendigen Stand -- beim Speichern werden
    die Einheiten deshalb ersetzt statt einzeln abgeglichen.

    Erwartet im Kontext "player" (Besitzer der Armee) -- fuer die
    Eindeutigkeit des Namens, die nur je Spieler gilt.

    Die Regeln aus dem Katalog prueft validate(): alles in der Armee gehoert
    zu ihrer Spezies, die Entitaeten passen zum Einheitentyp und seinen
    Profilen, und Waffen/Ausruestung passen in die Slots.
    """

    name = serializers.CharField(max_length=255)
    species_id = serializers.PrimaryKeyRelatedField(
        queryset=Species.objects.all(), source="species"
    )
    units = UnitWriteSerializer(many=True, required=False, default=list)

    def validate_name(self, value):
        armies = Army.objects.filter(player=self.context["player"], name=value)
        if self.instance is not None:
            armies = armies.exclude(pk=self.instance.pk)
        if armies.exists():
            raise serializers.ValidationError("Es gibt bereits eine Armee mit diesem Namen.")
        return value

    def validate(self, attrs):
        species = attrs["species"]
        for number, unit in enumerate(attrs["units"], start=1):
            self._validate_unit(species, unit, f"Einheit {number}")
        return attrs

    @staticmethod
    def _validate_unit(species, unit, label):
        unit_type = unit["unit_type"]
        if unit_type.species_id != species.id:
            raise serializers.ValidationError(
                f"{label}: Einheitentyp {unit_type.name} gehoert nicht zur Spezies {species.name}."
            )

        entities = unit["entities"]
        lower = unit_type.min_entities
        upper = min(unit_type.max_entities, MAX_UNIT_ENTITIES)
        if not lower <= len(entities) <= upper:
            raise serializers.ValidationError(
                f"{label}: {unit_type.name} braucht {lower} bis {upper} Entitaeten, "
                f"hat aber {len(entities)}."
            )

        profile_counts = Counter()
        for number, entity in enumerate(entities, start=1):
            entity_label = f"{label}, Entitaet {number}"
            profile = entity["profile"]
            if profile.unit_type_id != unit_type.id:
                raise serializers.ValidationError(
                    f"{entity_label}: Profil {profile.name} gehoert nicht zu {unit_type.name}."
                )
            profile_counts[profile.id] += 1

            for key, (weapon_class, slots_field, class_label) in ENTITY_WEAPON_CLASSES.items():
                weapons = entity[key]
                slots = getattr(profile, slots_field)
                if len(weapons) > slots:
                    raise serializers.ValidationError(
                        f"{entity_label}: {profile.name} hat nur {slots} Slots fuer "
                        f"{class_label}n, bekommt aber {len(weapons)}."
                    )
                for weapon in weapons:
                    if weapon.weapon_class != weapon_class:
                        raise serializers.ValidationError(
                            f"{entity_label}: {weapon.name} ist keine {class_label} "
                            f"({weapon.get_weapon_class_display()})."
                        )
                    if weapon.species_id != species.id:
                        raise serializers.ValidationError(
                            f"{entity_label}: {weapon.name} gehoert nicht zur Spezies "
                            f"{species.name}."
                        )

            equipment = entity["equipment"]
            if len(equipment) > profile.equipment_slots:
                raise serializers.ValidationError(
                    f"{entity_label}: {profile.name} hat nur "
                    f"{profile.equipment_slots} Ausruestungsslots."
                )
            for item in equipment:
                if item.species_id != species.id:
                    raise serializers.ValidationError(
                        f"{entity_label}: {item.name} gehoert nicht zur Spezies {species.name}."
                    )

        for key, limit_field in UNIT_WEAPON_LIMITS.items():
            limit = getattr(unit_type, limit_field)
            if limit is None:
                continue
            count = sum(len(entity[key]) for entity in entities)
            if count > limit:
                class_label = ENTITY_WEAPON_CLASSES[key][2]
                raise serializers.ValidationError(
                    f"{label}: {unit_type.name} darf hoechstens {limit} {class_label}n "
                    f"fuehren, hat aber {count}."
                )

        for profile in unit_type.profiles.all():
            count = profile_counts[profile.id]
            if not profile.min_count <= count <= profile.max_count:
                raise serializers.ValidationError(
                    f"{label}: {unit_type.name} braucht {profile.min_count} bis "
                    f"{profile.max_count} Entitaeten mit Profil {profile.name}, hat aber {count}."
                )

    def create(self, validated_data):
        with transaction.atomic():
            army = Army.objects.create(
                player=self.context["player"],
                name=validated_data["name"],
                species=validated_data["species"],
            )
            self._create_units(army, validated_data["units"])
        return army

    def update(self, instance, validated_data):
        with transaction.atomic():
            instance.name = validated_data["name"]
            instance.species = validated_data["species"]
            instance.save(update_fields=["name", "species"])
            instance.units.all().delete()
            self._create_units(instance, validated_data["units"])
        return instance

    @staticmethod
    def _create_units(army, units):
        for unit_position, unit_data in enumerate(units):
            unit = Unit.objects.create(
                army=army, position=unit_position, unit_type=unit_data["unit_type"]
            )
            for entity_position, entity_data in enumerate(unit_data["entities"]):
                entity = UnitEntity.objects.create(
                    unit=unit, position=entity_position, profile=entity_data["profile"]
                )
                UnitEntityWeapon.objects.bulk_create(
                    UnitEntityWeapon(
                        entity=entity, weapon_class=weapon_class, slot=slot, weapon=weapon
                    )
                    for key, (weapon_class, _, _) in ENTITY_WEAPON_CLASSES.items()
                    for slot, weapon in enumerate(entity_data[key])
                )
                UnitEntityEquipment.objects.bulk_create(
                    UnitEntityEquipment(entity=entity, slot=slot, equipment=item)
                    for slot, item in enumerate(entity_data["equipment"])
                )
