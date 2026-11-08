"""
Zugangscodes der Spieler: wann ein Paar Codes gueltig ist. Woher sie kommen und
wozu sie dienen, steht in game_files.py.
"""

import re
from typing import TypeGuard


# Ein Zugangscode: eine 64-Bit-Zahl als hoechstens 16 Hexziffern, so wie
# zucchini_admin_client ihn erwartet. 0 ist in der Whitelist ein freier Platz.
_ACCESS_CODE = re.compile(r"^[0-9a-fA-F]{1,16}$")


def _is_access_code(code: object) -> bool:
    """Ein Code: 1-16 Hexziffern."""
    return isinstance(code, str) and _ACCESS_CODE.match(code) is not None


def _distinct_and_nonzero(codes: list[str]) -> bool:
    """Keiner ist 0 (ein freier Platz in der Whitelist), und beide sind verschieden."""
    values: list[int] = [int(code, 16) for code in codes]
    return 0 not in values and values[0] != values[1]


def valid_access_codes(codes: object) -> TypeGuard[list[str]]:
    """Genau zwei verschiedene Codes, je 1-16 Hexziffern und nicht 0."""
    return (
        isinstance(codes, list)
        and len(codes) == 2
        and all(_is_access_code(code) for code in codes)
        and _distinct_and_nonzero(codes)
    )
