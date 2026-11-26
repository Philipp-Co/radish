"""
Die Einheiten-, Tile- und Effekt-Bilder aus der Datenbank fuer den Wasm-Client
(api/domain/sprites/publishing.py, api/domain/tiles/publishing.py,
api/domain/effects/publishing.py): das
Asset-Manifest unter /wasm/assets/manifest.json, um sie ergaenzt, und ihre
Dateien unter /wasm/assets/uploaded/... aus der Datenbank.

Der Client und der Admin-Katalog laden nur das Manifest und danach jede Datei,
die darin steht (shared/game-canvas/game-canvas.component.ts,
core/unit-sprite.service.ts). Ein vollstaendiges hochgeladenes Bild ist damit
fuer beide da, ohne neuen Build und ohne Neustart.
"""

from django.http import Http404, HttpRequest, HttpResponse, JsonResponse
from django.utils.cache import patch_cache_control

from api.domain.effects.publishing import published_effect_file, uploaded_effect_manifest
from api.domain.sprites.client_build import built_manifest
from api.domain.sprites.publishing import PublishedFile, published_file, uploaded_manifest
from api.domain.tiles.publishing import published_tile_file, uploaded_tile_manifest
from web.interfaces import serving


def asset_manifest(request: HttpRequest) -> HttpResponse:
    """
    Das Manifest aus dem Build (WASM_DIR/assets/manifest.json), dazu die
    Eintraege der Einheiten-, Tile- und Effekt-Bilder aus der Datenbank. Mit
    festem Namen und ohne Hash: es wird bei jeder Verwendung nachgeprueft
    (no-cache), wie bisher die Datei.
    """
    manifest: dict[str, str] = dict(built_manifest(serving.WASM_DIR))
    manifest.update(uploaded_manifest())
    manifest.update(uploaded_tile_manifest())
    manifest.update(uploaded_effect_manifest())
    response: JsonResponse = JsonResponse(manifest, json_dumps_params={"indent": 2, "ensure_ascii": False})
    patch_cache_control(response, no_cache=True)
    return response


def uploaded_asset(request: HttpRequest, name: str) -> HttpResponse:
    """
    Eine Datei eines hochgeladenen Bildes, wie sie im Manifest steht. Der Name
    traegt ihren Hash, deshalb darf sie dauerhaft gecacht werden; 404 fuer alles,
    was es so nicht (mehr) gibt.
    """
    path: str = f"uploaded/{name}"
    found: PublishedFile | None = (
        published_file(path) or published_tile_file(path) or published_effect_file(path)
    )
    if found is None:
        raise Http404(name)
    response: HttpResponse = HttpResponse(found.content, content_type=found.content_type)
    patch_cache_control(response, max_age=31536000, immutable=True, public=True)
    return response
