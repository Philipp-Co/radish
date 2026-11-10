"""Die sprite.json des Clients aus einem hochgeladenen Einheitenbild."""

from collections.abc import Iterable

from api.domain.sprites.problems import FACINGS
from api.models import ROTATIONS, SpriteAnimation, SpriteFacing, UnitSprite, UnitSpriteAtlas

# Version der sprite.json (radish/client/schema/sprite.schema.json).
SPRITE_JSON_VERSION = 1

# Name in "timing_ms" -> Feld am UnitSprite.
_TIMINGS: tuple[tuple[str, str], ...] = (
    ("step", "step_ms"),
    ("turn", "turn_ms"),
    ("aim", "aim_ms"),
    ("aim_hold", "aim_hold_ms"),
)


def _atlas_document(atlas: UnitSpriteAtlas) -> dict[str, object]:
    document: dict[str, object] = {
        "file": atlas.image.name,
        "frame_width": atlas.frame_width,
        "frame_height": atlas.frame_height,
        "frame_count": atlas.frame_count,
    }
    if atlas.animation in ROTATIONS:
        document["key_frames"] = list(atlas.key_frames or [])
    else:
        document["fps"] = atlas.fps
    return document


def sprite_document(sprite: UnitSprite, atlases: Iterable[UnitSpriteAtlas]) -> dict[str, object]:
    """
    Die sprite.json eines vollstaendigen Bildes (problems.sprite_problems leer),
    in der Reihenfolge des Schemas -- gleiche Daten ergeben dieselben Bytes und
    damit denselben Hash (publishing.py). "atlases" samt image.
    """
    document: dict[str, object] = {
        "version": SPRITE_JSON_VERSION,
        "animations": _animations_document(atlases),
        "anchor": {"x": sprite.anchor_x, "y": sprite.anchor_y},
        "draw_width": sprite.draw_width,
        "own_shadow": sprite.own_shadow,
    }
    timing: dict[str, float] = _timing_document(sprite)
    if timing:
        document["timing_ms"] = timing
    return document


def _animations_document(atlases: Iterable[UnitSpriteAtlas]) -> dict[str, object]:
    """"animations": die Drehung fuer alle Richtungen, jede andere je Richtung."""
    by_slot: dict[tuple[str, str], UnitSpriteAtlas] = {(a.animation, a.facing): a for a in atlases}
    animations: dict[str, object] = {
        SpriteAnimation.TURN.value: _atlas_document(by_slot[(SpriteAnimation.TURN, SpriteFacing.NONE)])
    }
    animation: SpriteAnimation
    for animation in (SpriteAnimation.AIM, SpriteAnimation.MOVE, SpriteAnimation.FIRE, SpriteAnimation.DESTROYED):
        animations[animation.value] = {facing: _atlas_document(by_slot[(animation, facing)]) for facing in FACINGS}
    return animations


def _timing_document(sprite: UnitSprite) -> dict[str, float]:
    """"timing_ms": nur die Zeiten, die das Bild selbst nennt."""
    timing: dict[str, float] = {}
    name: str
    field: str
    for name, field in _TIMINGS:
        value: float | None = getattr(sprite, field)
        if value is not None:
            timing[name] = value
    return timing
