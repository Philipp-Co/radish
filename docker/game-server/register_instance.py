#!/usr/bin/env python3
"""
Meldet diese Game-Instanz beim Backend an- bzw. wieder ab (siehe
radish/backend/api/server_views.py: POST/DELETE api/servers/).

Aufgerufen von game-instance-entrypoint.sh, einmal beim Start ("register")
und einmal beim Beenden ("deregister") -- nicht von Django selbst, das
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
    ip_address = _env("RADISH_GAME_SERVER_IP")
    port = _env("RADISH_GAME_SERVER_PORT")
    backend_url = _env("RADISH_BACKEND_URL")

    token = _fetch_access_token()
    response = requests.post(
        f"{backend_url}/api/servers/",
        json={"name": name, "ip_address": ip_address, "port": int(port)},
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


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("register", "deregister"):
        print(f"Aufruf: {sys.argv[0]} register|deregister", file=sys.stderr)
        sys.exit(2)

    if sys.argv[1] == "register":
        register()
    else:
        deregister()


if __name__ == "__main__":
    main()
