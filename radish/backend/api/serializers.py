from collections import Counter

from django.db import transaction
from rest_framework import serializers

from .models import (
    MAX_UNIT_ENTITIES,
    Army,
    EntityProfile,
    Equipment,
    Game,
    GameServer,
    Species,
    Unit,
    UnitEntity,
    UnitEntityEquipment,
    UnitEntityWeapon,
    UnitType,
    Weapon,
    WeaponClass,
)

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


class GameServerSerializer(serializers.ModelSerializer):
    class Meta:
        model = GameServer
        fields = ["id", "name", "address", "port", "is_occupied", "control_url"]


class GameListSerializer(serializers.ModelSerializer):
    """
    Eintrag in der Liste offener Spiele: Name und Spielerzahl.

    host ist Pflicht (zaehlt also immer als ein Spieler), second_player
    ist erst gesetzt, sobald jemand beigetreten ist -- die Spielerzahl ist
    deshalb 1 oder 2.
    """

    player_count = serializers.SerializerMethodField()

    class Meta:
        model = Game
        fields = ["name", "player_count", "points_limit"]

    def get_player_count(self, obj):
        return 2 if obj.second_player_id else 1


class GameCreateSerializer(serializers.Serializer):
    """
    Eingabe fuer das Erstellen eines Spiels: Name und Passwort. Wer der
    Ersteller ist, kommt nicht mehr vom Client (kein player_identifier
    mehr) -- die View ermittelt den Player ueber request.user, also ueber
    das validierte Access-Token. server und host werden ebenfalls von der
    View zugewiesen/aufgeloest, nicht direkt vom Client gesetzt.
    """

    name = serializers.CharField()
    password = serializers.CharField()
    # Mit wie vielen Punkten gespielt wird, und die Armee des Erstellers --
    # die muss dazu passen (siehe client_views._army_for_game).
    points_limit = serializers.IntegerField(min_value=1)
    army_id = serializers.IntegerField()


class GameJoinSerializer(serializers.Serializer):
    """
    Eingabe fuer das Beitreten: welches Spiel (Name) und dessen Passwort.
    Wer beitritt, kommt wie bei GameCreateSerializer nicht mehr vom Client,
    sondern aus request.user (siehe client_views.GameJoinView). army_id: die
    Armee, mit der er antritt -- sie muss zum Punktelimit des Spiels passen.
    """

    name = serializers.CharField()
    password = serializers.CharField()
    army_id = serializers.IntegerField()


class GameDetailSerializer(serializers.ModelSerializer):
    """
    Antwort auf ein erstelltes/beigetretenes Spiel. Ohne password -- die
    muss der Client ohnehin schon kennen, es besteht kein Grund, sie in
    der Antwort zu wiederholen. Bewusst auch ohne den belegten Server:
    Adresse und Port eines GameServer sollen ueber diese Endpunkte nicht
    nach aussen dringen (die UDP-Bruecke zum Server laeuft ueber den
    WebSocket-Consumer, siehe consumers.EchoConsumer -- der Client braucht
    die Serveradresse dafuer nicht).

    host/second_player sind Player-Fremdschluessel -- nach aussen aber
    weiterhin einfach deren Kennung (identifier), keine verschachtelten
    Player-Objekte, da Player ohnehin nichts weiter als die Kennung traegt.
    """

    host_identifier = serializers.SlugRelatedField(
        source="host", slug_field="identifier", read_only=True
    )
    second_player_identifier = serializers.SlugRelatedField(
        source="second_player", slug_field="identifier", read_only=True
    )

    host_army_name = serializers.CharField(source="host_army.name", read_only=True)
    second_player_army_name = serializers.CharField(
        source="second_player_army.name", read_only=True
    )

    # Ob der anfragende Spieler (context["player"]) Host ist -- nur der darf
    # das Spiel aus der Lobby heraus starten (siehe client_views
    # .GameStartView). Ohne Player im Kontext False.
    is_host = serializers.SerializerMethodField()

    class Meta:
        model = Game
        fields = [
            "id",
            "name",
            "host_identifier",
            "second_player_identifier",
            "points_limit",
            "host_army_name",
            "second_player_army_name",
            "status",
            "is_host",
        ]

    def get_is_host(self, obj):
        player = self.context.get("player")
        return player is not None and obj.host_id == player.id



# ---------------------------------------------------------------------------
# Katalog (nur lesend -- Schreiben kommt mit den Admin-Schnittstellen)
# ---------------------------------------------------------------------------


class EntityProfileSerializer(serializers.ModelSerializer):
    class Meta:
        model = EntityProfile
        fields = [
            "id",
            "name",
            "health",
            "armor",
            "strength",
            "accuracy",
            "equipment_slots",
            "weapon_slots",
            "heavy_weapon_slots",
            "super_heavy_weapon_slots",
            "cost",
            "min_count",
            "max_count",
        ]


