"""Der Wasm-Client unter "/wasm/"."""

from django.http import HttpRequest
from django.http.response import HttpResponseBase

from web.interfaces.serving import serve_wasm


def wasm_file(request: HttpRequest, filename: str) -> HttpResponseBase:
    """
    Liefert eine Datei aus radish/web/ (client.js, client.wasm, ggf.
    weitere spaeter dazukommende Assets) unter wasm/<filename> -- passend zu
    environment.wasmBaseUrl im Frontend (radish/frontend/src/environments/).

    django.views.static.serve schuetzt selbst gegen Verzeichnis-Ausbrueche
    (ueber django.utils._os.safe_join) und liefert 404 bei unbekannten oder
    ausserhalb von WASM_DIR liegenden Pfaden.
    """
    return serve_wasm(request, filename)
