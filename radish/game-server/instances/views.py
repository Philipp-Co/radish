"""
Endpunkte, ueber die das Backend (radish/backend/) die Instanzen in diesem
Container steuert: ein Spiel starten und ein Spiel abbrechen.

Der Start ist synchron: das Backend ruft GameStartView auf, sobald der Host
sein Spiel aus der Lobby heraus startet (siehe radish/backend/api/
client_views.py, GameStartView). Die Instanz setzt das Spiel noch in diesem
Aufruf auf und antwortet erst danach -- 200 heisst, das Spiel ist bereit,
und die Spieler verlassen die Lobby.

Aufsetzen heisst: die Zugangscodes der beiden Spieler in die Whitelist von
zucchini eintragen, die Spieldaten als Datei ablegen und radish_server per
SIGUSR1 darauf hinweisen (siehe _prepare_game).

Die Zugangscodes sind geheim (radish/backend/api/access.py): das Backend
vergibt sie beim Start, setzt sie vor jede Nachricht seiner Spieler, und
zucchini laesst nur durch, was einen davon traegt. Sie stehen deshalb nicht
in der Spielstart-Datei -- radish_server braucht sie nicht --, sondern nur in
der Whitelist und, damit sie sich wieder austragen lassen, in zugang.json. Der C-Prozess meldet sich
nicht zurueck -- 200 heisst also "Datei liegt, Prozess ist benachrichtigt",
nicht "Prozess hat sie gelesen". Was er daraus gemacht hat, steht nur in
seinem Log.

Je Instanz ein Verzeichnis unter settings.RADISH_GAME_DATA_DIR:

    <RADISH_GAME_DATA_DIR>/<instanz>/spielstart.json     schreibt Django
    <RADISH_GAME_DATA_DIR>/<instanz>/zugang.json         schreibt Django
    <RADISH_GAME_DATA_DIR>/<instanz>/radish_server.pid   schreibt radish_server

Angelegt und beim Start der Instanz geleert wird es von
docker/game-server/game-instance-entrypoint.sh.

Eine Instanz wird ueber ihren Namen in der URL identifiziert -- derselbe
Name, unter dem sie sich beim Backend meldet (RADISH_GAME_SERVER_NAME, siehe
docker/game-server/register_instance.py, dort GameServer.name).
"""

import json
import logging
import os
import re
import signal
import subprocess
from pathlib import Path

from django.conf import settings
from django.http import Http404
from rest_framework import status
from rest_framework.permissions import AllowAny
from rest_framework.response import Response
from rest_framework.views import APIView


logger = logging.getLogger(__name__)

GAME_START_FILE = "spielstart.json"
ACCESS_FILE = "zugang.json"
PID_FILE = "radish_server.pid"

# Ein Zugangscode: eine 64-Bit-Zahl als hoechstens 16 Hexziffern, so wie
# zucchini_admin_client ihn erwartet. 0 ist in der Whitelist ein freier Platz.
_ACCESS_CODE = re.compile(r"^[0-9a-fA-F]{1,16}$")

# Wie lange ein Aufruf des Admin-Clients dauern darf. Er legt nur ein Ereignis in
# eine Queue und weckt zucchini -- das ist sofort erledigt.
ADMIN_CLIENT_TIMEOUT_SECONDS = 5

# Instanznamen, wie RADISH_GAME_SERVER_NAME sie vergibt (z.B.
# "game-instance-1"). Der Name wird Teil eines Pfads -- alles andere, etwa
# "../", wird abgewiesen, bevor er das tut.
_INSTANCE_NAME = re.compile(r"^[A-Za-z0-9_-]+$")


class InstanceNotRunning(Exception):
    """radish_server laeuft nicht (keine oder eine veraltete PID-Datei)."""


class WhitelistError(Exception):
    """zucchini_admin_client hat einen Code nicht ein- oder austragen koennen."""


