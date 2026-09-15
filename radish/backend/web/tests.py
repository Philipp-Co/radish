"""
Tests fuer web/ -- serviert index.html, client.js und client.wasm aus
radish/web/ (siehe views.py). Kein Model, keine Auth involviert, deshalb
reicht der einfache Django-TestCase-Client.
"""

from django.test import SimpleTestCase


class FrontendServingTests(SimpleTestCase):
    def test_root_serves_index_html(self):
        response = self.client.get("/")

        self.assertEqual(response.status_code, 200)
        self.assertIn(b"<canvas", b"".join(response.streaming_content))

    def test_client_js_is_served_under_its_own_name(self):
        response = self.client.get("/client.js")

        self.assertEqual(response.status_code, 200)

    def test_client_wasm_is_served_with_wasm_mime_type(self):
        response = self.client.get("/client.wasm")

        self.assertEqual(response.status_code, 200)
        self.assertEqual(response["Content-Type"], "application/wasm")

    def test_unknown_file_returns_404(self):
        response = self.client.get("/does-not-exist.txt")

        self.assertEqual(response.status_code, 404)

    def test_directory_traversal_is_rejected(self):
        response = self.client.get("/../backend/manage.py")

        # Django weist ".."-Segmente teils schon vor der View als
        # SuspiciousOperation ab (400), teils schlaegt erst safe_join in
        # django.views.static.serve zu (404) -- je nach genauem Pfad und
        # Django-Version. Entscheidend ist nur: kein 200, die Datei
        # ausserhalb von FRONTEND_DIR wird nicht ausgeliefert.
        self.assertIn(response.status_code, (400, 404))
