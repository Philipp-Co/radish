import mimetypes

from django.apps import AppConfig


class WebConfig(AppConfig):
    """
    Liefert das WASM-Web-Frontend aus radish/web/ direkt aus Django aus
    (siehe web/views.py) -- ohne externe Abhaengigkeit wie whitenoise,
    nur mit Djangos eigenem django.views.static.serve.
    """

    default_auto_field = "django.db.models.BigAutoField"
    name = "web"

    def ready(self):
        # .wasm ist in der mimetypes-Datenbank je nach OS/Python-Version
        # nicht als application/wasm hinterlegt -- ohne das lehnen Browser
        # teils ab, das Modul zu instanziieren. Bisher setzte das der
        # separate nginx-Container explizit (siehe docker/web/default.conf,
        # "location ~ \.wasm$ { default_type application/wasm; }");
        # dasselbe jetzt hier fuer Djangos eigene Auslieferung.
        mimetypes.add_type("application/wasm", ".wasm")