def valid_access_codes(codes):
    """Genau zwei verschiedene Codes, je 1-16 Hexziffern und nicht 0."""
    if not isinstance(codes, list) or len(codes) != 2:
        return False
    if not all(isinstance(code, str) and _ACCESS_CODE.match(code) for code in codes):
        return False
    values = [int(code, 16) for code in codes]
    return 0 not in values and values[0] != values[1]


def _whitelist(command, code):
    """Ruft zucchini_admin_client mit "whitelist-add" oder "whitelist-remove"."""
    try:
        subprocess.run(
            [settings.ZUCCHINI_ADMIN_CLIENT, command, code],
            check=True,
            capture_output=True,
            timeout=ADMIN_CLIENT_TIMEOUT_SECONDS,
        )
    except (OSError, subprocess.SubprocessError) as exc:
        raise WhitelistError(f"{command} fehlgeschlagen: {exc}") from exc


def _revoke_access(directory):
    """
    Traegt die Codes des vorigen Spiels aus der Whitelist aus und loescht
    zugang.json. Fehler werden nur geloggt: ein Code, der stehen bleibt, gehoert
    zu einem Spiel, das es nicht mehr gibt -- und wird beim naechsten Aufraeumen
    erneut versucht, solange zugang.json bleibt.
    """
    access_file = directory / ACCESS_FILE
    try:
        codes = json.loads(access_file.read_text())
    except FileNotFoundError:
        return
    except ValueError:
        logger.warning("%s ist unlesbar und wird verworfen", access_file)
        access_file.unlink(missing_ok=True)
        return

    failed = False
    for code in codes if isinstance(codes, list) else []:
        try:
            _whitelist("whitelist-remove", code)
        except WhitelistError as exc:
            logger.warning("Code nicht aus der Whitelist entfernt: %s", exc)
            failed = True
    if not failed:
        access_file.unlink(missing_ok=True)


def _grant_access(directory, codes):
    """
    Traegt die Codes in die Whitelist ein und merkt sie sich in zugang.json --
    vor dem ersten Eintrag, damit auch ein halb gelungener Eintrag wieder
    ausgetragen werden kann. Wirft WhitelistError.
    """
    (directory / ACCESS_FILE).write_text(json.dumps(codes))
    for code in codes:
        _whitelist("whitelist-add", code)


def _instance_dir(name):
    if not _INSTANCE_NAME.match(name):
        raise Http404("Unbekannte Instanz.")
    return Path(settings.RADISH_GAME_DATA_DIR) / name


def _notify(directory):
    """
    Schickt SIGUSR1 an radish_server: die Spielstart-Datei hat sich
    geaendert. Wirft InstanceNotRunning, wenn es keinen laufenden Prozess
    dazu gibt.
    """
    try:
        pid = int((directory / PID_FILE).read_text().strip())
        os.kill(pid, signal.SIGUSR1)
    except FileNotFoundError as exc:
        raise InstanceNotRunning("keine PID-Datei -- radish_server laeuft nicht") from exc
    except ValueError as exc:
        raise InstanceNotRunning("PID-Datei ist unlesbar") from exc
    except ProcessLookupError as exc:
        raise InstanceNotRunning(f"kein Prozess mit PID {pid}") from exc


def _prepare_game(name, spieldaten, codes):
    """
    Setzt das Spiel auf der Instanz "name" auf: tauscht die Zugangscodes in der
    Whitelist gegen "codes" aus, legt spieldaten (das Dokument des Backends nach
    radish/game/schema/spielstart.schema.json) als Spielstart-Datei ab und
    benachrichtigt radish_server.

    Erst in eine temporaere Datei, dann per os.replace an ihren Platz: das
    Umbenennen ist atomar, radish_server sieht nie eine halb geschriebene
    Datei. Wirft OSError, wenn das Schreiben scheitert, WhitelistError, wenn
    zucchini die Codes nicht nimmt, und InstanceNotRunning, wenn niemand zu
    benachrichtigen ist.
    """
    directory = _instance_dir(name)
    if not directory.is_dir():
        raise OSError(f"kein Verzeichnis fuer Instanz '{name}'")
    _revoke_access(directory)
    _grant_access(directory, codes)

    target = directory / GAME_START_FILE
    temporary = directory / (GAME_START_FILE + ".tmp")
    with open(temporary, "w", encoding="utf-8") as file:
        json.dump(spieldaten, file, ensure_ascii=False)
    os.replace(temporary, target)
    _notify(directory)


