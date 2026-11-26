"""
Die Effekte fuer den Client: Eintraege fuer sein Asset-Manifest (Pfad im
Client -> Dateiname) und der Inhalt hinter jedem Dateinamen -- wie die
Einheitenbilder (api/domain/sprites/publishing.py), ausgeliefert unter
/wasm/assets/ (web/interfaces/sprite_views.py).

Jeder Dateiname traegt den Anfang des SHA-256 seines Inhalts:
"uploaded/effects/<key>/<name>.<hash>.png" und
"uploaded/effects/<key>/effect.<hash>.json". Im Client liegen sie unter
/assets/effects/<key>/; dort sucht er jeden Ordner mit einer effect.json
(view/rendering/iso_effect_images.c).

Ins Manifest kommt nur ein vollstaendiger Effekt (problems.py) -- und davon nur
die PNGs, die ein Atlas zeigt: der Client laedt jede Datei im Manifest. Der
Build des Clients bringt selbst keine Effekt-Bilder mit.
"""

import hashlib
import re

from django.db.models import Prefetch, QuerySet

from api.domain.effects.document import effect_json_bytes
from api.domain.effects.problems import effect_problems
from api.domain.sprites.publishing import HASH_LENGTH, PublishedFile, current_json
from api.models import Effect, EffectAtlas, EffectImage

PREFIX = "uploaded/effects"

_PUBLISHED = re.compile(
    rf"^{PREFIX}/(?P<key>[a-z0-9_]{{1,31}})/(?P<stem>[a-z0-9_]{{1,27}})\.(?P<hash>[0-9a-f]{{{HASH_LENGTH}}})"
    r"\.(?P<ext>png|json)$"
)


def _short_hash(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()[:HASH_LENGTH]


def _image_name(effect: Effect, image: EffectImage) -> str:
    stem: str = image.name.removesuffix(".png")
    return f"{PREFIX}/{effect.key}/{stem}.{image.sha256[:HASH_LENGTH]}.png"


def _published_effects() -> QuerySet[Effect]:
    """Alle Effekte samt Atlanten und deren PNGs -- ohne die Bytes."""
    return Effect.objects.prefetch_related(
        Prefetch("atlases", queryset=EffectAtlas.objects.select_related("image").defer("image__data"))
    )


def effect_manifest_entries(effect: Effect, atlases: list[EffectAtlas]) -> dict[str, str]:
    """Die Eintraege eines vollstaendigen Effekts fuer das Manifest; eine geteilte PNG einmal."""
    entries: dict[str, str] = {
        f"/assets/effects/{effect.key}/effect.json": (
            f"{PREFIX}/{effect.key}/effect.{_short_hash(effect_json_bytes(effect, atlases))}.json"
        )
    }
    atlas: EffectAtlas
    for atlas in atlases:
        entries[f"/assets/effects/{effect.key}/{atlas.image.name}"] = _image_name(effect, atlas.image)
    return entries


def uploaded_effect_manifest() -> dict[str, str]:
    """Die Eintraege aller vollstaendigen Effekte."""
    entries: dict[str, str] = {}
    effect: Effect
    for effect in _published_effects():
        atlases: list[EffectAtlas] = list(effect.atlases.all())
        if not effect_problems(effect, atlases):
            entries.update(effect_manifest_entries(effect, atlases))
    return entries


def published_effect_file(name: str) -> PublishedFile | None:
    """
    Der Inhalt hinter einem Dateinamen aus uploaded_effect_manifest(); None,
    wenn es ihn nicht (mehr) gibt -- auch fuer eine alte Fassung oder einen
    Effekt, der gerade nicht vollstaendig ist.
    """
    match: re.Match[str] | None = _PUBLISHED.match(name)
    if match is None:
        return None
    effect: Effect | None = _published_effects().filter(key=match["key"]).first()
    if effect is None:
        return None
    atlases: list[EffectAtlas] = list(effect.atlases.all())
    if effect_problems(effect, atlases):
        return None
    if match["ext"] == "json":
        return current_json(effect_json_bytes(effect, atlases), "effect", match)
    return _published_png(effect, {atlas.image.name for atlas in atlases}, match)


def _published_png(effect: Effect, shown: set[str], match: re.Match[str]) -> PublishedFile | None:
    """Eine PNG, die ein Atlas zeigt ("shown": ihre Namen), in der Fassung mit diesem Hash."""
    image_name: str = f"{match['stem']}.png"
    image: EffectImage | None = (
        EffectImage.objects.filter(effect=effect, name=image_name, sha256__startswith=match["hash"]).first()
        if image_name in shown
        else None
    )
    return None if image is None else PublishedFile(content=bytes(image.data), content_type="image/png")
