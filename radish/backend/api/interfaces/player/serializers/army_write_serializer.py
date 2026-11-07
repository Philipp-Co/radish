from collections import Counter
from typing import Any, Literal, TypedDict, cast

from django.db import transaction
from django.db.models import QuerySet
from rest_framework import serializers

from ....models import (
    MAX_UNIT_ENTITIES,
    Army,
    EntityProfile,
    Equipment,
    Species,
    Unit,
    UnitEntity,
    UnitEntityEquipment,
    UnitEntityWeapon,
    UnitType,
    Weapon,
    WeaponClass,
)

from .unit_write_serializer import UnitWriteSerializer


# Die Waffenfelder einer validierten Entitaet (siehe ValidatedEntity) --
# als Literal, damit sie den TypedDict typsicher indizieren.
WeaponKey = Literal["weapons", "heavy_weapons", "super_heavy_weapons"]


class ValidatedEntity(TypedDict):
    """Eine Entitaet nach UnitEntityWriteSerializer: die ids sind aufgeloest."""

    profile: EntityProfile
    weapons: list[Weapon]
    heavy_weapons: list[Weapon]
    super_heavy_weapons: list[Weapon]
    equipment: list[Equipment]


class ValidatedUnit(TypedDict):
    """Eine Einheit nach UnitWriteSerializer."""

    unit_type: UnitType
    entities: list[ValidatedEntity]


class ValidatedArmy(TypedDict):
    """Die validierten Daten von ArmyWriteSerializer (Felder ueber "source")."""

    name: str
    species: Species
    units: list[ValidatedUnit]


