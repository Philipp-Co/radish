"""
Helfer fuer die Tests der Effekte: PNGs zu einem Effekt und ein vollstaendiger
Effekt, der seine vier Richtungen hintereinander in einer PNG hat -- wie
explosion_2.
"""

import hashlib

from api.domain.sprites.problems import FACINGS
from api.models import Effect, EffectAtlas, EffectImage
from api.tests.sprite_helpers import png_bytes

# Das Schema der effect.json, gegen das die erzeugten Dokumente geprueft werden.
EFFECT_SCHEMA_PATH = "client/schema/effect.schema.json"


def add_effect_image(effect: Effect, name: str, width: int = 512, height: int = 512) -> EffectImage:
    """Eine PNG zu "effect", wie der Upload sie anlegt."""
    data: bytes = png_bytes(width, height)
    return EffectImage.objects.create(
        effect=effect, name=name, data=data, sha256=hashlib.sha256(data).hexdigest(), width=width, height=height
    )


def create_complete_effect(key: str = "explosion_2") -> Effect:
    """
    Ein vollstaendiger Effekt: eine PNG von 512 x 512 (16 Zellen zu 128 x 128),
    darin je Richtung vier Bilder, in der Reihenfolge sw, se, ne, nw.
    """
    effect: Effect = Effect.objects.create(key=key, draw_width=256, duration_ms=2500.0)
    image: EffectImage = add_effect_image(effect, "sheet.png")
    index: int
    facing: str
    for index, facing in enumerate(FACINGS):
        EffectAtlas.objects.create(
            effect=effect,
            facing=facing,
            image=image,
            frame_width=128,
            frame_height=128,
            first_frame=4 * index,
            frame_count=4,
            anchor_x=0.25,
            anchor_y=0.75,
        )
    return effect
