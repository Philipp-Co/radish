"""
Helfer fuer die Tests der Tile-Bilder: PNGs zu einem Typ und ein Typ, der
aussieht wie das hohe Gras -- Grundbild und zwei Dekorationen.
"""

import hashlib

from api.models import TerrainDecor, TerrainImage, TerrainType
from api.tests.sprite_helpers import png_bytes

# Das Schema der tile.json, gegen das die erzeugten Dokumente geprueft werden.
TILE_SCHEMA_PATH = "client/schema/tile.schema.json"


def add_terrain_image(terrain: TerrainType, name: str, width: int = 128, height: int = 128) -> TerrainImage:
    """Eine PNG zu "terrain", wie der Upload sie anlegt."""
    data: bytes = png_bytes(width, height)
    return TerrainImage.objects.create(
        terrain=terrain, name=name, data=data, sha256=hashlib.sha256(data).hexdigest(), width=width, height=height
    )


def dress_like_high_grass(terrain: TerrainType) -> TerrainType:
    """Grundbild, dazu die hintere und die vordere Haelfte des Grases."""
    TerrainImage.objects.filter(pk=add_terrain_image(terrain, "grass_tile.png").pk).update(is_base=True)
    back: TerrainImage = add_terrain_image(terrain, "high_grass_back.png")
    front: TerrainImage = add_terrain_image(terrain, "high_grass_front.png")
    TerrainDecor.objects.create(terrain=terrain, layer=0, image=back, offset_x=6, offset_y=-66)
    TerrainDecor.objects.create(terrain=terrain, layer=1, image=front, offset_x=6, offset_y=-66, in_front=True)
    return terrain
