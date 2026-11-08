from django.test import RequestFactory, SimpleTestCase

from web.interfaces.serving import _accepts_gzip


class AcceptsGzipTests(SimpleTestCase):
    def test_accept_encoding_decides(self) -> None:
        header: str
        expected: bool
        for header, expected in (
            ("", False),
            ("br, deflate", False),
            ("gzip", True),
            ("deflate, GZIP", True),
            ("gzip;q=0.5", True),
            ("gzip;q=0", False),
            ("gzip;q=kaputt", False),
            ("gzip;q=0, gzip", False),  # der erste gzip-Eintrag entscheidet
        ):
            with self.subTest(header=header):
                self.assertEqual(
                    _accepts_gzip(RequestFactory().get("/", HTTP_ACCEPT_ENCODING=header)), expected
                )