def _cancel_game(name):
    """
    Loescht die Spielstart-Datei der Instanz "name" und benachrichtigt
    radish_server -- eine fehlende Datei heisst fuer ihn: kein Spiel mehr.
    Fehler werden nur geloggt, wie beim Abbruch im Backend: das Spiel ist
    dort ohnehin schon beendet.
    """
    directory = _instance_dir(name)
    _revoke_access(directory)
    try:
        (directory / GAME_START_FILE).unlink(missing_ok=True)
        _notify(directory)
    except (OSError, InstanceNotRunning) as exc:
        logger.warning("Abbruch auf Instanz '%s' nicht vollstaendig: %s", name, exc)


class InstanceAPIView(APIView):
    """
    Gemeinsame Basis aller Instanz-Endpunkte, damit die Auth spaeter an
    genau einer Stelle eingehaengt wird.

    Vorerst ohne Auth: keine Authentifizierung, jeder darf. Hier kommt
    spaeter z.B. eine Keycloak-JWT-Pruefung wie im Backend hin (siehe
    radish/backend/api/authentication.py). Ohne SessionAuthentication
    erzwingt DRF auch kein CSRF -- Aufrufe des Backends (Server zu Server,
    kein Browser) kommen deshalb ohne CSRF-Token durch. Nur fuer Dev.
    """

    authentication_classes = []
    permission_classes = [AllowAny]


class GameStartView(InstanceAPIView):
    """
    Startet ein Spiel auf einer Instanz, d.h. bereitet sie dafuer vor.
    """

    def post(self, request, name):
        """
        Setzt das Spiel auf der Instanz "name" auf und antwortet erst, wenn
        es bereit ist. Erwartet die Spieldaten nach radish/game/schema/
        spielstart.schema.json und daneben "zugangscodes", die Codes der
        beiden Spieler in derselben Reihenfolge (radish/backend/api/
        instance_client.py, game_start_request). Hier wird nur die grobe Form
        geprueft (zwei Spieler unter "spieldaten", zwei verschiedene Codes) --
        dass das Dokument dem Schema genuegt, stellt das Backend sicher (siehe
        dortige Tests).
        """
        data = request.data if isinstance(request.data, dict) else {}
        spieler = data.get("spieldaten")
        if not isinstance(spieler, list) or len(spieler) != 2:
            return Response(
                {"detail": "spieldaten fehlt oder enthaelt nicht genau zwei Spieler."},
                status=status.HTTP_400_BAD_REQUEST,
            )
        codes = data.get("zugangscodes")
        if not valid_access_codes(codes):
            return Response(
                {"detail": "zugangscodes fehlt oder ist nicht zwei verschiedene Hex-Codes."},
                status=status.HTTP_400_BAD_REQUEST,
            )

        try:
            # In die Datei nur die Spieldaten -- die Codes bleiben ausserhalb.
            _prepare_game(name, {"spieldaten": spieler}, codes)
        except (OSError, WhitelistError, InstanceNotRunning) as exc:
            return Response(
                {"detail": f"Spiel konnte nicht aufgesetzt werden: {exc}"},
                status=status.HTTP_500_INTERNAL_SERVER_ERROR,
            )
        return Response({"instance": name, "status": "ready"})


class GameCancelView(InstanceAPIView):
    """
    Bricht das Spiel auf einer Instanz ab.
    """

    def post(self, request, name):
        """
        Bricht das Spiel der Instanz "name" ab -- aufgerufen, wenn ein
        Spieler ein gestartetes Spiel verlaesst (siehe _cancel_game). Immer
        204: das Backend hat das Spiel bereits beendet und wertet die
        Antwort nicht aus.
        """
        _cancel_game(name)
        return Response(status=status.HTTP_204_NO_CONTENT)
