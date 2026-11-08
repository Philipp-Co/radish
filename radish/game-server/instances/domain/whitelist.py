"""
Die Whitelist von zucchini: Zugangscodes ein- und austragen, ueber
zucchini_admin_client. Welche Codes gerade eingetragen sind, merkt sich
zugang.json im Verzeichnis der Instanz (siehe game_files.py).
"""

import json
import logging
from pathlib import Path
import subprocess
from typing import cast

from django.conf import settings

logger = logging.getLogger(__name__)


ACCESS_FILE = "zugang.json"


# Wie lange ein Aufruf des Admin-Clients dauern darf. Er legt nur ein Ereignis in
# eine Queue und weckt zucchini -- das ist sofort erledigt.
ADMIN_CLIENT_TIMEOUT_SECONDS = 5


class WhitelistError(Exception):
    """zucchini_admin_client hat einen Code nicht ein- oder austragen koennen."""


def _whitelist(name: str, command: str, code: str) -> None:
    """
    Ruft zucchini_admin_client mit "whitelist-add" oder "whitelist-remove" fuer
    die Zucchini-Instanz "name" auf.
    """
    try:
        subprocess.run(
            [settings.ZUCCHINI_ADMIN_CLIENT, "-n", name, command, code],
            check=True,
            capture_output=True,
            timeout=ADMIN_CLIENT_TIMEOUT_SECONDS,
        )
    except (OSError, subprocess.SubprocessError) as exc:
        raise WhitelistError(f"{command} fehlgeschlagen: {exc}") from exc


def _load_access_codes(access_file: Path) -> list[str] | None:
    """
    Die Codes aus zugang.json; None, wenn es nichts auszutragen gibt. Eine
    unlesbare Datei wird verworfen, ein Inhalt, der keine Liste ist, gilt als
    leere Liste.
    """
    try:
        codes: object = json.loads(access_file.read_text())
    except FileNotFoundError:
        return None
    except ValueError:
        logger.warning("%s ist unlesbar und wird verworfen", access_file)
        access_file.unlink(missing_ok=True)
        return None
    # zugang.json schreibt nur grant_access, mit Codes, die valid_access_codes
    # geprueft hat -- eine Liste darin enthaelt also Strings.
    return cast(list[str], codes) if isinstance(codes, list) else []


def _remove_codes(name: str, codes: list[str]) -> bool:
    """Traegt die Codes aus der Whitelist aus; True, wenn alle entfernt sind."""
    failed: bool = False
    code: str
    for code in codes:
        try:
            _whitelist(name, "whitelist-remove", code)
        except WhitelistError as exc:
            logger.warning("Code nicht aus der Whitelist entfernt: %s", exc)
            failed = True
    return not failed


def revoke_access(name: str, directory: Path) -> None:
    """
    Traegt die Codes des vorigen Spiels aus der Whitelist aus und loescht
    zugang.json. Fehler werden nur geloggt: ein Code, der stehen bleibt, gehoert
    zu einem Spiel, das es nicht mehr gibt -- und wird beim naechsten Aufraeumen
    erneut versucht, solange zugang.json bleibt.
    """
    access_file: Path = directory / ACCESS_FILE
    codes: list[str] | None = _load_access_codes(access_file)
    if codes is None:
        return
    if _remove_codes(name, codes):
        access_file.unlink(missing_ok=True)


def grant_access(name: str, directory: Path, codes: list[str]) -> None:
    """
    Traegt die Codes in die Whitelist ein und merkt sie sich in zugang.json --
    vor dem ersten Eintrag, damit auch ein halb gelungener Eintrag wieder
    ausgetragen werden kann. Wirft WhitelistError.
    """
    (directory / ACCESS_FILE).write_text(json.dumps(codes))
    code: str
    for code in codes:
        _whitelist(name, "whitelist-add", code)
