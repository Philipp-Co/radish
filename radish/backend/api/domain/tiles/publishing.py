"""
Die Tile-Bilder fuer den Client: Eintraege fuer sein Asset-Manifest (Pfad im
Client -> Dateiname) und der Inhalt hinter jedem Dateinamen -- wie die
Einheitenbilder (api/domain/sprites/publishing.py), ausgeliefert unter
/wasm/assets/ (web/interfaces/sprite_views.py).

Jeder Dateiname traegt den Anfang des SHA-256 seines Inhalts:
"uploaded/tiles/<code>/<name>.<hash>.png" und
"uploaded/tiles/<code>/tile.<hash>.json". Im Client liegen sie unter
/assets/tiles/<code>/.

Ins Manifest kommt jeder Typ mit Bild oder Dekoration -- und davon nur die
PNGs, die er zeigt: der Client laedt jede Datei im Manifest. Der Build des
Clients bringt selbst keine Tile-Bilder mit.
"""

import hashlib
import re

from django.db.models import Prefetch, QuerySet

from api.domain.sprites.publishing import HASH_LENGTH, PublishedFile
from api.domain.tiles.document import tile_document, tile_json_bytes
from api.models import TerrainDecor, TerrainImage, TerrainType

PREFIX = "uploaded/tiles"

_PUBLISHED = re.compile(
    rf"^{PREFIX}/(?P<code>[a-z0-9_]{{1,31}})/(?P<stem>[a-z0-9_]{{1,27}})\.(?P<hash>[0-9a-f]{{{HASH_LENGTH}}})"
    r"\.(?P<ext>png|json)$"
)


def _short_hash(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()[:HASH_LENGTH]


def _image_name(terrain: TerrainType, image: TerrainImage) -> str:
    stem: str = image.name.removesuffix(".png")
    return f"{PREFIX}/{terrain.code}/{stem}.{image.sha256[:HASH_LENGTH]}.png"


def published_terrain() -> QuerySet[TerrainType]:
    """Alle Typen samt Grundbild und Dekorationen mit ihren PNGs -- ohne die Bytes."""
    return TerrainType.objects.prefetch_related(
        Prefetch("images", queryset=TerrainImage.objects.filter(is_base=True).defer("data"), to_attr="base_images"),
        Prefetch("decor", queryset=TerrainDecor.objects.select_related("image").defer("image__data")),
    )


def base_image(terrain: TerrainType) -> TerrainImage | None:
    """Das Grundbild eines Typs aus published_terrain(); None fuer keins."""
    images: list[TerrainImage] = getattr(terrain, "base_images")
    return images[0] if images else None


def shown_images(base: TerrainImage | None, decor: list[TerrainDecor]) -> list[TerrainImage]:
    """Die PNGs, die der Typ zeigt, jede einmal."""
    images: dict[str, TerrainImage] = {}
    if base is not None:
        images[base.name] = base
    entry: TerrainDecor
    for entry in decor:
        images[entry.image.name] = entry.image
    return list(images.values())


def tile_manifest_entries(terrain: TerrainType) -> dict[str, str]:
    """Die Eintraege eines Typs aus published_terrain() fuer das Manifest; leer ohne Bild und Dekoration."""
    base: TerrainImage | None = base_image(terrain)
    decor: list[TerrainDecor] = list(terrain.decor.all())
    document: dict[str, object] | None = tile_document(base, decor)
    if document is None:
        return {}
    entries: dict[str, str] = {
        f"/assets/tiles/{terrain.code}/tile.json": (
            f"{PREFIX}/{terrain.code}/tile.{_short_hash(tile_json_bytes(document))}.json"
        )
    }
    image: TerrainImage
    for image in shown_images(base, decor):
        entries[f"/assets/tiles/{terrain.code}/{image.name}"] = _image_name(terrain, image)
    return entries


def uploaded_tile_manifest() -> dict[str, str]:
    """Die Eintraege aller Typen mit Bild oder Dekoration."""
    entries: dict[str, str] = {}
    terrain: TerrainType
    for terrain in published_terrain():
        entries.update(tile_manifest_entries(terrain))
    return entries


def published_tile_file(name: str) -> PublishedFile | None:
    """
    Der Inhalt hinter einem Dateinamen aus uploaded_tile_manifest(); None, wenn
    es ihn nicht (mehr) gibt -- auch fuer eine alte Fassung.
    """
    match: re.Match[str] | None = _PUBLISHED.match(name)
    if match is None:
        return None
    terrain: TerrainType | None = published_terrain().filter(code=match["code"]).first()
    if terrain is None:
        return None
    base: TerrainImage | None = base_image(terrain)
    decor: list[TerrainDecor] = list(terrain.decor.all())
    if match["ext"] == "json":
        return _published_json(base, decor, match)
    return _published_png(terrain, shown_images(base, decor), match)


def _published_json(base: TerrainImage | None, decor: list[TerrainDecor], match: re.Match[str]) -> PublishedFile | None:
    """Die tile.json, wenn Name und Hash zum heutigen Stand passen."""
    document: dict[str, object] | None = tile_document(base, decor)
    if document is None or match["stem"] != "tile":
        return None
    content: bytes = tile_json_bytes(document)
    if _short_hash(content) != match["hash"]:
        return None
    return PublishedFile(content=content, content_type="application/json")


def _published_png(terrain: TerrainType, shown: list[TerrainImage], match: re.Match[str]) -> PublishedFile | None:
    """Eine PNG, die der Typ zeigt, in der Fassung mit diesem Hash."""
    image_name: str = f"{match['stem']}.png"
    if image_name not in {image.name for image in shown}:
        return None
    image: TerrainImage | None = TerrainImage.objects.filter(
        terrain=terrain, name=image_name, sha256__startswith=match["hash"]
    ).first()
    if image is None:
        return None
    return PublishedFile(content=bytes(image.data), content_type="image/png")
