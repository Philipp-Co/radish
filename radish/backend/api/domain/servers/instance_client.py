"""
Aufrufe des Backends an die Instanz-API eines GameServer (siehe
radish/game-server/instances/views.py): ein Spiel starten und abbrechen.

Was beim Start uebergeben wird, beschreibt radish/game/schema/
spielstart.schema.json (siehe game_start_document): die Spieler mit ihren
Armeen und das Gelaende (TerrainType).

Der Start ist synchron: die Instanz setzt das Spiel noch im Aufruf auf
(eine Datei schreiben, den C-Prozess benachrichtigen) und antwortet erst
danach -- 200 heisst, das Spiel ist bereit. Noch ohne Auth, wie die
Instanz-API selbst.
"""

import logging
from typing import TypedDict

import requests

from ...models import Army, Game, GameServer, Player, TerrainCode, TerrainType
from ..army_queries import armies_with_costs

logger = logging.getLogger(__name__)

# Das Aufsetzen ist kurz, und der Aufruf blockiert die Anfrage des Hosts.
TIMEOUT_SECONDS = 5


# Das Spielstart-Dokument nach radish/game/schema/spielstart.schema.json;
# die Klassen entsprechen den $defs dort (Schluesselnamen wie im Schema).


class WeaponDocument(TypedDict):
    """$defs/waffe"""

    name: str
    klasse: str
    schuesse: int
    staerke: int
    min_reichweite: int
    max_reichweite: int
    durchschlag: int


class EquipmentDocument(TypedDict):
    """$defs/ausruestung"""

    name: str


class EntityDocument(TypedDict):
    """$defs/entitaet"""

    profil: str
    leben: int
    ruestung: int
    widerstand: int
    staerke: int
    genauigkeit: int
    waffen: list[WeaponDocument]
    ausruestung: list[EquipmentDocument]


class UnitDocument(TypedDict):
    """$defs/einheit"""

    typ: str
    bewegung: int
    sichtweite: int
    transportkapazitaet: int
    kann_ziele_einnehmen: bool
    entitaeten: list[EntityDocument]


class ArmyDocument(TypedDict):
    """$defs/armee"""

    name: str
    spezies: str
    einheiten: list[UnitDocument]


class PlayerDocument(TypedDict):
    """$defs/spieler"""

    name: str
    kennung: str
    armee: ArmyDocument


class TerrainDocument(TypedDict):
    """$defs/terrain"""

    typ: str
    bewegung: int
    deckung: int
    unbegehbar: bool
    sichtblockierend: bool


class GameStartDocument(TypedDict):
    """Das ganze Dokument (Wurzel des Schemas)."""

    spieldaten: list[PlayerDocument]
    gelaende: list[TerrainDocument]


class GameStartRequest(GameStartDocument):
    """Was die Instanz beim Start bekommt: das Dokument plus die Zucchini-Codes."""

    zugangscodes: list[str]


class InstanceError(Exception):
    """Die Instanz war nicht erreichbar oder hat den Auftrag abgelehnt."""


def _url(server: GameServer, action: str) -> str:
    # instance_name, nicht name: die Instanz-API kennt ihre Instanzen nur unter
    # dem Namen im Container (siehe models.GameServer.instance_name).
    return f"{server.control_url.rstrip('/')}/api/instances/{server.instance_name}/game/{action}/"


def _reachable(server: GameServer) -> bool:
    """Ob die Instanz gemeldet hat, wie ihre Instanz-API zu erreichen ist."""
    return bool(server.control_url and server.instance_name)


def _army_document(army: Army) -> ArmyDocument:
    """
    Eine Armee mit allen Werten aus dem Katalog -- die Instanz hat keinen
    Zugriff darauf. army muss ueber armies_with_costs() geladen sein, sonst
    entsteht je Entitaet eine eigene Abfrage.
    """
    return {
        "name": army.name,
        "spezies": army.species.name,
        "einheiten": [
            {
                "typ": unit.unit_type.name,
                "bewegung": unit.unit_type.movement,
                "sichtweite": unit.unit_type.sight_range,
                "transportkapazitaet": unit.unit_type.transport_capacity,
                "kann_ziele_einnehmen": unit.unit_type.can_capture_objectives,
                "entitaeten": [
                    {
                        "profil": entity.profile.name,
                        "leben": entity.profile.health,
                        "ruestung": entity.profile.armor,
                        "widerstand": entity.profile.toughness,
                        "staerke": entity.profile.strength,
                        "genauigkeit": entity.profile.accuracy,
                        "waffen": [
                            {
                                "name": slot.weapon.name,
                                "klasse": slot.weapon.weapon_class,
                                "schuesse": slot.weapon.shots,
                                "staerke": slot.weapon.strength,
                                "min_reichweite": slot.weapon.min_range,
                                "max_reichweite": slot.weapon.max_range,
                                "durchschlag": slot.weapon.armor_penetration,
                            }
                            for slot in entity.weapons.all()
                        ],
                        "ausruestung": [
                            {"name": slot.equipment.name} for slot in entity.equipment.all()
                        ],
                    }
                    for entity in unit.entities.all()
                ],
            }
            for unit in army.units.all()
        ],
    }


