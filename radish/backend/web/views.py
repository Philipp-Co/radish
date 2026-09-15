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

from django.conf import settings
from django.http import Http404
from django.views.static import serve

# radish/backend/config/settings.py setzt BASE_DIR = radish/backend/ --
# radish/frontend/ und radish/web/ liegen als Geschwisterverzeichnisse
# daneben (siehe docker/web/Dockerfile fuer denselben Aufbau via nginx).
FRONTEND_DIST_DIR = Path(settings.BASE_DIR).parent / "frontend" / "dist" / "radish-frontend" / "browser"
WASM_DIR = Path(settings.BASE_DIR).parent / "web"


def index(request):
    """Liefert das gebaute Angular-index.html unter "/"."""
    return serve(request, "index.html", document_root=FRONTEND_DIST_DIR)


def wasm_file(request, filename):
    """
    Liefert eine Datei aus radish/web/ (client.js, client.wasm, ggf.
    weitere spaeter dazukommende Assets) unter wasm/<filename> -- passend zu
    environment.wasmBaseUrl im Frontend (radish/frontend/src/environments/).

    django.views.static.serve schuetzt selbst gegen Verzeichnis-Ausbrueche
    (ueber django.utils._os.safe_join) und liefert 404 bei unbekannten oder
    ausserhalb von WASM_DIR liegenden Pfaden.
    """
    return serve(request, filename, document_root=WASM_DIR)


def static_file(request, filename):
    """
    Liefert eine Datei aus dem Angular-Build (JS-/CSS-Bundles mit Hash im
    Namen, siehe angular.json "outputHashing": "all") unter ihrem eigenen
    Pfad -- passend zu den von Angular selbst generierten Referenzen in
    index.html, ohne fuer jede neue Datei eine eigene URL eintragen zu
    muessen.

    Existiert die angefragte Datei nicht, wird stattdessen index.html
    ausgeliefert (SPA-Fallback): Angular routet clientseitig, pfadbasiert
    (kein Hash-Routing, siehe app.routes.ts) -- ein direkter Aufruf oder
    Reload von z.B. "/games" waere sonst ein 404, obwohl die Route in der
    App existiert. Entspricht "try_files $uri $uri/ /index.html;" in
    docker/web/default.conf.
    """
    try:
        return serve(request, filename, document_root=FRONTEND_DIST_DIR)
    except Http404:
        return serve(request, "index.html", document_root=FRONTEND_DIST_DIR)
