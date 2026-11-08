"""
Views, um sowohl das gebaute Angular-Frontend (radish/frontend/, Login/
Spieleliste/Spielansicht) als auch den WASM-Client (radish/web/, von
radish/client/CMakeLists.txt per RUNTIME_OUTPUT_DIRECTORY dorthin gebaut:
client.js, client.wasm) direkt aus Django auszuliefern -- ohne externe
Abhaengigkeit wie whitenoise, nur mit Djangos eigenem
django.views.static.serve. Es gibt dafuer bewusst keinen separaten
Webserver-Container mehr: Django ist der einzige Dienst, der Anfragen des
Browsers beantwortet (siehe docker/backend/Dockerfile, das radish/frontend/
im eigenen Node-Stage mitbaut).

Bewusst nicht ueber STATIC_URL/die staticfiles-App geloest: die dient den
eigenen Static-Assets von Django (z.B. Admin-CSS) unter "/static/" und
serviert Inhalte aus STATICFILES_DIRS nur automatisch, wenn DEBUG=True ist
(siehe Django-Doku zu django.contrib.staticfiles). Hier soll das Frontend
dagegen unter "/" erreichbar sein und unabhaengig von DEBUG funktionieren,
daher eigene schlanke Views statt eine Umleitung ueber staticfiles.

django.views.static.serve ist laut Django-Doku fuer echten Produktivbetrieb
mit hoher Last nicht die empfohlene Loesung (kein Caching, kein
Range-Support) -- fuer dieses Projekt (kleiner Spiele-Client, keine
externen Abhaengigkeiten gewuenscht) reicht das aber.
"""

from pathlib import Path
import re

from django.conf import settings
from django.http import FileResponse, Http404, HttpRequest
from django.http.response import HttpResponseBase
from django.utils.cache import patch_cache_control, patch_vary_headers
from django.views.static import serve

# radish/backend/config/settings.py setzt BASE_DIR = radish/backend/ --
# radish/frontend/ und radish/web/ liegen als Geschwisterverzeichnisse
# daneben (siehe docker/web/Dockerfile fuer denselben Aufbau via nginx).
FRONTEND_DIST_DIR = Path(settings.BASE_DIR).parent / "frontend" / "dist" / "radish-frontend" / "browser"
WASM_DIR = Path(settings.BASE_DIR).parent / "web"


def _accepts_gzip(request: HttpRequest) -> bool:
    """Ob Accept-Encoding gzip erlaubt (und nicht per "gzip;q=0" ablehnt)."""
    part: str
    name: str
    params: list[str]
    for part in request.headers.get("Accept-Encoding", "").split(","):
        name, *params = (p.strip() for p in part.split(";"))
        if name.lower() == "gzip":
            # Der erste gzip-Eintrag entscheidet.
            return _gzip_params_allow(params)
    return False


def _gzip_params_allow(params: list[str]) -> bool:
    """q-Wert des gzip-Eintrags: ohne q erlaubt, q>0 erlaubt, q=0 oder ungueltig nicht."""
    param: str
    for param in params:
        if param.startswith("q="):
            try:
                return float(param[2:]) > 0
            except ValueError:
                return False
    return True


# Dateien mit Inhalts-Hash im Namen: die Angular-Bundles ("main-R6I5S75B.js",
# angular.json "outputHashing": "all") und die Client-Assets
# ("turn.04b160980f5a.png", radish/client/publish_assets.cmake). Aendert sich
# der Inhalt, aendert sich der Name -- der Browser darf sie also behalten,
# ohne je nachzufragen.
_HASHED_FILE = re.compile(r"(-[A-Z0-9]{8}\.(js|css)|\.[0-9a-f]{12}\.[a-z0-9]+)$")


def _set_cache_control(response: HttpResponseBase, filename: str) -> None:
    """
    Gehashte Dateien ein Jahr und unveraenderlich; alles andere (index.html,
    client.js/client.wasm, assets/manifest.json) traegt einen festen Namen und
    wird bei jeder Verwendung per If-Modified-Since nachgeprueft (304, solange
    unveraendert) -- sonst liefe ein alter client.js mit einem neuen Manifest.
    """
    if _HASHED_FILE.search(filename):
        patch_cache_control(response, max_age=31536000, immutable=True, public=True)
    else:
        patch_cache_control(response, no_cache=True)


def serve_file(request: HttpRequest, filename: str, document_root: Path) -> HttpResponseBase:
    """
    Wie serve(), bevorzugt aber eine vorkomprimierte "<filename>.gz" daneben
    (im Docker-Image beim Build erzeugt, siehe docker/backend/Dockerfile).
    serve() setzt fuer "client.js.gz" selbst Content-Type text/javascript und
    Content-Encoding gzip (mimetypes.guess_type erkennt die Endung als
    Kodierung). Ohne .gz -- lokal oder bei Bildern -- das Original wie bisher.
    """
    if _accepts_gzip(request):
        try:
            response: FileResponse = serve(request, f"{filename}.gz", document_root=str(document_root))
        except Http404:
            pass
        else:
            patch_vary_headers(response, ("Accept-Encoding",))
            _set_cache_control(response, filename)
            return response
    response = serve(request, filename, document_root=str(document_root))
    patch_vary_headers(response, ("Accept-Encoding",))
    _set_cache_control(response, filename)
    return response


def serve_frontend(request: HttpRequest, filename: str) -> HttpResponseBase:
    """Eine Datei aus dem Angular-Build (FRONTEND_DIST_DIR)."""
    return serve_file(request, filename, FRONTEND_DIST_DIR)


def serve_wasm(request: HttpRequest, filename: str) -> HttpResponseBase:
    """Eine Datei des Wasm-Clients (WASM_DIR)."""
    return serve_file(request, filename, WASM_DIR)
