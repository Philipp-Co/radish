"""Die Schluessel der Einheitenbilder aus dem Build (radish/assets/units/)."""

import json
from pathlib import Path
import re

from django.conf import settings

# Wie im Frontend (core/unit-sprite.service.ts): ein Bild hat seine sprite.json.
_SPRITE_PATH = re.compile(r"^/assets/units/([a-z0-9_]{1,31})/sprite\.json$")


def built_manifest(wasm_dir: Path | None = None) -> dict[str, str]:
    """
    Das Asset-Manifest des gebauten Clients (radish/client/publish_assets.cmake)
    in "wasm_dir", sonst in settings.RADISH_WASM_DIR: Pfad im Client ->
    Dateiname mit Hash. Leer, wenn es keinen Build gibt -- etwa lokal ohne Wasm
    oder in den Tests.
    """
    path: Path = Path(wasm_dir if wasm_dir is not None else settings.RADISH_WASM_DIR) / "assets" / "manifest.json"
    try:
        manifest: object = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return manifest if isinstance(manifest, dict) else {}


def built_sprite_keys() -> set[str]:
    """
    Die Schluessel der Bilder aus dem Build. Ein hochgeladenes Bild darf keinen
    davon tragen: sonst stuenden zwei Bilder unter demselben Namen, und eines
    wuerde still das andere verdecken.
    """
    keys: set[str] = set()
    path: str
    for path in built_manifest():
        match: re.Match[str] | None = _SPRITE_PATH.match(path)
        if match is not None:
            keys.add(match.group(1))
    return keys
