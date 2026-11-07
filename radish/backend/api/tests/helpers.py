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

from django.conf import settings
from django.contrib.auth import get_user_model

from api.models import (
    Species,
    TerrainType,
)


try:
    import jsonschema
except ImportError:  # wie die ctest-Schema-Tests: ohne Werkzeug entfaellt nur der Test
    jsonschema = None

User = get_user_model()

# Das Schema der Spieldaten beim Start (siehe instance_client.game_start_document).
SPIELSTART_SCHEMA = settings.BASE_DIR.parent / "game" / "schema" / "spielstart.schema.json"

# Reicht als Ersatz fuer ein echtes, validiertes Keycloak-Token in Tests --
# HasPlayerRole liest nur request.auth.get("realm_access", {})["roles"]
# (siehe interfaces/auth/roles/realm_roles._realm_roles), ein einfaches dict genuegt dafuer.
PLAYER_TOKEN = {"realm_access": {"roles": ["Player"]}}

# Analog zu PLAYER_TOKEN, aber fuer HasAdminRole (interfaces/admin/roles/has_admin_role.py) --
# steht fuer ein Token eines Nutzers mit der Realm-Rolle "Radish-Admin"
# (siehe docker/keycloak/realm-radish.json, User "radish-admin").
ADMIN_TOKEN = {"realm_access": {"roles": ["Radish-Admin"]}}
SERVER_TOKEN = {"realm_access": {"roles": ["Technischer-User-Server"]}}

# Fester Katalog fuer die Tests: die Spezies "Menschen" und je TerrainCode
# eine Zeile. Die Migrationen enthalten nur das Schema, und der Dump
# api/fixtures/seed.json aendert sich mit jeder Pflege der Daten -- die Tests
# sollen davon nicht abhaengen. Die Werte sind die, mit denen das Gelaende
# frueher angelegt wurde.
TEST_TERRAIN = [
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


def create_test_catalog():
    Species.objects.create(name="Menschen")
    for code, movement_modifier, cover, impassable, blocks_sight in TEST_TERRAIN:
        TerrainType.objects.create(
            code=code,
            movement_modifier=movement_modifier,
            cover=cover,
            impassable=impassable,
            blocks_sight=blocks_sight,
        )
