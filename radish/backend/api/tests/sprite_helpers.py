"""
Helfer fuer die Tests der hochgeladenen Einheitenbilder: echte PNGs in jeder
Groesse und ein vollstaendiges Bild mit allen 17 Atlanten.
"""

import hashlib
import json
from pathlib import Path
import struct
import tempfile
import zlib

from django.test import SimpleTestCase, override_settings

from api.domain.sprites.problems import REQUIRED_ATLASES
from api.models import ROTATIONS, UnitSprite, UnitSpriteAtlas, UnitSpriteImage

# Das Schema der sprite.json, gegen das die erzeugten Dokumente geprueft werden.
SPRITE_SCHEMA_PATH = "client/schema/sprite.schema.json"


def png_bytes(width: int, height: int) -> bytes:
    """Eine gueltige, voll durchsichtige PNG (RGBA, 8 Bit) -- ohne Bildbibliothek."""

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    header: bytes = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    rows: bytes = b"".join(b"\x00" + b"\x00\x00\x00\x00" * width for _ in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


def add_image(sprite: UnitSprite, name: str, width: int = 512, height: int = 512) -> UnitSpriteImage:
    """Eine PNG zu "sprite", wie der Upload sie anlegt."""
    data: bytes = png_bytes(width, height)
    return UnitSpriteImage.objects.create(
        sprite=sprite, name=name, data=data, sha256=hashlib.sha256(data).hexdigest(), width=width, height=height
    )


def create_complete_sprite(key: str = "infantry") -> UnitSprite:
    """
    Ein vollstaendiges Bild: eine PNG von 512 x 512 (16 Zellen zu 128 x 128),
    alle 17 Atlanten darauf -- Drehungen mit vier Schluesselbildern, Folgen mit
    vier Bildern zu 10 fps.
    """
    sprite: UnitSprite = UnitSprite.objects.create(key=key, draw_width=96, step_ms=300.0)
    image: UnitSpriteImage = add_image(sprite, "sheet.png")
    animation: str
    facing: str
    for animation, facing in REQUIRED_ATLASES:
        rotation: bool = animation in ROTATIONS
        UnitSpriteAtlas.objects.create(
            sprite=sprite,
            animation=animation,
            facing=facing,
            image=image,
            frame_width=128,
            frame_height=128,
            frame_count=16 if rotation else 4,
            key_frames=[0, 4, 8, 12] if rotation else None,
            fps=None if rotation else 10.0,
        )
    return sprite


def use_built_manifest(test_case: SimpleTestCase) -> None:
    """
    Ein Build mit dem Bild "heavy_tank" fuer die Dauer des Tests -- wie
    radish/web/assets/manifest.json (settings.RADISH_WASM_DIR).
    """
    # pylint kennt enterContext nicht: der Kontext wird dort geschlossen.
    # pylint: disable-next=consider-using-with
    wasm_dir: Path = Path(test_case.enterContext(tempfile.TemporaryDirectory()))
    (wasm_dir / "assets").mkdir()
    (wasm_dir / "assets" / "manifest.json").write_text(
        json.dumps({"/assets/units/heavy_tank/sprite.json": "units/heavy_tank/sprite.0123456789ab.json"})
    )
    test_case.enterContext(override_settings(RADISH_WASM_DIR=wasm_dir))
