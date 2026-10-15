#!/usr/bin/env python3
"""
Meldet diese Game-Instanz beim Backend an- bzw. wieder ab (siehe
radish/backend/api/server_views.py: POST/DELETE api/servers/) und meldet
das Ende eines Spiels (POST api/servers/<name>/game-finished/).

    register_instance.py register
    register_instance.py deregister
    register_instance.py game-finished <winner> <loser>
    register_instance.py game-finished draw

Anmelden und Abmelden ruft game-instance-entrypoint.sh, einmal beim Start
("register") und einmal beim Beenden ("deregister"). Das Spielende ruft
radish_server, sobald ein Spiel vorbei ist (RADISH_GAME_FINISHED_COMMAND,
siehe radish/game-server-core/src/interface/game_report.h). Nicht von Django selbst, das
bleibt weiterhin eine leere Huelle (siehe config/settings.py). Kein Modul
unter instances/, weil dieses Skript nichts mit der Django-Anwendung
dieses Containers zu tun hat: es meldet sich beim *anderen* Backend
(radish/backend/) an, nicht bei sich selbst.

Authentifiziert sich als Service-Account per Client-Credentials-Grant gegen
Keycloak (Client "radish-game-server", siehe
docker/keycloak/realm-radish.json) -- kein Nutzer-Login, sondern ein
technischer Account mit der Realm-Rolle "Technischer-User-Server". Ein
frisches Token pro Aufruf, keine Wiederverwendung: Start und Ende dieses
Skripts koennen beliebig weit auseinander liegen, ein einmal geholtes Token
waere laengst abgelaufen.
"""

import os
import sys

import requests


def _raise_for_status_with_body(response):
    """
    Wie response.raise_for_status(), aber mit dem Antwortkoerper im Log --
    ohne das steht bei einem 401/403/400 nur Status und URL da, nicht wieso
    (z.B. DRFs {"detail": "..."} bei einem abgelehnten Token). Das laeuft in
    docker logs/supervisord und ist der einzige Ort, an dem sich das noch
    nachvollziehen laesst.
    """
    if not response.ok:
        print(
            f"{response.status_code} von {response.url}: {response.text}",
            file=sys.stderr,
        )
    response.raise_for_status()


def _env(name):
    value = os.environ.get(name)
    if not value:
        print(f"{name} ist nicht gesetzt.", file=sys.stderr)
        sys.exit(1)
    return value


def _fetch_access_token():
    token_url = _env("RADISH_KEYCLOAK_TOKEN_URL")
    client_id = _env("RADISH_KEYCLOAK_CLIENT_ID")
    client_secret = _env("RADISH_KEYCLOAK_CLIENT_SECRET")

    response = requests.post(
        token_url,
        data={
            "grant_type": "client_credentials",
            "client_id": client_id,
            "client_secret": client_secret,
        },
        timeout=10,
    )
    _raise_for_status_with_body(response)
    return response.json()["access_token"]


def register():
    """
    Meldet die Instanz an. Ein Fehlschlag hier ist absichtlich fatal (siehe
    game-instance-entrypoint.sh, kein "|| true" beim Aufruf): eine Instanz,
    die niemand finden kann, soll nicht still weiterlaufen, sondern
    supervisord soll sie als abgestuerzt behandeln und neu versuchen --
    ausser bei 400 (Name schon vergeben), siehe unten.
    """
    name = _env("RADISH_GAME_SERVER_NAME")
    # Muss keine IP sein -- radish/backend/api/models.py, GameServer.address
    # ist ein CharField und nimmt z.B. auch einen Docker-Compose-
    # Servicenamen wie "game-server" an (siehe docker-compose.yaml).
    address = _env("RADISH_GAME_SERVER_ADDRESS")
    port = _env("RADISH_GAME_SERVER_PORT")
    backend_url = _env("RADISH_BACKEND_URL")
    # HTTP-Adresse der Instanz-API dieses Containers (radish/game-server/
    # instances/views.py), ueber die das Backend Spiele startet/abbricht --
    # siehe GameServer.control_url in radish/backend/api/models.py.
    control_url = _env("RADISH_GAME_SERVER_CONTROL_URL")
    # Name der Instanz in diesem Container (Zucchini-Name, Pfadteil der
    # Instanz-API) -- anders als "name" nur hier eindeutig, siehe
    # GameServer.instance_name. Name, Port und dieser Name kommen je Instanz
    # aus game-instance-entrypoint.sh, nicht aus docker-compose.yaml.
    instance_name = _env("RADISH_GAME_INSTANCE_NAME")

    token = _fetch_access_token()
    response = requests.post(
        f"{backend_url}/api/servers/",
        json={
            "name": name,
            "address": address,
            "port": int(port),
            "control_url": control_url,
            "instance_name": instance_name,
        },
        headers={"Authorization": f"Bearer {token}"},
        timeout=10,
    )
    if response.status_code == 400:
        # Name schon vergeben -- am wahrscheinlichsten, weil ein frueherer
        # Deregister-Aufruf nie ankam (Absturz, Netzwerkfehler) und der
        # alte Eintrag dieser selben Instanz noch dasteht. Kein Grund, die
        # Instanz deswegen nicht hochzufahren: sie ist ja da, nur die
        # Meldung bleibt der alte Eintrag.
        print(f"Registrierung abgelehnt (400): {response.text}", file=sys.stderr)
        return
    _raise_for_status_with_body(response)
    print(f"Instanz '{name}' registriert: {response.json()}")


