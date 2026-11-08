"""
Spiele auf den Instanzen dieses Containers aufsetzen und abbrechen. Das
Backend (radish/backend/) loest beides ueber die Endpunkte in interfaces/ aus
(game_start_view.py, game_cancel_view.py).

Der Start ist synchron: das Backend ruft GameStartView auf, sobald der Host
sein Spiel aus der Lobby heraus startet (siehe radish/backend/api/
interfaces/player/views/client_views.py, GameStartView). Die Instanz setzt das Spiel noch in diesem
Aufruf auf und antwortet erst danach -- 200 heisst, das Spiel ist bereit,
und die Spieler verlassen die Lobby.

Aufsetzen heisst: die Zugangscodes der beiden Spieler in die Whitelist von
zucchini eintragen, die Spieldaten als Datei ablegen und radish_server per
SIGUSR1 darauf hinweisen (siehe prepare_game).

Die Zugangscodes sind geheim (radish/backend/api/domain/servers/access.py): das Backend
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

Eine Instanz wird ueber ihren Namen in der URL identifiziert -- ihren Namen
in diesem Container, wie supervisord.conf ihn an game-instance-entrypoint.sh
uebergibt. Derselbe Name ist ihr Zucchini-Name (zucchini_admin_client -n)
und ihr Datenverzeichnis. Beim Backend steht er als GameServer.instance_name;
dessen GameServer.name ist ein anderer, ueber alle Container eindeutiger
(siehe docker/game-server/register_instance.py).
"""

import json
import logging
import os
from pathlib import Path
import re
import signal
from typing import TextIO, TypedDict

from django.conf import settings
from django.http import Http404

from instances.domain.whitelist import grant_access, revoke_access

logger = logging.getLogger(__name__)


GAME_START_FILE = "spielstart.json"


PID_FILE = "radish_server.pid"


# Instanznamen, wie supervisord.conf sie vergibt (z.B. "game-instance-1"). Der
# Name wird Teil eines Pfads -- alles andere, etwa "../", wird abgewiesen,
# bevor er das tut.
_INSTANCE_NAME = re.compile(r"^[A-Za-z0-9_-]+$")


class InstanceNotRunningError(Exception):
    """radish_server laeuft nicht (keine oder eine veraltete PID-Datei)."""


class GameStartFile(TypedDict):
    """Was in spielstart.json landet: die Spieldaten des Backends ohne die Codes."""

    spieldaten: list[object]
    gelaende: list[object]


def _instance_dir(name: str) -> Path:
    if not _INSTANCE_NAME.match(name):
        raise Http404("Unbekannte Instanz.")
    return Path(settings.RADISH_GAME_DATA_DIR) / name


def _notify(directory: Path) -> None:
    """
    Schickt SIGUSR1 an radish_server: die Spielstart-Datei hat sich
    geaendert. Wirft InstanceNotRunningError, wenn es keinen laufenden Prozess
    dazu gibt.
    """
    try:
        pid: int = int((directory / PID_FILE).read_text().strip())
        os.kill(pid, signal.SIGUSR1)
    except FileNotFoundError as exc:
        raise InstanceNotRunningError("keine PID-Datei -- radish_server laeuft nicht") from exc
    except ValueError as exc:
        raise InstanceNotRunningError("PID-Datei ist unlesbar") from exc
    except ProcessLookupError as exc:
        raise InstanceNotRunningError(f"kein Prozess mit PID {pid}") from exc


def prepare_game(name: str, spieldaten: GameStartFile, codes: list[str]) -> None:
    """
    Setzt das Spiel auf der Instanz "name" auf: tauscht die Zugangscodes in der
    Whitelist gegen "codes" aus, legt spieldaten (das Dokument des Backends nach
    radish/game/schema/spielstart.schema.json) als Spielstart-Datei ab und
    benachrichtigt radish_server.

    Erst in eine temporaere Datei, dann per os.replace an ihren Platz: das
    Umbenennen ist atomar, radish_server sieht nie eine halb geschriebene
    Datei. Wirft OSError, wenn das Schreiben scheitert, WhitelistError, wenn
    zucchini die Codes nicht nimmt, und InstanceNotRunningError, wenn niemand zu
    benachrichtigen ist.
    """
    directory: Path = _instance_dir(name)
    if not directory.is_dir():
        raise OSError(f"kein Verzeichnis fuer Instanz '{name}'")
    revoke_access(name, directory)
    grant_access(name, directory, codes)

    target: Path = directory / GAME_START_FILE
    temporary: Path = directory / (GAME_START_FILE + ".tmp")
    file: TextIO
    with open(temporary, "w", encoding="utf-8") as file:
        json.dump(spieldaten, file, ensure_ascii=False)
    os.replace(temporary, target)
    _notify(directory)


def cancel_game(name: str) -> None:
    """
    Loescht die Spielstart-Datei der Instanz "name" und benachrichtigt
    radish_server -- eine fehlende Datei heisst fuer ihn: kein Spiel mehr.
    Fehler werden nur geloggt, wie beim Abbruch im Backend: das Spiel ist
    dort ohnehin schon beendet.
    """
    directory: Path = _instance_dir(name)
    revoke_access(name, directory)
    try:
        (directory / GAME_START_FILE).unlink(missing_ok=True)
        _notify(directory)
    except (OSError, InstanceNotRunningError) as exc:
        logger.warning("Abbruch auf Instanz '%s' nicht vollstaendig: %s", name, exc)
