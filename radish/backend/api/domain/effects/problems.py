"""
Ob ein Effekt vollstaendig und gueltig ist -- dieselben Regeln, mit denen der
Client die effect.json liest (radish/client/src/view/rendering/
iso_effect_sprite.c, radish/client/schema/effect.schema.json), dazu, dass jede
PNG genug Zellen fuer ihre Folge hat (im Client: RAD_IsoEffectAtlasColumns).
Nur ein Effekt ohne Probleme kommt ins Asset-Manifest (publishing.py).
"""

from collections.abc import Iterable

from api.domain.sprites.problems import FACINGS
from api.models import Effect, EffectAtlas, SpriteFacing


def facing_label(facing: str) -> str:
    """Wie eine Richtung in Meldungen heisst, z.B. "links unten"."""
    return str(SpriteFacing(facing).label)


def atlas_problems(atlas: EffectAtlas) -> list[str]:
    """Was an einer Folge gegen ihre PNG nicht stimmt."""
    columns: int = atlas.image.width // atlas.frame_width
    rows: int = atlas.image.height // atlas.frame_height
    needed: int = atlas.first_frame + atlas.frame_count
    if columns * rows >= needed:
        return []
    return [
        f"{facing_label(atlas.facing)}: {atlas.image.name} ({atlas.image.width} x {atlas.image.height}) hat Platz "
        f"fuer {columns * rows} Zellen von {atlas.frame_width} x {atlas.frame_height}, gebraucht werden {needed}."
    ]


def effect_problems(effect: Effect, atlases: Iterable[EffectAtlas] | None = None) -> list[str]:
    """
    Was einem Effekt fehlt oder an ihm nicht stimmt; leer, wenn er vollstaendig
    ist. "atlases" darf schon geladen uebergeben werden (samt image), sonst
    fragt es sie selbst ab.
    """
    if atlases is None:
        atlases = effect.atlases.select_related("image").defer("image__data")
    by_facing: dict[str, EffectAtlas] = {atlas.facing: atlas for atlas in atlases}

    problems: list[str] = []
    facing: str
    for facing in FACINGS:
        atlas: EffectAtlas | None = by_facing.get(facing)
        if atlas is None:
            problems.append(f"Atlas fehlt: {facing_label(facing)}.")
        else:
            problems += atlas_problems(atlas)
    return problems
