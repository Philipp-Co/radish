"""Die Dateien des Angular-Builds unter ihrem eigenen Pfad, mit SPA-Fallback."""

from django.http import Http404, HttpRequest
from django.http.response import HttpResponseBase

from web.interfaces.serving import serve_frontend


def static_file(request: HttpRequest, filename: str) -> HttpResponseBase:
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
        return serve_frontend(request, filename)
    except Http404:
        return serve_frontend(request, "index.html")