class UnitTypeSerializer(serializers.ModelSerializer):
    profiles = EntityProfileSerializer(many=True, read_only=True)

    class Meta:
        model = UnitType
        fields = [
            "id",
            "name",
            "min_entities",
            "max_entities",
            "max_heavy_weapons",
            "max_super_heavy_weapons",
            "transport_capacity",
            "can_capture_objectives",
            "movement",
            "profiles",
        ]


class WeaponSerializer(serializers.ModelSerializer):
    class Meta:
        model = Weapon
        fields = [
            "id",
            "name",
            "weapon_class",
            "shots",
            "strength",
            "min_range",
            "max_range",
            "armor_penetration",
            "cost",
        ]


class EquipmentSerializer(serializers.ModelSerializer):
    class Meta:
        model = Equipment
        fields = ["id", "name", "cost"]


class SpeciesCatalogSerializer(serializers.ModelSerializer):
    """
    Eine Spezies mit allem, was eine Armee von ihr enthalten kann -- genau
    das, was der Armee-Editor im Frontend zur Auswahl braucht.
    """

    unit_types = UnitTypeSerializer(many=True, read_only=True)
    weapons = WeaponSerializer(many=True, read_only=True)
    equipment = EquipmentSerializer(many=True, read_only=True)

    class Meta:
        model = Species
        fields = ["id", "name", "unit_types", "weapons", "equipment"]


# ---------------------------------------------------------------------------
# Armeen
#
# Nach aussen verweist eine Armee nur per id auf den Katalog (species_id,
# unit_type_id, profile_id, weapon_ids, ...) -- die Werte dazu holt sich der
# Client einmal ueber den Katalog-Endpunkt, statt sie in jeder Armee
# mitzuschicken. weapon_ids/equipment_ids sind nach Slot geordnet.
#
# total_cost auf Entitaet, Einheit und Armee rechnen die Modelle aus dem
# Katalog aus (siehe models.Army.total_cost) -- nur lesend, der Client
# schickt keine Kosten mit.
# ---------------------------------------------------------------------------


class ArmyListSerializer(serializers.ModelSerializer):
    species_name = serializers.CharField(source="species.name", read_only=True)
    unit_count = serializers.IntegerField(read_only=True)
    total_cost = serializers.IntegerField(read_only=True)

    class Meta:
        model = Army
        fields = ["id", "name", "species_id", "species_name", "unit_count", "total_cost"]


class UnitEntityReadSerializer(serializers.ModelSerializer):
    """weapon_ids/heavy_weapon_ids/super_heavy_weapon_ids: Waffen je Slotklasse, nach Slot."""

    weapon_ids = serializers.SerializerMethodField()
    heavy_weapon_ids = serializers.SerializerMethodField()
    super_heavy_weapon_ids = serializers.SerializerMethodField()
    equipment_ids = serializers.SerializerMethodField()
    total_cost = serializers.IntegerField(read_only=True)

    class Meta:
        model = UnitEntity
        fields = [
            "profile_id",
            "weapon_ids",
            "heavy_weapon_ids",
            "super_heavy_weapon_ids",
            "equipment_ids",
            "total_cost",
        ]

    @staticmethod
    def _ids(obj, weapon_class):
        return [slot.weapon_id for slot in obj.weapons.all() if slot.weapon_class == weapon_class]

    def get_weapon_ids(self, obj):
        return self._ids(obj, WeaponClass.STANDARD)

    def get_heavy_weapon_ids(self, obj):
        return self._ids(obj, WeaponClass.HEAVY)

    def get_super_heavy_weapon_ids(self, obj):
        return self._ids(obj, WeaponClass.SUPER_HEAVY)

    def get_equipment_ids(self, obj):
        return [slot.equipment_id for slot in obj.equipment.all()]


class UnitReadSerializer(serializers.ModelSerializer):
    entities = UnitEntityReadSerializer(many=True, read_only=True)
    total_cost = serializers.IntegerField(read_only=True)

    class Meta:
        model = Unit
        fields = ["unit_type_id", "entities", "total_cost"]


class ArmyDetailSerializer(serializers.ModelSerializer):
    species_name = serializers.CharField(source="species.name", read_only=True)
    units = UnitReadSerializer(many=True, read_only=True)
    total_cost = serializers.IntegerField(read_only=True)

    class Meta:
        model = Army
        fields = ["id", "name", "species_id", "species_name", "units", "total_cost"]


