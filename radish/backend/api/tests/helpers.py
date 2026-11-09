"""
Gemeinsame Helfer fuer die Tests unter api/tests/ (Tokens, Testkatalog,
Spielstart-Schema); die Basisklassen stehen in army_test_case.py und
game_test_case.py.

Authentifizierung laeuft ueber echte Keycloak-Tokens (siehe
authentication.KeycloakJWTAuthentication), fuer Tests deshalb per
force_authenticate umgangen statt ein echtes JWT zu bauen. HasPlayerRole
(interfaces/player/roles/has_player_role.py) liest die Realm-Rolle aus request.auth -- deshalb wird bei
force_authenticate zusaetzlich ein token-Objekt mitgegeben, das sich wie
ein validiertes JWT mit "realm_access.roles" verhaelt.
"""

from collections.abc import Iterable, Mapping
from typing import TYPE_CHECKING, NotRequired, Required, TypedDict, TypeVar

from django.conf import settings
from django.contrib.auth import get_user_model
from django.contrib.auth.base_user import AbstractBaseUser
from rest_framework.test import APIClient

from api.models import Species, TerrainType

User = get_user_model()

# Was der DRF-Testclient zurueckgibt (Response mit .data). Den Typ gibt es nur
# in den Stubs, zur Laufzeit genuegt die Basisklasse Response (Annotationen
# lokaler Variablen werden nie ausgewertet).
if TYPE_CHECKING:
    from rest_framework.response import _MonkeyPatchedResponse as TestResponse
else:
    from rest_framework.response import Response

    TestResponse = Response

# Das Schema der Spieldaten beim Start (siehe instance_client.game_start_document).
SPIELSTART_SCHEMA = settings.BASE_DIR.parent / "game" / "schema" / "spielstart.schema.json"



class RealmAccess(TypedDict):
    roles: list[str]


class TestToken(TypedDict):
    """
    Die Claims eines validierten Keycloak-Tokens, soweit das Backend sie
    liest: die Realm-Rollen und optional den Keycloak-Benutzernamen.
    """

    realm_access: RealmAccess
    preferred_username: NotRequired[str]


def authenticate(client: APIClient, user: AbstractBaseUser, token: TestToken) -> None:
    """force_authenticate mit einem Claims-dict als request.auth (siehe oben)."""
    # ignore[arg-type]: Die Stubs typisieren token als rest_framework.authtoken
    # Token; request.auth ist in diesem Projekt aber das Claims-dict des
    # Keycloak-Tokens, und genau das reichen die Tests hier durch.
    client.force_authenticate(user=user, token=token)  # type: ignore[arg-type]


# Reicht als Ersatz fuer ein echtes, validiertes Keycloak-Token in Tests --
# HasPlayerRole liest nur request.auth.get("realm_access", {})["roles"]
# (siehe interfaces/auth/roles/realm_roles._realm_roles), ein einfaches dict genuegt dafuer.
PLAYER_TOKEN: TestToken = {"realm_access": {"roles": ["Player"]}}

# Analog zu PLAYER_TOKEN, aber fuer HasAdminRole (interfaces/admin/roles/has_admin_role.py) --
# steht fuer ein Token eines Nutzers mit der Realm-Rolle "Radish-Admin"
# (siehe docker/keycloak/realm-radish.json, User "radish-admin").
ADMIN_TOKEN: TestToken = {"realm_access": {"roles": ["Radish-Admin"]}}
SERVER_TOKEN: TestToken = {"realm_access": {"roles": ["Technischer-User-Server"]}}

# Fester Katalog fuer die Tests: die Spezies "Menschen" und je TerrainCode
# eine Zeile. Die Migrationen enthalten nur das Schema, und der Dump
# api/fixtures/seed.json aendert sich mit jeder Pflege der Daten -- die Tests
# sollen davon nicht abhaengen. Die Werte sind die, mit denen das Gelaende
# frueher angelegt wurde.
TEST_TERRAIN: list[tuple[str, int, int, bool, bool]] = [
    # code, movement_modifier, cover, impassable, blocks_sight
    ("void", 0, 0, True, False),
    ("ground", 0, 1, False, False),
    ("water_shallow", -1, 0, False, False),
    ("mud", -1, 0, False, False),
    ("meadow", 0, 0, False, False),
    ("forest", -1, 2, False, True),
    ("water_deep", 0, 0, True, False),
    ("city", 0, 2, False, True),
    ("road", 1, 0, False, False),
    ("fortification", -1, 3, False, True),
    ("dragons_teeth", 0, 0, True, False),
    ("high_grass", 0, 1, False, True),
]


def create_test_catalog() -> None:
    Species.objects.create(name="Menschen")
    code: str
    movement_modifier: int
    cover: int
    impassable: bool
    blocks_sight: bool
    for code, movement_modifier, cover, impassable, blocks_sight in TEST_TERRAIN:
        TerrainType.objects.create(
            code=code,
            movement_modifier=movement_modifier,
            cover=cover,
            impassable=impassable,
            blocks_sight=blocks_sight,
        )


