"""Breite und Hoehe einer PNG aus ihrem Kopf -- ohne Bildbibliothek."""

import struct

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class InvalidPng(ValueError):
    """Die Daten sind keine PNG, wie sie der Client laden kann."""


def png_size(data: bytes) -> tuple[int, int]:
    """
    (Breite, Hoehe) einer PNG. Eine PNG beginnt mit ihrer Signatur und dem
    IHDR-Block, der Breite und Hoehe als vorzeichenlose 32-Bit-Zahlen traegt
    (PNG-Spezifikation, 11.2.2). Mehr wird nicht gelesen: ob der Rest heil ist,
    merkt erst, wer das Bild dekodiert.
    """
    if not _has_png_header(data):
        raise InvalidPng("Keine PNG-Datei.")
    width: int
    height: int
    width, height = struct.unpack(">II", data[16:24])
    if min(width, height) == 0:
        raise InvalidPng("Die PNG ist leer (0 Pixel).")
    return width, height


def _has_png_header(data: bytes) -> bool:
    """Signatur, dann der IHDR-Block mit Breite und Hoehe."""
    return len(data) >= 24 and data.startswith(PNG_SIGNATURE) and data[12:16] == b"IHDR"