def _player_document(player: Player, army: Army) -> PlayerDocument:
    return {
        # Ohne bekannten Keycloak-Namen (Player von vor dessen Einfuehrung,
        # die seitdem keinen Aufruf mehr gemacht haben) ersatzweise die
        # Kennung -- das Schema verlangt einen nicht-leeren Namen.
        "name": player.name or player.identifier,
        "kennung": player.identifier,
        "armee": _army_document(army),
    }


def _terrain_document() -> list[TerrainDocument]:
    """
    Jeder Tile-Typ mit seinen Werten, so wie der Admin sie gepflegt hat. Das
    Spiel verlangt jeden Typ genau einmal; fehlt einer in der Tabelle (der
    Dump api/fixtures/seed.json bringt alle mit), ist das ein Fehler hier
    und kein Spiel, das die Instanz still ablehnt.
    """
    terrain: list[TerrainType] = list(TerrainType.objects.all())
    missing: set[str] = set(TerrainCode.values) - {entry.code for entry in terrain}
    if missing:
        raise InstanceError(f"Gelaende ohne Werte: {', '.join(sorted(missing))}")
    return [
        {
            "typ": entry.code,
            "bewegung": entry.movement_modifier,
            "deckung": entry.cover,
            "unbegehbar": entry.impassable,
            "sichtblockierend": entry.blocks_sight,
        }
        for entry in terrain
    ]


def game_start_document(game: Game) -> GameStartDocument:
    """
    Die Spieldaten fuer den Start nach radish/game/schema/
    spielstart.schema.json: Host und zweiter Spieler mit ihren Armeen, dazu
    das Gelaende.
    """
    # Gestartet wird nur ein Spiel mit zweitem Spieler (siehe GameStartView).
    assert game.second_player is not None
    # Ebenso beide Armeen (GameCreateView/GameJoinView verlangen sie); ohne
    # das waere armies[None] unten ein KeyError.
    assert game.host_army_id is not None and game.second_player_army_id is not None
    armies: dict[int, Army] = armies_with_costs().in_bulk([game.host_army_id, game.second_player_army_id])
    return {
        "spieldaten": [
            _player_document(game.host, armies[game.host_army_id]),
            _player_document(game.second_player, armies[game.second_player_army_id]),
        ],
        "gelaende": _terrain_document(),
    }


def game_start_request(game: Game) -> GameStartRequest:
    """
    Was die Instanz beim Start bekommt: die Spieldaten nach dem Schema und
    daneben die Zucchini-Codes der beiden Spieler, in derselben Reihenfolge
    (access.py). Die Codes stehen bewusst ausserhalb von "spieldaten": die
    Instanz nimmt sie in ihre Whitelist und schreibt sie nicht in die
    Spielstart-Datei -- der Spielserver braucht sie nicht.
    """
    return {
        **game_start_document(game),
        "zugangscodes": [game.host_zucchini_code, game.second_player_zucchini_code],
    }


def start_game(game: Game) -> None:
    """
    Laesst die Instanz des Spiels es aufsetzen. Kehrt erst zurueck, wenn sie
    fertig ist; wirft InstanceError, wenn sie es nicht aufsetzen konnte.

    Die Zucchini-Codes muessen am Spiel schon gesetzt sein (GameStartView).
    """
    server: GameServer = game.server
    if not _reachable(server):
        raise InstanceError(
            f"Instanz '{server.name}' hat keine Steuer-URL oder keinen Instanznamen gemeldet."
        )

    try:
        response: requests.Response = requests.post(
            _url(server, "start"), json=game_start_request(game), timeout=TIMEOUT_SECONDS
        )
    except requests.RequestException as exc:
        raise InstanceError(f"Instanz '{server.name}' nicht erreichbar: {exc}") from exc
    if response.status_code != 200:
        raise InstanceError(
            f"Instanz '{server.name}' lehnt den Start ab ({response.status_code}): {response.text}"
        )


def cancel_game(server: GameServer) -> None:
    """
    Bricht das Spiel auf der Instanz ab. Fehler werden nur geloggt: das
    Spiel ist im Backend dann trotzdem beendet, und eine haengende Instanz
    darf das Verlassen der Spieler nicht verhindern.
    """
    if not _reachable(server):
        return
    try:
        response: requests.Response = requests.post(_url(server, "cancel"), timeout=TIMEOUT_SECONDS)
    except requests.RequestException as exc:
        logger.warning("Abbruch auf Instanz '%s' fehlgeschlagen: %s", server.name, exc)
        return
    if not response.ok:
        logger.warning(
            "Instanz '%s' lehnt den Abbruch ab (%s): %s",
            server.name,
            response.status_code,
            response.text,
        )
