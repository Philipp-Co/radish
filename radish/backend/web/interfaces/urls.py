from django.urls import path

from web.interfaces.index_view import index
from web.interfaces.sprite_views import asset_manifest, uploaded_asset
from web.interfaces.static_file_view import static_file
from web.interfaces.wasm_file_view import wasm_file

app_name = "web"

urlpatterns = [
    path("", index, name="index"),
    # Vor "wasm/<path>": das Manifest ergaenzt um die hochgeladenen
    # Einheitenbilder, und deren Dateien aus der Datenbank (interfaces/sprite_views.py).
    path("wasm/assets/manifest.json", asset_manifest, name="asset-manifest"),
    path("wasm/assets/uploaded/<path:name>", uploaded_asset, name="uploaded-asset"),
    path("wasm/<path:filename>", wasm_file, name="wasm-file"),
    # Zuletzt eingehaengt: faengt alles ausser "wasm/..." (siehe oben) ab,
    # inklusive eines SPA-Fallbacks auf index.html (siehe interfaces/static_file_view.py).
    path("<path:filename>", static_file, name="static-file"),
]
