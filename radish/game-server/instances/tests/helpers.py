"""Gemeinsame Testdaten und Typen fuer die Tests unter instances/tests/."""

from typing import TYPE_CHECKING

from instances.domain.game_files import GameStartFile

# Was der DRF-Testclient zurueckgibt. Den Typ gibt es nur in den Stubs, zur
# Laufzeit werden Annotationen lokaler Variablen nicht ausgewertet.
if TYPE_CHECKING:
    from rest_framework.response import _MonkeyPatchedResponse as TestResponse
else:
    from rest_framework.response import Response

    TestResponse = Response

# Gekuerzt -- vollstaendig siehe radish/game/schema/spielstart.schema.json.
SPIELDATEN: GameStartFile = {
    "spieldaten": [
        {"name": "test-user", "kennung": "aB3xK9pQ", "armee": {"name": "A"}},
        {"name": "test-user-2", "kennung": "Zz1Yy2Xx", "armee": {"name": "B"}},
    ],
    "gelaende": [
        {"typ": "forest", "bewegung": -1, "deckung": 2, "unbegehbar": False},
    ],
}

# Was das Backend schickt: die Spieldaten und daneben die geheimen Codes.
CODES: list[str] = ["1122334455667788", "99aabbccddeeff00"]
START: dict[str, object] = {**SPIELDATEN, "zugangscodes": CODES}
