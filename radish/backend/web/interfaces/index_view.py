"""Das Angular-Frontend unter "/"."""

from django.http import HttpRequest
from django.http.response import HttpResponseBase

from web.interfaces.serving import serve_frontend


def index(request: HttpRequest) -> HttpResponseBase:
    """Liefert das gebaute Angular-index.html unter "/"."""
    return serve_frontend(request, "index.html")
