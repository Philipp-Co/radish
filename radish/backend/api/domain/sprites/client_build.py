"""Das Asset-Manifest des gebauten Wasm-Clients (radish/web/assets/manifest.json)."""

import json
from pathlib import Path

from django.conf import settings


def built_manifest(wasm_dir: Path | None = None) -> dict[str, str]:
    """
    Das Asset-Manifest des gebauten Clients (radish/client/publish_assets.cmake)
    in "wasm_dir", sonst in settings.RADISH_WASM_DIR: Pfad im Client ->
    Dateiname mit Hash. Leer, wenn es keinen Build gibt -- etwa lokal ohne Wasm
    oder in den Tests.

    Einheiten-, Tile- und Effekt-Bilder stehen darin nicht: die kommen alle aus
    der Datenbank (publishing.py, api/domain/tiles/publishing.py,
    api/domain/effects/publishing.py).
    """
    path: Path = Path(wasm_dir if wasm_dir is not None else settings.RADISH_WASM_DIR) / "assets" / "manifest.json"
    try:
        manifest: object = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return manifest if isinstance(manifest, dict) else {}
