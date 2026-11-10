"""
Die hochgeladenen Einheitenbilder fuer den Client: Eintraege fuer sein
Asset-Manifest (Pfad im Client -> Dateiname) und der Inhalt hinter jedem
Dateinamen. Ausgeliefert unter /wasm/assets/ wie die Dateien aus dem Build
(web/interfaces/sprite_views.py); der Client merkt keinen Unterschied
(radish/frontend/src/app/shared/game-canvas/game-canvas.component.ts legt alles
ins Dateisystem des Moduls).

Jeder Dateiname traegt den Anfang des SHA-256 seines Inhalts, wie bei
radish/client/publish_assets.cmake: "uploaded/units/<key>/<name>.<hash>.png"
und "uploaded/units/<key>/sprite.<hash>.json". Eine neue Fassung hat einen neuen
Namen, und jede Datei darf dauerhaft gecacht werden.

Ins Manifest kommt nur ein vollstaendiges Bild (problems.py), und nur eines,
dessen Schluessel kein Bild aus dem Build traegt (builtin.py) -- und davon nur
die PNGs, die ein Atlas zeigt: der Client laedt jede Datei im Manifest.
"""

from dataclasses import dataclass
import hashlib
import json
import re

from django.db.models import Prefetch, QuerySet

from api.domain.sprites.builtin import built_sprite_keys
from api.domain.sprites.document import sprite_document
from api.domain.sprites.problems import sprite_problems
from api.models import UnitSprite, UnitSpriteAtlas, UnitSpriteImage

# Wie publish_assets.cmake: die ersten 12 Hexziffern.
HASH_LENGTH = 12
PREFIX = "uploaded/units"

_PUBLISHED = re.compile(
    rf"^{PREFIX}/(?P<key>[a-z0-9_]{{1,31}})/(?P<stem>[a-z0-9_]{{1,27}})\.(?P<hash>[0-9a-f]{{{HASH_LENGTH}}})"
    r"\.(?P<ext>png|json)$"
)


@dataclass(frozen=True)
class PublishedFile:
    """Eine Datei, wie der Client sie bekommt."""

    content: bytes
    content_type: str


def sprite_json_bytes(sprite: UnitSprite, atlases: list[UnitSpriteAtlas]) -> bytes:
    """Die sprite.json als Bytes -- dieselben Daten ergeben dieselben Bytes."""
    return (json.dumps(sprite_document(sprite, atlases), indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def _short_hash(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()[:HASH_LENGTH]


def _image_name(sprite: UnitSprite, image: UnitSpriteImage) -> str:
    stem: str = image.name.removesuffix(".png")
    return f"{PREFIX}/{sprite.key}/{stem}.{image.sha256[:HASH_LENGTH]}.png"


def _published_sprites() -> QuerySet[UnitSprite]:
    """Alle Bilder samt Atlanten und deren PNGs -- ohne die Bytes."""
    return UnitSprite.objects.prefetch_related(
        Prefetch("atlases", queryset=UnitSpriteAtlas.objects.select_related("image").defer("image__data"))
    ).exclude(key__in=built_sprite_keys())


def sprite_manifest_entries(sprite: UnitSprite, atlases: list[UnitSpriteAtlas]) -> dict[str, str]:
    """Die Eintraege eines vollstaendigen Bildes fuer das Manifest."""
    entries: dict[str, str] = {
        f"/assets/units/{sprite.key}/sprite.json": (
            f"{PREFIX}/{sprite.key}/sprite.{_short_hash(sprite_json_bytes(sprite, atlases))}.json"
        )
    }
    atlas: UnitSpriteAtlas
    for atlas in atlases:
        entries[f"/assets/units/{sprite.key}/{atlas.image.name}"] = _image_name(sprite, atlas.image)
    return entries


def uploaded_manifest() -> dict[str, str]:
    """Die Eintraege aller vollstaendigen hochgeladenen Bilder."""
    entries: dict[str, str] = {}
    sprite: UnitSprite
    for sprite in _published_sprites():
        atlases: list[UnitSpriteAtlas] = list(sprite.atlases.all())
        if not sprite_problems(sprite, atlases):
            entries.update(sprite_manifest_entries(sprite, atlases))
    return entries


def published_file(name: str) -> PublishedFile | None:
    """
    Der Inhalt hinter einem Dateinamen aus uploaded_manifest(); None, wenn es
    ihn nicht (mehr) gibt -- auch fuer eine alte Fassung oder ein Bild, das
    gerade nicht vollstaendig ist.
    """
    match: re.Match[str] | None = _PUBLISHED.match(name)
    if match is None:
        return None
    sprite: UnitSprite | None = _published_sprites().filter(key=match["key"]).first()
    if sprite is None:
        return None
    atlases: list[UnitSpriteAtlas] = list(sprite.atlases.all())
    if sprite_problems(sprite, atlases):
        return None
    if match["ext"] == "json":
        return _published_json(sprite, atlases, match)
    return _published_png(sprite, atlases, match)


def _published_json(sprite: UnitSprite, atlases: list[UnitSpriteAtlas], match: re.Match[str]) -> PublishedFile | None:
    """Die sprite.json, wenn Name und Hash zum heutigen Stand passen."""
    content: bytes = sprite_json_bytes(sprite, atlases)
    if match["stem"] != "sprite" or _short_hash(content) != match["hash"]:
        return None
    return PublishedFile(content=content, content_type="application/json")


def _published_png(sprite: UnitSprite, atlases: list[UnitSpriteAtlas], match: re.Match[str]) -> PublishedFile | None:
    """Eine PNG, die ein Atlas zeigt, in der Fassung mit diesem Hash."""
    image_name: str = f"{match['stem']}.png"
    if image_name not in {atlas.image.name for atlas in atlases}:
        return None
    image: UnitSpriteImage | None = UnitSpriteImage.objects.filter(
        sprite=sprite, name=image_name, sha256__startswith=match["hash"]
    ).first()
    if image is None:
        return None
    return PublishedFile(content=bytes(image.data), content_type="image/png")
