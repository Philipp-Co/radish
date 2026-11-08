"""
Gemeinsame Grundlage der Tests unter web/tests/: Frontend- und Wasm-Verzeichnis
zeigen auf temporaere Verzeichnisse mit eigenen Testdateien -- die Tests
brauchen also weder einen Angular- noch einen Wasm-Build.
"""

import gzip
from pathlib import Path
import tempfile
from typing import TYPE_CHECKING
from unittest import mock

from django.http import StreamingHttpResponse
from django.test import SimpleTestCase

# Was der Django-Testclient zurueckgibt. Den Typ gibt es nur in den Stubs;
# Annotationen lokaler Variablen werden zur Laufzeit nie ausgewertet.
if TYPE_CHECKING:
    from django.test.client import _MonkeyPatchedWSGIResponse as TestResponse
else:
    from django.http import HttpResponse

    TestResponse = HttpResponse

INDEX_HTML: bytes = b"<!doctype html><html><body><app-root></app-root></body></html>"
# Ein Angular-Bundle mit Inhalts-Hash im Namen (siehe serving._HASHED_FILE),
# dazu seine vorkomprimierte Fassung wie im Docker-Image.
HASHED_BUNDLE: str = "main-ABCD1234.js"
BUNDLE_JS: bytes = b"console.log('bundle');"
CLIENT_JS: bytes = b"console.log('client');"
CLIENT_WASM: bytes = b"\x00asm\x01\x00\x00\x00"


class ServingTestCase(SimpleTestCase):
    """Legt FRONTEND_DIST_DIR und WASM_DIR fuer jeden Test neu an."""

    frontend_dir: Path
    wasm_dir: Path

    def setUp(self) -> None:
        # pylint kennt enterContext nicht: der Kontext wird dort geschlossen.
        # pylint: disable-next=consider-using-with
        self.frontend_dir = Path(self.enterContext(tempfile.TemporaryDirectory()))
        # pylint: disable-next=consider-using-with
        self.wasm_dir = Path(self.enterContext(tempfile.TemporaryDirectory()))
        self.enterContext(mock.patch("web.interfaces.serving.FRONTEND_DIST_DIR", self.frontend_dir))
        self.enterContext(mock.patch("web.interfaces.serving.WASM_DIR", self.wasm_dir))

        (self.frontend_dir / "index.html").write_bytes(INDEX_HTML)
        (self.frontend_dir / HASHED_BUNDLE).write_bytes(BUNDLE_JS)
        (self.frontend_dir / f"{HASHED_BUNDLE}.gz").write_bytes(gzip.compress(BUNDLE_JS))
        (self.wasm_dir / "client.js").write_bytes(CLIENT_JS)
        (self.wasm_dir / "client.wasm").write_bytes(CLIENT_WASM)

    def content(self, response: TestResponse) -> bytes:
        """Der ausgelieferte Inhalt; serve() antwortet mit einer FileResponse."""
        self.assertIsInstance(response, StreamingHttpResponse)
        # Fuer mypy: erst eine StreamingHttpResponse hat streaming_content.
        assert isinstance(response, StreamingHttpResponse)
        return b"".join(response.streaming_content)
