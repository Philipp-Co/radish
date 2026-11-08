import json
import signal
import subprocess
import tempfile
from pathlib import Path
from unittest import mock

from django.test import override_settings
from django.urls import reverse
from rest_framework import status
from rest_framework.test import APISimpleTestCase

from instances.tests.helpers import CODES, SPIELDATEN, START, TestResponse


class GameDataFileTests(APISimpleTestCase):
    """
    Start und Abbruch gegen ein echtes Verzeichnis (RADISH_GAME_DATA_DIR auf
    ein temporaeres umgebogen); nur das Signal selbst ist gemockt.
    """

    def setUp(self) -> None:
        # enterContext schliesst Temp-Verzeichnis, Settings und Patches beim
        # Aufraeumen des Tests wieder, in umgekehrter Reihenfolge.
        # pylint kennt enterContext nicht: der Kontext wird dort geschlossen.
        # pylint: disable-next=consider-using-with
        self.tmp_dir: str = self.enterContext(tempfile.TemporaryDirectory())
        self.enterContext(override_settings(RADISH_GAME_DATA_DIR=self.tmp_dir))

        self.directory: Path = Path(self.tmp_dir) / "game-instance-1"
        self.directory.mkdir()
        (self.directory / "radish_server.pid").write_text("4711\n")
        self.start_url: str = reverse("instances:game-start", args=["game-instance-1"])
        self.cancel_url: str = reverse("instances:game-cancel", args=["game-instance-1"])

        self.kill: mock.MagicMock = self.enterContext(mock.patch("instances.domain.game_files.os.kill"))

        # Der Admin-Client von zucchini: nur seine Aufrufe zaehlen. Nicht
        # self.run -- das ist die Methode, mit der unittest den Test ausfuehrt.
        self.admin_client: mock.MagicMock = self.enterContext(mock.patch("instances.domain.whitelist.subprocess.run"))

    def whitelist_calls(self) -> list[list[str]]:
        # Jeder Aufruf richtet sich an die Zucchini-Instanz mit dem Namen aus
        # der URL; zurueck kommt nur, was danach steht.
        calls: list[list[str]] = [call.args[0][1:] for call in self.admin_client.call_args_list]
        call: list[str]
        for call in calls:
            self.assertEqual(call[:2], ["-n", "game-instance-1"])
        return [call[2:] for call in calls]

    def test_start_writes_file_and_signals_server(self) -> None:
        response: TestResponse = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        written: object = json.loads((self.directory / "spielstart.json").read_text())
        self.assertEqual(written, SPIELDATEN)
        self.assertFalse((self.directory / "spielstart.json.tmp").exists())
        self.kill.assert_called_once_with(4711, signal.SIGUSR1)

    def test_start_whitelists_codes_and_keeps_them_out_of_the_file(self) -> None:
        self.client.post(self.start_url, START, format="json")

        self.assertEqual(self.whitelist_calls(), [["whitelist-add", CODES[0]], ["whitelist-add", CODES[1]]])
        self.assertEqual(json.loads((self.directory / "zugang.json").read_text()), CODES)
        game_start: str = (self.directory / "spielstart.json").read_text()
        code: str
        for code in CODES:
            self.assertNotIn(code, game_start)

    def test_restart_replaces_old_codes(self) -> None:
        (self.directory / "zugang.json").write_text(json.dumps(["aa", "bb"]))

        self.client.post(self.start_url, START, format="json")

        self.assertEqual(self.whitelist_calls(), [
            ["whitelist-remove", "aa"], ["whitelist-remove", "bb"],
            ["whitelist-add", CODES[0]], ["whitelist-add", CODES[1]],
        ])

    def test_failed_whitelist_is_server_error(self) -> None:
        self.admin_client.side_effect = subprocess.CalledProcessError(1, "zucchini_admin_client")

        response: TestResponse = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)
        self.assertFalse((self.directory / "spielstart.json").exists())
        self.kill.assert_not_called()
        # Gemerkt ist er trotzdem, damit er sich wieder austragen laesst.
        self.assertEqual(json.loads((self.directory / "zugang.json").read_text()), CODES)

    def test_cancel_revokes_codes(self) -> None:
        (self.directory / "zugang.json").write_text(json.dumps(CODES))

        self.client.post(self.cancel_url)

        self.assertEqual(self.whitelist_calls(), [["whitelist-remove", CODES[0]], ["whitelist-remove", CODES[1]]])
        self.assertFalse((self.directory / "zugang.json").exists())

    def test_start_replaces_previous_file(self) -> None:
        (self.directory / "spielstart.json").write_text("alt")

        self.client.post(self.start_url, START, format="json")

        self.assertEqual(json.loads((self.directory / "spielstart.json").read_text()), SPIELDATEN)

    def test_start_without_pid_file_fails(self) -> None:
        (self.directory / "radish_server.pid").unlink()

        response: TestResponse = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)
        self.kill.assert_not_called()

    def test_start_with_stale_pid_fails(self) -> None:
        self.kill.side_effect = ProcessLookupError

        response: TestResponse = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_start_with_unreadable_pid_fails(self) -> None:
        (self.directory / "radish_server.pid").write_text("kaputt")

        response: TestResponse = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_start_without_instance_directory_fails(self) -> None:
        url: str = reverse("instances:game-start", args=["game-instance-2"])

        response: TestResponse = self.client.post(url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_instance_name_cannot_leave_data_dir(self) -> None:
        url: str = reverse("instances:game-start", args=[".."])

        response: TestResponse = self.client.post(url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)
        self.assertFalse((Path(self.tmp_dir).parent / "spielstart.json").exists())

    def test_cancel_removes_file_and_signals_server(self) -> None:
        (self.directory / "spielstart.json").write_text("{}")

        response: TestResponse = self.client.post(self.cancel_url)

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        self.assertFalse((self.directory / "spielstart.json").exists())
        self.kill.assert_called_once_with(4711, signal.SIGUSR1)

    def test_cancel_without_running_server_is_only_logged(self) -> None:
        (self.directory / "radish_server.pid").unlink()

        with self.assertLogs("instances.domain.game_files", level="WARNING"):
            response: TestResponse = self.client.post(self.cancel_url)

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