# Waffenklassen einer Entitaet: Feld (ohne "_ids") -> (Klasse, Slotfeld am
# Entitaetsprofil, Bezeichnung fuer Meldungen). Alle Waffen, auch schwere
# und super-schwere, traegt eine einzelne Entitaet.
ENTITY_WEAPON_CLASSES: dict[WeaponKey, tuple[WeaponClass, str, str]] = {
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
UNIT_WEAPON_LIMITS: dict[WeaponKey, str] = {
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

    def validate_name(self, value: str) -> str:
        armies: QuerySet[Army] = Army.objects.filter(player=self.context["player"], name=value)
        if self.instance is not None:
            armies = armies.exclude(pk=self.instance.pk)
        if armies.exists():
            raise serializers.ValidationError("Es gibt bereits eine Armee mit diesem Namen.")
        return value

    # Any: Signatur von DRF Serializer.validate (Stubs); enger waere eine
    # Liskov-Verletzung.
    def validate(self, attrs: dict[str, Any]) -> dict[str, Any]:
        # Die Feld-Serializer haben attrs zu genau dieser Struktur aufgeloest
        # (PrimaryKeyRelatedField -> Model, verschachtelte Serializer).
        data: ValidatedArmy = cast(ValidatedArmy, attrs)
        species: Species = data["species"]
        number: int
        unit: ValidatedUnit
        for number, unit in enumerate(data["units"], start=1):
            self._validate_unit(species, unit, f"Einheit {number}")
        return attrs

    @staticmethod
    def _validate_unit(species: Species, unit: ValidatedUnit, label: str) -> None:
        """Prueft eine Einheit gegen den Katalog; die Reihenfolge bestimmt die Fehlermeldung."""
        unit_type: UnitType = unit["unit_type"]
        if unit_type.species_id != species.id:
            raise serializers.ValidationError(
                f"{label}: Einheitentyp {unit_type.name} gehoert nicht zur Spezies {species.name}."
            )

        entities: list[ValidatedEntity] = unit["entities"]
        lower: int = unit_type.min_entities
        upper: int = min(unit_type.max_entities, MAX_UNIT_ENTITIES)
        if not lower <= len(entities) <= upper:
            raise serializers.ValidationError(
                f"{label}: {unit_type.name} braucht {lower} bis {upper} Entitaeten, "
                f"hat aber {len(entities)}."
            )

        profile_counts: Counter[int] = Counter()
        number: int
        entity: ValidatedEntity
        for number, entity in enumerate(entities, start=1):
            ArmyWriteSerializer._validate_entity(species, unit_type, entity, f"{label}, Entitaet {number}")
            profile_counts[entity["profile"].id] += 1

        ArmyWriteSerializer._validate_unit_weapon_limits(unit_type, entities, label)
        ArmyWriteSerializer._validate_profile_counts(unit_type, profile_counts, label)

    @staticmethod
    def _validate_entity(species: Species, unit_type: UnitType, entity: ValidatedEntity, label: str) -> None:
        """Profil, Waffen und Ausruestung einer Entitaet."""
        profile: EntityProfile = entity["profile"]
        if profile.unit_type_id != unit_type.id:
            raise serializers.ValidationError(
                f"{label}: Profil {profile.name} gehoert nicht zu {unit_type.name}."
            )
        ArmyWriteSerializer._validate_entity_weapons(species, profile, entity, label)
        ArmyWriteSerializer._validate_entity_equipment(species, profile, entity, label)

    @staticmethod
    def _validate_entity_weapons(
        species: Species, profile: EntityProfile, entity: ValidatedEntity, label: str
    ) -> None:
        """Je Waffenklasse: Slots des Profils, Klasse und Spezies jeder Waffe."""
        key: WeaponKey
        weapon_class: WeaponClass
        slots_field: str
        class_label: str
        weapon: Weapon
        for key, (weapon_class, slots_field, class_label) in ENTITY_WEAPON_CLASSES.items():
            weapons: list[Weapon] = entity[key]
            slots: int = getattr(profile, slots_field)
            if len(weapons) > slots:
                raise serializers.ValidationError(
                    f"{label}: {profile.name} hat nur {slots} Slots fuer "
                    f"{class_label}n, bekommt aber {len(weapons)}."
                )
            for weapon in weapons:
                ArmyWriteSerializer._validate_weapon(species, weapon, weapon_class, class_label, label)

    @staticmethod
    def _validate_weapon(
        species: Species, weapon: Weapon, weapon_class: WeaponClass, class_label: str, label: str
    ) -> None:
        """Erst die Klasse, dann die Spezies einer einzelnen Waffe."""
        if weapon.weapon_class != weapon_class:
            raise serializers.ValidationError(
                f"{label}: {weapon.name} ist keine {class_label} "
                f"({weapon.get_weapon_class_display()})."
            )
        if weapon.species_id != species.id:
            raise serializers.ValidationError(
                f"{label}: {weapon.name} gehoert nicht zur Spezies "
                f"{species.name}."
            )

    @staticmethod
    def _validate_entity_equipment(
        species: Species, profile: EntityProfile, entity: ValidatedEntity, label: str
    ) -> None:
        """Ausruestungsslots des Profils und Spezies jedes Gegenstands."""
        equipment: list[Equipment] = entity["equipment"]
        if len(equipment) > profile.equipment_slots:
            raise serializers.ValidationError(
                f"{label}: {profile.name} hat nur "
                f"{profile.equipment_slots} Ausruestungsslots."
            )
        item: Equipment
        for item in equipment:
            if item.species_id != species.id:
                raise serializers.ValidationError(
                    f"{label}: {item.name} gehoert nicht zur Spezies {species.name}."
                )

    @staticmethod
    def _validate_unit_weapon_limits(unit_type: UnitType, entities: list[ValidatedEntity], label: str) -> None:
        """Hoechstzahl schwerer bzw. super-schwerer Waffen ueber alle Entitaeten der Einheit."""
        key: WeaponKey
        limit_field: str
        for key, limit_field in UNIT_WEAPON_LIMITS.items():
            limit: int | None = getattr(unit_type, limit_field)
            if limit is None:
                continue
            count: int = sum(len(entity[key]) for entity in entities)
            if count > limit:
                class_label: str = ENTITY_WEAPON_CLASSES[key][2]
                raise serializers.ValidationError(
                    f"{label}: {unit_type.name} darf hoechstens {limit} {class_label}n "
                    f"fuehren, hat aber {count}."
                )

    @staticmethod
    def _validate_profile_counts(unit_type: UnitType, profile_counts: Counter[int], label: str) -> None:
        """Wie viele Entitaeten je Profil die Einheit haben darf (min_count/max_count)."""
        profile: EntityProfile
        for profile in unit_type.profiles.all():
            count: int = profile_counts[profile.id]
            if not profile.min_count <= count <= profile.max_count:
                raise serializers.ValidationError(
                    f"{label}: {unit_type.name} braucht {profile.min_count} bis "
                    f"{profile.max_count} Entitaeten mit Profil {profile.name}, hat aber {count}."
                )

    # Any: Signatur von DRF Serializer.create (Stubs); enger waere eine
    # Liskov-Verletzung.
    def create(self, validated_data: dict[str, Any]) -> Army:
        # Durch validate() bzw. die Feld-Serializer garantierte Struktur.
        data: ValidatedArmy = cast(ValidatedArmy, validated_data)
        with transaction.atomic():
            army: Army = Army.objects.create(
                player=self.context["player"],
                name=data["name"],
                species=data["species"],
            )
            self._create_units(army, data["units"])
        return army

    # Any: Signatur von DRF Serializer.update (Stubs); enger waere eine
    # Liskov-Verletzung.
    def update(self, instance: Army, validated_data: dict[str, Any]) -> Army:
        # Durch validate() bzw. die Feld-Serializer garantierte Struktur.
        data: ValidatedArmy = cast(ValidatedArmy, validated_data)
        with transaction.atomic():
            instance.name = data["name"]
            instance.species = data["species"]
            instance.save(update_fields=["name", "species"])
            instance.units.all().delete()
            self._create_units(instance, data["units"])
        return instance

    @staticmethod
    def _create_units(army: Army, units: list[ValidatedUnit]) -> None:
        unit_position: int
        unit_data: ValidatedUnit
        entity_position: int
        entity_data: ValidatedEntity
        for unit_position, unit_data in enumerate(units):
            unit: Unit = Unit.objects.create(
                army=army, position=unit_position, unit_type=unit_data["unit_type"]
            )
            for entity_position, entity_data in enumerate(unit_data["entities"]):
                ArmyWriteSerializer._create_entity(unit, entity_position, entity_data)

    @staticmethod
    def _create_entity(unit: Unit, position: int, entity_data: ValidatedEntity) -> None:
        """Eine Entitaet samt Waffen und Ausruestung (je ein bulk_create)."""
        entity: UnitEntity = UnitEntity.objects.create(
            unit=unit, position=position, profile=entity_data["profile"]
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
