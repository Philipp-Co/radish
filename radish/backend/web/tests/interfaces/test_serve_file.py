import gzip

from web.tests.helpers import BUNDLE_JS, CLIENT_JS, HASHED_BUNDLE, ServingTestCase, TestResponse


class ServeFileTests(ServingTestCase):
    def test_precompressed_file_is_preferred_when_gzip_is_accepted(self) -> None:
        response: TestResponse = self.client.get(f"/{HASHED_BUNDLE}", HTTP_ACCEPT_ENCODING="gzip")

        self.assertEqual(response["Content-Encoding"], "gzip")
        self.assertEqual(gzip.decompress(self.content(response)), BUNDLE_JS)
        self.assertIn("Accept-Encoding", response["Vary"])

    def test_original_is_served_without_gzip(self) -> None:
        response: TestResponse = self.client.get(f"/{HASHED_BUNDLE}")

        self.assertNotIn("Content-Encoding", response)
        self.assertEqual(self.content(response), BUNDLE_JS)
        self.assertIn("Accept-Encoding", response["Vary"])

    def test_original_is_served_when_there_is_no_gz_file(self) -> None:
        response: TestResponse = self.client.get("/wasm/client.js", HTTP_ACCEPT_ENCODING="gzip")

        self.assertNotIn("Content-Encoding", response)
        self.assertEqual(self.content(response), CLIENT_JS)

    def test_hashed_file_is_cached_for_a_year(self) -> None:
        response: TestResponse = self.client.get(f"/{HASHED_BUNDLE}")

        cache_control: str = response["Cache-Control"]
        self.assertIn("max-age=31536000", cache_control)
        self.assertIn("immutable", cache_control)
        self.assertIn("public", cache_control)

    def test_file_with_fixed_name_is_revalidated(self) -> None:
        response: TestResponse = self.client.get("/wasm/client.js")

        self.assertIn("no-cache", response["Cache-Control"])
