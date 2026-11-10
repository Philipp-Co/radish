"""
Ob ein hochgeladenes Einheitenbild vollstaendig und gueltig ist -- dieselben
Regeln, mit denen der Client die sprite.json liest
(radish/client/src/rendering/iso_unit_sprite.c,
radish/client/schema/sprite.schema.json), dazu, dass jede PNG genug Zellen hat
(im Client: RAD_IsoUnitAtlasColumns). Nur ein Bild ohne Probleme kommt ins
Asset-Manifest (publishing.py).
"""

from collections.abc import Iterable

from api.models import (
    ROTATIONS,
    SPRITE_FPS_MAX,
    SPRITE_FRAMES_MAX,
    SpriteAnimation,
    SpriteFacing,
    UnitSprite,
    UnitSpriteAtlas,
)

# Die Blickrichtungen in der Reihenfolge des Clients (RAD_IsoFacing_t).
FACINGS: tuple[str, ...] = (SpriteFacing.SW, SpriteFacing.SE, SpriteFacing.NE, SpriteFacing.NW)

# Jeder Atlas, den ein Bild braucht: die Drehung einmal, jede andere Animation
# je Blickrichtung -- 17, wie RAD_ISO_UNIT_ATLASES im Client.
REQUIRED_ATLASES: tuple[tuple[str, str], ...] = (
    (SpriteAnimation.TURN, SpriteFacing.NONE),
    *(
        (animation, facing)
        for animation in (SpriteAnimation.AIM, SpriteAnimation.MOVE, SpriteAnimation.FIRE, SpriteAnimation.DESTROYED)
        for facing in FACINGS
    ),
)


def atlas_label(animation: str, facing: str) -> str:
    """Wie ein Atlas in Meldungen heisst, z.B. "Zielen, links unten"."""
    name: str = SpriteAnimation(animation).label
    return name if not facing else f"{name}, {SpriteFacing(facing).label}"


def atlas_problems(atlas: UnitSpriteAtlas) -> list[str]:
    """Was an einem Atlas nicht stimmt -- fuer sich und gegen seine PNG."""
    label: str = atlas_label(atlas.animation, atlas.facing)
    problems: list[str] = []
    if atlas.animation in ROTATIONS:
        problems += _rotation_problems(atlas, label)
    else:
        problems += _sequence_problems(atlas, label)

    columns: int = atlas.image.width // atlas.frame_width
    rows: int = atlas.image.height // atlas.frame_height
    if columns * rows < atlas.frame_count:
        problems.append(
            f"{label}: {atlas.image.name} ({atlas.image.width} x {atlas.image.height}) hat Platz fuer "
            f"{columns * rows} Zellen von {atlas.frame_width} x {atlas.frame_height}, gebraucht werden "
            f"{atlas.frame_count}."
        )
    return problems


def _rotation_problems(atlas: UnitSpriteAtlas, label: str) -> list[str]:
    """Eine Drehung: vier Schluesselbilder, streng aufsteigend, alle im Atlas; keine fps."""
    problems: list[str] = []
    if atlas.fps is not None:
        problems.append(f"{label}: eine Drehung hat keine Bilder je Sekunde.")
    keys: list[int] | None = _key_frames(atlas.key_frames)
    if keys is None:
        problems.append(f"{label}: braucht vier Schluesselbilder (0, 90, 180, 270 Grad).")
        return problems
    if not _strictly_ascending(keys):
        problems.append(f"{label}: die Schluesselbilder muessen streng aufsteigen.")
    if keys[-1] >= atlas.frame_count:
        problems.append(f"{label}: Schluesselbild {keys[-1]} liegt hinter dem letzten von {atlas.frame_count} Bildern.")
    return problems


def _key_frames(value: object) -> list[int] | None:
    """Die Schluesselbilder, wenn es vier ganze Zahlen sind; sonst None."""
    if not isinstance(value, list) or len(value) != len(FACINGS):
        return None
    return value if all(map(_is_int, value)) else None


def _is_int(value: object) -> bool:
    """Eine ganze Zahl -- True und False zaehlen in Python auch dazu, hier nicht."""
    return isinstance(value, int) and not isinstance(value, bool)


def _strictly_ascending(keys: list[int]) -> bool:
    """Ab 0 und jedes groesser als das davor."""
    return keys[0] >= 0 and all(later > earlier for earlier, later in zip(keys, keys[1:]))


def _sequence_problems(atlas: UnitSpriteAtlas, label: str) -> list[str]:
    """Eine Folge: Bilder je Sekunde in (0, SPRITE_FPS_MAX]; keine Schluesselbilder."""
    problems: list[str] = []
    if atlas.key_frames is not None:
        problems.append(f"{label}: eine Folge hat keine Schluesselbilder.")
    if atlas.fps is None or not 0.0 < atlas.fps <= SPRITE_FPS_MAX:
        problems.append(f"{label}: braucht Bilder je Sekunde (mehr als 0, hoechstens {SPRITE_FPS_MAX:g}).")
    if atlas.frame_count > SPRITE_FRAMES_MAX:
        problems.append(f"{label}: hoechstens {SPRITE_FRAMES_MAX} Bilder.")
    return problems


def sprite_problems(sprite: UnitSprite, atlases: Iterable[UnitSpriteAtlas] | None = None) -> list[str]:
    """
    Was einem Bild fehlt oder an ihm nicht stimmt; leer, wenn es vollstaendig
    ist. "atlases" darf schon geladen uebergeben werden (samt image), sonst
    fragt es sie selbst ab.
    """
    if atlases is None:
        atlases = sprite.atlases.select_related("image").defer("image__data")
    by_slot: dict[tuple[str, str], UnitSpriteAtlas] = {(a.animation, a.facing): a for a in atlases}

    problems: list[str] = []
    slot: tuple[str, str]
    for slot in REQUIRED_ATLASES:
        problems += _slot_problems(*slot, by_slot.pop(slot, None))
    return problems + [_extra_atlas(*slot) for slot in by_slot]


def _extra_atlas(animation: str, facing: str) -> str:
    """Ein Atlas, den kein Bild braucht."""
    return f"Atlas gehoert nicht dazu: {animation} {facing or '(ohne Richtung)'}."


def _slot_problems(animation: str, facing: str, atlas: UnitSpriteAtlas | None) -> list[str]:
    """Was an einem Platz fehlt oder nicht stimmt."""
    if atlas is None:
        return [f"Atlas fehlt: {atlas_label(animation, facing)}."]
    return atlas_problems(atlas)
