"""
Die effect.json eines Effekts, wie der Client sie liest
(radish/client/schema/effect.schema.json, radish/client/src/view/rendering/
iso_effect_sprite.c).
"""

from collections.abc import Iterable
import json

from api.domain.sprites.problems import FACINGS
from api.models import Effect, EffectAtlas

# Version der effect.json (radish/client/schema/effect.schema.json).
EFFECT_JSON_VERSION = 1


def effect_document(effect: Effect, atlases: Iterable[EffectAtlas]) -> dict[str, object]:
    """
    Die effect.json eines vollstaendigen Effekts (problems.effect_problems
    leer), in der Reihenfolge des Schemas -- gleiche Daten ergeben dieselben
    Bytes und damit denselben Hash (publishing.py). "atlases" samt image.
    """
    by_facing: dict[str, EffectAtlas] = {atlas.facing: atlas for atlas in atlases}
    return {
        "version": EFFECT_JSON_VERSION,
        "kind": effect.kind,
        "draw_width": effect.draw_width,
        "duration_ms": effect.duration_ms,
        "facings": {facing: _atlas_document(by_facing[facing]) for facing in FACINGS},
    }


def _atlas_document(atlas: EffectAtlas) -> dict[str, object]:
    return {
        "file": atlas.image.name,
        "frame_width": atlas.frame_width,
        "frame_height": atlas.frame_height,
        "first_frame": atlas.first_frame,
        "frame_count": atlas.frame_count,
        "anchor": {"x": atlas.anchor_x, "y": atlas.anchor_y},
    }


def effect_json_bytes(effect: Effect, atlases: Iterable[EffectAtlas]) -> bytes:
    """Die effect.json als Bytes -- dieselben Daten ergeben dieselben Bytes."""
    return (json.dumps(effect_document(effect, atlases), indent=2, ensure_ascii=False) + "\n").encode("utf-8")
