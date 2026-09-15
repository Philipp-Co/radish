from django.urls import path

from . import views

app_name = "web"

urlpatterns = [
    path("", views.index, name="index"),
    path("wasm/<path:filename>", views.wasm_file, name="wasm-file"),
    # Zuletzt eingehaengt: faengt alles ausser "wasm/..." (siehe oben) ab,
    # inklusive eines SPA-Fallbacks auf index.html (siehe views.static_file).
    path("<path:filename>", views.static_file, name="static-file"),
]
