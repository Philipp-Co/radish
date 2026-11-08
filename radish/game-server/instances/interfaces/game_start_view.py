"""
Endpunkt, ueber den das Backend ein Spiel auf einer Instanz startet. Was dabei
passiert, steht in domain/game_files.py.
"""

from collections.abc import Callable
from typing import cast

from rest_framework import status
from rest_framework.request import Request
from rest_framework.response import Response

from ..domain.access_codes import valid_access_codes
from ..domain.game_files import InstanceNotRunning, prepare_game
from ..domain.whitelist import WhitelistError
from .instance_api_view import InstanceAPIView


def _is_two_players(value: object) -> bool:
    return isinstance(value, list) and len(value) == 2


def _is_terrain_list(value: object) -> bool:
    return isinstance(value, list) and bool(value)


# Die grobe Form eines Spielstarts, in dieser Reihenfolge geprueft: Schluessel,
# Pruefung und die Meldung, wenn sie fehlschlaegt.
_START_CHECKS: list[tuple[str, Callable[[object], bool], str]] = [
    ("spieldaten", _is_two_players, "spieldaten fehlt oder enthaelt nicht genau zwei Spieler."),
    ("gelaende", _is_terrain_list, "gelaende fehlt oder ist keine Liste von Tile-Typen."),
    ("zugangscodes", valid_access_codes, "zugangscodes fehlt oder ist nicht zwei verschiedene Hex-Codes."),
]


def _start_request_error(data: dict[str, object]) -> Response | None:
    """Die Antwort 400 fuer die erste fehlgeschlagene Pruefung, sonst None."""
    key: str
    check: Callable[[object], bool]
    message: str
    for key, check, message in _START_CHECKS:
        if not check(data.get(key)):
            return Response({"detail": message}, status=status.HTTP_400_BAD_REQUEST)
    return None


class GameStartView(InstanceAPIView):
    """
    Startet ein Spiel auf einer Instanz, d.h. bereitet sie dafuer vor.
    """

    def post(self, request: Request, name: str) -> Response:
        """
        Setzt das Spiel auf der Instanz "name" auf und antwortet erst, wenn
        es bereit ist. Erwartet die Spieldaten nach radish/game/schema/
        spielstart.schema.json und daneben "zugangscodes", die Codes der
        beiden Spieler in derselben Reihenfolge (radish/backend/api/
        domain/servers/instance_client.py, game_start_request). Hier wird nur die grobe Form
        geprueft (zwei Spieler unter "spieldaten", eine Liste unter
        "gelaende", zwei verschiedene Codes) -- dass das Dokument dem Schema
        genuegt, stellt das Backend sicher (siehe dortige Tests).
        """
        data: dict[str, object] = request.data if isinstance(request.data, dict) else {}
        error: Response | None = _start_request_error(data)
        if error is not None:
            return error
        # Die Form ist geprueft (_START_CHECKS): zwei Listen und zwei Codes.
        spieler: list[object] = cast(list[object], data["spieldaten"])
        gelaende: list[object] = cast(list[object], data["gelaende"])
        codes: list[str] = cast(list[str], data["zugangscodes"])

        try:
            # In die Datei nur die Spieldaten und das Gelaende -- die Codes
            # bleiben ausserhalb.
            prepare_game(name, {"spieldaten": spieler, "gelaende": gelaende}, codes)
        except (OSError, WhitelistError, InstanceNotRunning) as exc:
            return Response(
                {"detail": f"Spiel konnte nicht aufgesetzt werden: {exc}"},
                status=status.HTTP_500_INTERNAL_SERVER_ERROR,
            )
        return Response({"instance": name, "status": "ready"})
