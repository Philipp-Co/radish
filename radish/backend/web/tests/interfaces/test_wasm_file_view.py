from web.tests.helpers import CLIENT_JS, CLIENT_WASM, ServingTestCase, TestResponse


class WasmFileViewTests(ServingTestCase):
    def test_client_js_is_served_under_wasm(self) -> None:
        response: TestResponse = self.client.get("/wasm/client.js")

        self.assertEqual(response.status_code, 200)
        self.assertEqual(self.content(response), CLIENT_JS)

    def test_client_wasm_is_served_with_wasm_mime_type(self) -> None:
        response: TestResponse = self.client.get("/wasm/client.wasm")

        self.assertEqual(response.status_code, 200)
        self.assertEqual(response["Content-Type"], "application/wasm")
        self.assertEqual(self.content(response), CLIENT_WASM)

    def test_unknown_wasm_file_returns_404(self) -> None:
        # Anders als unter "/" gibt es hier keinen Fallback auf index.html.
        response: TestResponse = self.client.get("/wasm/does-not-exist.js")

        self.assertEqual(response.status_code, 404)
