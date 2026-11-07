import unittest
from unittest import mock


class AccessTests(unittest.TestCase):
    """access.py: Codes, Spieler-Id und wie eine Nachricht an den Spielserver aussieht."""

    def test_player_id_packs_the_identifier(self):
        from api.domain.servers import access

        # Dieselbe Zahl wie RAD_ControlUserIdFromIdentifier im Spielserver
        # (game-server-core/test/control/test_game_start.c).
        self.assertEqual(access.player_id("aB3xK9pQ"), 0x614233784B397051)
        with self.assertRaises(ValueError):
            access.player_id("kurz")

    def test_new_codes_are_hex_and_never_reserved(self):
        from api.domain.servers import access

        with mock.patch("api.domain.servers.access.secrets.randbits", side_effect=[0, 1, 0xABC]):
            self.assertEqual(access.new_zucchini_code(), "0000000000000abc")
        self.assertRegex(access.new_zucchini_code(), r"^[0-9a-f]{16}$")

    def test_frame_puts_code_and_sender_in_front(self):
        from api.domain.servers import access

        framed = access.frame("00000000000000ab", "aB3xK9pQ", b"\x0a\x02")

        self.assertEqual(framed[:8], bytes.fromhex("00000000000000ab"))
        self.assertEqual(framed[8:16], bytes.fromhex("614233784b397051"))
        self.assertEqual(framed[16:], b"\x0a\x02")
