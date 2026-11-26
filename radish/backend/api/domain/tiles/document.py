"""
Die tile.json eines Tile-Typs, wie der Client sie liest
(radish/client/schema/tile.schema.json, radish/client/src/view/rendering/
iso_tile_sprite.c).
"""

from collections.abc import Iterable
import json

from api.models import TerrainDecor, TerrainImage


def tile_document(base: TerrainImage | None, decor: Iterable[TerrainDecor]) -> dict[str, object] | None:
    """
    Die tile.json eines Typs als Daten, aus seinem Grundbild ("base", None fuer
    keins) und seinen Dekorationen ("decor" samt image); None, wenn er beides
    nicht hat -- dann gibt es keine Datei.
    """
    entries: list[dict[str, object]] = [_decor_document(entry) for entry in sorted(decor, key=_layer)]
    document: dict[str, object] = {"version": 1}
    if base is not None:
        document["image"] = base.name
    if entries:
        document["decor"] = entries
    return document if len(document) > 1 else None


def _layer(entry: TerrainDecor) -> int:
    return entry.layer


def _decor_document(entry: TerrainDecor) -> dict[str, object]:
    return {
        "image": entry.image.name,
        "offset_x": entry.offset_x,
        "offset_y": entry.offset_y,
        "in_front": entry.in_front,
    }


def tile_json_bytes(document: dict[str, object]) -> bytes:
    """Die tile.json als Bytes -- dieselben Daten ergeben dieselben Bytes."""
    return (json.dumps(document, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