class UnitEntityWriteSerializer(serializers.Serializer):
    profile_id = serializers.PrimaryKeyRelatedField(
        queryset=EntityProfile.objects.select_related("unit_type"), source="profile"
    )
    weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=Weapon.objects.all()),
        source="weapons",
        required=False,
        default=list,
    )
    heavy_weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=Weapon.objects.all()),
        source="heavy_weapons",
        required=False,
        default=list,
    )
    super_heavy_weapon_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=Weapon.objects.all()),
        source="super_heavy_weapons",
        required=False,
        default=list,
    )
    equipment_ids = serializers.ListField(
        child=serializers.PrimaryKeyRelatedField(queryset=Equipment.objects.all()),
        source="equipment",
        required=False,
        default=list,
    )


class UnitWriteSerializer(serializers.Serializer):
    unit_type_id = serializers.PrimaryKeyRelatedField(
        queryset=UnitType.objects.all(), source="unit_type"
    )
    entities = UnitEntityWriteSerializer(many=True, allow_empty=False)


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


# ---------------------------------------------------------------------------
# Katalog-Pflege fuer Admins (siehe admin_views.py)
#
# Der Admin darf jeden Eintrag jederzeit aendern, auch wenn Spieler ihn in
# ihren Armeen verwenden -- Aenderungen am Katalog sind Aenderungen an
# Werten, keine strukturellen, deshalb werden gespeicherte Armeen danach
# nicht erneut geprueft. Fest bleibt nur:
# - immutable_fields: die Zuordnung (Spezies bzw. Einheitentyp) steht nach
#   dem Anlegen fest -- sonst gehoerte z.B. eine Waffe ploetzlich einer
#   anderen Spezies, und der Katalog der Spezies waere nicht mehr eindeutig.
# - Loeschen: ein verwendeter Eintrag laesst sich nicht loeschen (PROTECT,
#   siehe admin_views), sonst verschwaenden Einheiten aus Spielerarmeen.
#
# Ob ein Eintrag verwendet wird, liefert die View als Annotation "in_use"
# (siehe admin_views.IN_USE); frisch angelegte Eintraege sind es nie.
# ---------------------------------------------------------------------------


def _value(serializer, attrs, name):
    """
    Wert aus der Eingabe; fehlt er, bei PATCH der bisherige und beim
    Anlegen der Default des Modells (z.B. Weapon.min_range = 0).
    """
    if name in attrs:
        return attrs[name]
    if serializer.instance is not None:
        return getattr(serializer.instance, name)
    return serializer.Meta.model._meta.get_field(name).get_default()


class _CatalogAdminSerializer(serializers.ModelSerializer):
    in_use = serializers.BooleanField(read_only=True, default=False)

    immutable_fields = ()

    def validate(self, attrs):
        if self.instance is None:
            return attrs
        for field in self.immutable_fields:
            if field in attrs and attrs[field] != getattr(self.instance, field):
                raise serializers.ValidationError(
                    {field: "Kann nach dem Anlegen nicht mehr geaendert werden."}
                )
        return attrs


class AdminSpeciesSerializer(serializers.ModelSerializer):
    class Meta:
        model = Species
        fields = ["id", "name"]


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

    def validate(self, attrs):
        attrs = super().validate(attrs)
        unit_type = _value(self, attrs, "unit_type")
        min_count = _value(self, attrs, "min_count")
        max_count = _value(self, attrs, "max_count")

        if min_count > max_count:
            raise serializers.ValidationError(
                "Mindestanzahl darf nicht groesser als die Hoechstanzahl sein."
            )

        others = unit_type.profiles.all()
        if self.instance is not None:
            others = others.exclude(pk=self.instance.pk)
        required = min_count + sum(profile.min_count for profile in others)
        if required > unit_type.max_entities:
            raise serializers.ValidationError(
                f"Die Mindestanzahlen aller Profile ({required}) uebersteigen die "
                f"Hoechstzahl an Entitaeten von {unit_type.name} ({unit_type.max_entities})."
            )
        return attrs


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
            "in_use",
            "profiles",
        ]

    def validate(self, attrs):
        attrs = super().validate(attrs)
        max_entities = _value(self, attrs, "max_entities")
        if _value(self, attrs, "min_entities") > max_entities:
            raise serializers.ValidationError(
                "Mindestzahl an Entitaeten darf nicht groesser als die Hoechstzahl sein."
            )
        if self.instance is not None:
            required = sum(profile.min_count for profile in self.instance.profiles.all())
            if required > max_entities:
                raise serializers.ValidationError(
                    {"max_entities": f"Die Profile verlangen zusammen mindestens "
                                     f"{required} Entitaeten."}
                )
        return attrs


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


class AdminEquipmentSerializer(_CatalogAdminSerializer):
    species_id = serializers.PrimaryKeyRelatedField(
        queryset=Species.objects.all(), source="species"
    )
    species_name = serializers.CharField(source="species.name", read_only=True)

    immutable_fields = ("species",)

    class Meta:
        model = Equipment
        fields = ["id", "species_id", "species_name", "name", "cost", "in_use"]