# Ein Eintrag einer API-Antwort (ein TypedDict wie TerrainTypeData).
EntryT = TypeVar("EntryT", bound=Mapping[str, object])


def find_by(items: Iterable[EntryT], **fields: object) -> EntryT:
    """
    Der eine Eintrag aus items, dessen Felder die angegebenen Werte haben.
    Laesst den Test fehlschlagen, wenn keiner oder mehrere passen.
    """
    matches: list[EntryT] = [
        item for item in items if all(item[key] == value for key, value in fields.items())
    ]
    if len(matches) != 1:
        raise AssertionError(f"{len(matches)} Eintraege mit {fields}, erwartet genau einer")
    return matches[0]


# Payloads, die die Tests an die API schicken. Die Tests schicken auch
# absichtlich unvollstaendige Payloads, deshalb sind manche Schluessel optional.


class EntityPayload(TypedDict, total=False):
    profile_id: Required[int]
    weapon_ids: list[int]
    heavy_weapon_ids: list[int]
    super_heavy_weapon_ids: list[int]
    equipment_ids: list[int]


class UnitPayload(TypedDict):
    unit_type_id: int
    entities: list[EntityPayload]


class UnitPayloadOverrides(TypedDict, total=False):
    """Einzelne Schluessel von UnitPayload, die ein Test ersetzt."""

    unit_type_id: int
    entities: list[EntityPayload]


class ArmyPayload(TypedDict):
    name: str
    species_id: int
    units: list[UnitPayload]


class WeaponPayload(TypedDict, total=False):
    """Admin-Katalog: Waffe. total=False, weil Tests Schluessel weglassen oder einzeln ersetzen."""

    species_id: int
    name: str
    weapon_class: str
    shots: int
    strength: int
    min_range: int
    max_range: int
    armor_penetration: int
    cost: int


class ProfilePayload(TypedDict, total=False):
    """Admin-Katalog: Entitaetenprofil. total=False wie bei WeaponPayload."""

    unit_type_id: int
    name: str
    health: int
    armor: int
    toughness: int
    strength: int
    accuracy: int
    equipment_slots: int
    weapon_slots: int
    heavy_weapon_slots: int
    super_heavy_weapon_slots: int
    cost: int
    min_count: int
    max_count: int


class ServerRegistrationPayload(TypedDict, total=False):
    """Anmeldung einer Instanz. total=False, weil Tests einzelne Schluessel ersetzen."""

    name: str
    address: str
    port: int
    control_url: str
    instance_name: str


# Meldung der Instanz ueber ein beendetes Spiel; "game-instance" ist kein
# Bezeichner, deshalb die funktionale Schreibweise. Bei Unentschieden sind
# winner/loser None; die Tests lassen Schluessel auch absichtlich weg.
GameFinishedBody = TypedDict(
    "GameFinishedBody",
    {"winner": str | None, "loser": str | None, "game-instance": str},
    total=False,
)


# Antworten der API, soweit die Tests in sie hineinlesen (nach den
# Serializern unter interfaces/player/serializers und interfaces/admin/serializers).


class ProfileData(TypedDict):
    """EntityProfileSerializer"""

    id: int
    name: str
    health: int
    armor: int
    toughness: int
    strength: int
    accuracy: int
    equipment_slots: int
    weapon_slots: int
    heavy_weapon_slots: int
    super_heavy_weapon_slots: int
    cost: int
    min_count: int
    max_count: int


class UnitTypeData(TypedDict):
    """UnitTypeSerializer"""

    id: int
    name: str
    min_entities: int
    max_entities: int
    max_heavy_weapons: int | None
    max_super_heavy_weapons: int | None
    transport_capacity: int
    can_capture_objectives: bool
    movement: int
    sight_range: int
    profiles: list[ProfileData]


class WeaponData(TypedDict):
    """WeaponSerializer"""

    id: int
    name: str
    weapon_class: str
    shots: int
    strength: int
    min_range: int
    max_range: int
    armor_penetration: int
    cost: int


class EquipmentData(TypedDict):
    """EquipmentSerializer"""

    id: int
    name: str
    cost: int


class SpeciesCatalogData(TypedDict):
    """SpeciesCatalogSerializer"""

    id: int
    name: str
    unit_types: list[UnitTypeData]
    weapons: list[WeaponData]
    equipment: list[EquipmentData]


class EntityData(TypedDict):
    """UnitEntityReadSerializer"""

    profile_id: int
    weapon_ids: list[int]
    heavy_weapon_ids: list[int]
    super_heavy_weapon_ids: list[int]
    equipment_ids: list[int]
    total_cost: int


class UnitData(TypedDict):
    """UnitReadSerializer"""

    unit_type_id: int
    entities: list[EntityData]
    total_cost: int


class ArmyListData(TypedDict):
    """ArmyListSerializer"""

    id: int
    name: str
    species_id: int
    species_name: str
    unit_count: int
    total_cost: int


class TerrainTypeData(TypedDict):
    """AdminTerrainTypeSerializer"""

    code: str
    label: str
    movement_modifier: int
    cover: int
    impassable: bool
    blocks_sight: bool
