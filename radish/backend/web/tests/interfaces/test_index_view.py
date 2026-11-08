from web.tests.helpers import INDEX_HTML, ServingTestCase, TestResponse


class IndexViewTests(ServingTestCase):
    def test_root_serves_index_html(self) -> None:
        response: TestResponse = self.client.get("/")

        self.assertEqual(response.status_code, 200)
        self.assertEqual(self.content(response), INDEX_HTML)

    def test_index_html_is_revalidated_on_every_use(self) -> None:
        response: TestResponse = self.client.get("/")

        self.assertIn("no-cache", response["Cache-Control"])
