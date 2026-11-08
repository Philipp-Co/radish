from web.tests.helpers import BUNDLE_JS, HASHED_BUNDLE, INDEX_HTML, ServingTestCase, TestResponse


class StaticFileViewTests(ServingTestCase):
    def test_bundle_is_served_under_its_own_name(self) -> None:
        response: TestResponse = self.client.get(f"/{HASHED_BUNDLE}")

        self.assertEqual(response.status_code, 200)
        self.assertEqual(self.content(response), BUNDLE_JS)

    def test_unknown_path_falls_back_to_index_html(self) -> None:
        # SPA-Fallback: Angular routet clientseitig, "/games" gibt es nur dort.
        response: TestResponse = self.client.get("/games")

        self.assertEqual(response.status_code, 200)
        self.assertEqual(self.content(response), INDEX_HTML)

    def test_directory_traversal_is_rejected(self) -> None:
        response: TestResponse = self.client.get("/../backend/manage.py")

        # Django weist ".."-Segmente teils schon vor der View als
        # SuspiciousOperation ab (400), teils schlaegt erst safe_join in
        # django.views.static.serve zu (404) -- je nach genauem Pfad und
        # Django-Version. Entscheidend ist nur: kein 200, die Datei
        # ausserhalb des Frontend-Verzeichnisses wird nicht ausgeliefert.
        self.assertIn(response.status_code, (400, 404))