def deregister():
    """
    Meldet die Instanz ab. Fehlschlaege sind hier bewusst NICHT fatal (siehe
    "|| true" beim Aufruf in game-instance-entrypoint.sh): das laeuft beim
    Herunterfahren, und ein nicht erreichbares Backend soll das Beenden
    dieser Instanz nicht verhindern.
    """
    name = _env("RADISH_GAME_SERVER_NAME")
    backend_url = _env("RADISH_BACKEND_URL")

    token = _fetch_access_token()
    response = requests.delete(
        f"{backend_url}/api/servers/{name}/",
        headers={"Authorization": f"Bearer {token}"},
        timeout=10,
    )
    if response.status_code == 404:
        print(f"Instanz '{name}' war nicht (mehr) registriert.", file=sys.stderr)
        return
    _raise_for_status_with_body(response)
    print(f"Instanz '{name}' deregistriert.")


def game_finished(winner, loser):
    """
    Meldet, dass das Spiel auf dieser Instanz vorbei ist: "winner" hat
    gewonnen, "loser" verloren, je ueber die Spieler-Id (die Kennung aus der
    Spielstart-Datei, im Backend Player.identifier). Bei einem Unentschieden
    beide None.

    Danach raeumt das Backend auf wie beim Verlassen: das Spiel verschwindet,
    der Server ist wieder frei, und es bricht das Spiel auf dieser Instanz ab
    (game/cancel/ an die Instanz-API).

    409 heisst, auf dieser Instanz laeuft im Backend kein gestartetes Spiel
    (mehr) -- etwa weil das Ende schon gemeldet oder das Spiel verlassen
    wurde. Das ist kein Fehler: das Spiel ist so oder so vorbei. Jeder andere
    Fehlschlag endet mit einem Exit-Code ungleich 0, damit der Aufrufer es
    erneut versuchen kann.
    """
    name = _env("RADISH_GAME_SERVER_NAME")
    backend_url = _env("RADISH_BACKEND_URL")

    token = _fetch_access_token()
    response = requests.post(
        f"{backend_url}/api/servers/{name}/game-finished/",
        json={"winner": winner, "loser": loser, "game-instance": name},
        headers={"Authorization": f"Bearer {token}"},
        timeout=10,
    )
    if response.status_code == 409:
        print(f"Kein laufendes Spiel auf '{name}' (409): {response.text}", file=sys.stderr)
        return
    _raise_for_status_with_body(response)
    if winner is None:
        print(f"Spielende auf '{name}' gemeldet: unentschieden.")
    else:
        print(f"Spielende auf '{name}' gemeldet: {winner} gewinnt gegen {loser}.")


def _usage():
    print(
        f"Aufruf: {sys.argv[0]} register | deregister | "
        "game-finished <winner> <loser> | game-finished draw",
        file=sys.stderr,
    )
    sys.exit(2)


def main():
    arguments = sys.argv[1:]

    if arguments == ["register"]:
        register()
    elif arguments == ["deregister"]:
        deregister()
    elif arguments == ["game-finished", "draw"]:
        game_finished(None, None)
    elif len(arguments) == 3 and arguments[0] == "game-finished":
        game_finished(arguments[1], arguments[2])
    else:
        _usage()


if __name__ == "__main__":
    main()
