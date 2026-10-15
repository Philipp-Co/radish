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

import requests

from .models import TerrainCode, TerrainType, armies_with_costs

logger = logging.getLogger(__name__)

# Das Aufsetzen ist kurz, und der Aufruf blockiert die Anfrage des Hosts.
TIMEOUT_SECONDS = 5


class InstanceError(Exception):
    """Die Instanz war nicht erreichbar oder hat den Auftrag abgelehnt."""


def _url(server, action):
    # instance_name, nicht name: die Instanz-API kennt ihre Instanzen nur unter
    # dem Namen im Container (siehe models.GameServer.instance_name).
    return f"{server.control_url.rstrip('/')}/api/instances/{server.instance_name}/game/{action}/"


def _reachable(server):
    """Ob die Instanz gemeldet hat, wie ihre Instanz-API zu erreichen ist."""
    return bool(server.control_url and server.instance_name)


def _army_document(army):
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


def _player_document(player, army):
    return {
        # Ohne bekannten Keycloak-Namen (Player von vor dessen Einfuehrung,
        # die seitdem keinen Aufruf mehr gemacht haben) ersatzweise die
        # Kennung -- das Schema verlangt einen nicht-leeren Namen.
        "name": player.name or player.identifier,
        "kennung": player.identifier,
        "armee": _army_document(army),
    }


def _terrain_document():
    """
    Jeder Tile-Typ mit seinen Werten, so wie der Admin sie gepflegt hat. Das
    Spiel verlangt jeden Typ genau einmal; fehlt einer in der Tabelle (die
    Migration legt alle an), ist das ein Fehler hier und kein Spiel, das die
    Instanz still ablehnt.
    """
    terrain = list(TerrainType.objects.all())
    missing = set(TerrainCode.values) - {entry.code for entry in terrain}
    if missing:
        raise InstanceError(f"Gelaende ohne Werte: {', '.join(sorted(missing))}")
    return [
        {
            "typ": entry.code,
            "bewegung": entry.movement_modifier,
            "deckung": entry.cover,
            "unbegehbar": entry.impassable,
        }
        for entry in terrain
    ]


def game_start_document(game):
    """
    Die Spieldaten fuer den Start nach radish/game/schema/
    spielstart.schema.json: Host und zweiter Spieler mit ihren Armeen, dazu
    das Gelaende.
    """
    armies = armies_with_costs().in_bulk([game.host_army_id, game.second_player_army_id])
    return {
        "spieldaten": [
            _player_document(game.host, armies[game.host_army_id]),
            _player_document(game.second_player, armies[game.second_player_army_id]),
        ],
        "gelaende": _terrain_document(),
    }


def game_start_request(game):
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


def start_game(game):
    """
    Laesst die Instanz des Spiels es aufsetzen. Kehrt erst zurueck, wenn sie
    fertig ist; wirft InstanceError, wenn sie es nicht aufsetzen konnte.

    Die Zucchini-Codes muessen am Spiel schon gesetzt sein (GameStartView).
    """
    server = game.server
    if not _reachable(server):
        raise InstanceError(
            f"Instanz '{server.name}' hat keine Steuer-URL oder keinen Instanznamen gemeldet."
        )

    try:
        response = requests.post(
            _url(server, "start"), json=game_start_request(game), timeout=TIMEOUT_SECONDS
        )
    except requests.RequestException as exc:
        raise InstanceError(f"Instanz '{server.name}' nicht erreichbar: {exc}") from exc
    if response.status_code != 200:
        raise InstanceError(
            f"Instanz '{server.name}' lehnt den Start ab ({response.status_code}): {response.text}"
        )


def cancel_game(server):
    """
    Bricht das Spiel auf der Instanz ab. Fehler werden nur geloggt: das
    Spiel ist im Backend dann trotzdem beendet, und eine haengende Instanz
    darf das Verlassen der Spieler nicht verhindern.
    """
    if not _reachable(server):
        return
    try:
        response = requests.post(_url(server, "cancel"), timeout=TIMEOUT_SECONDS)
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
