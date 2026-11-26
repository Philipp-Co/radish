# Grenzen, die das Spiel setzt. Was hier steht, muss zu radish/game/include/
# radish/game/model/unit/unit.h und zu game/schema/spielstart.schema.json
# passen -- ueberschreitet eine Armee sie, lehnt die Spielinstanz den Start ab
# (control/start_game.c), und das soll schon beim Anlegen im Katalog bzw.
# beim Zusammenstellen der Armee auffallen.

from django.core.exceptions import ValidationError
from django.utils.deconstruct import deconstructible

# Obergrenze fuer die Entitaeten einer Einheit, unabhaengig vom Einheitentyp
# (RAD_UNIT_MAX_MEMBERS).
MAX_UNIT_ENTITIES = 10

# Laenge eines Namens, der ins Spiel geht -- Einheitentyp, Profil, Waffe --,
# in **Bytes** (UTF-8): das Spiel legt ihn in ein festes Feld von
# RAD_UNIT_NAME_MAX = 32 Bytes samt abschliessender Null. Ein Umlaut belegt
# zwei Bytes; deshalb reicht max_length (zaehlt Zeichen) allein nicht, dazu
# kommt MaxUtf8BytesValidator.
MAX_NAME_BYTES = 31

# Schluessel des Bildes eines Einheitentyps (UnitType.sprite, UnitSprite.key):
# im Client der Ordner unter assets/units/, in dem sprite.json liegt. Nur
# Kleinbuchstaben, Ziffern und "_", damit er ohne Pruefung in einen Pfad passt;
# hoechstens 31 Zeichen, denn das Spiel legt ihn wie einen Namen in 32 Bytes
# samt Null ab (RAD_UNIT_SPRITE_KEY_MAX). Leer heisst: kein Bild, die Einheit
# wird als Rechteck gezeichnet.
MAX_SPRITE_KEY_LENGTH = 31
SPRITE_KEY_PATTERN = r"^[a-z0-9_]{1,31}$"

# Grenzen einer sprite.json, wie der Client sie liest
# (radish/client/src/include/radish/rendering/iso_unit_sprite.h,
# radish/client/schema/sprite.schema.json): Zellgroesse, Zahl der Bilder je
# Atlas, Zeichenbreite, Bilder je Sekunde und Dauern. Ein hochgeladenes Bild
# (UnitSprite) muss in sie passen, sonst laedt der Client es nicht.
SPRITE_FRAME_SIZE_MAX = 1024
SPRITE_FRAMES_MAX = 1024
SPRITE_DRAW_WIDTH_MAX = 1024
SPRITE_FPS_MAX = 60.0
SPRITE_DURATION_MS_MAX = 10000.0

# Dateiname eines Atlas im Ordner des Bildes: hoechstens 27 Zeichen aus
# [a-z0-9_] und ".png", zusammen 31 (RAD_ISO_UNIT_FILE_NAME_MAX samt Null).
SPRITE_FILE_NAME_PATTERN = r"^[a-z0-9_]{1,27}\.png$"

# Wie gross eine hochgeladene PNG hoechstens sein darf. Ein voller Atlas von
# 4096 x 4096 Pixeln hat rund 3 MB; das laesst Luft, haelt die Datenbank aber
# von Unfaellen frei.
SPRITE_IMAGE_BYTES_MAX = 16 * 1024 * 1024

# Grenzen einer tile.json (radish/client/src/include/radish/view/rendering/
# iso_tile_sprite.h, radish/client/schema/tile.schema.json): wie viele
# Dekorationen ein Feld tragen kann (RAD_ISO_TILE_DECOR_MAX) und wie weit eine
# gegen ihr Feld verschoben sein darf, in jede Richtung
# (RAD_ISO_TILE_DECOR_OFFSET_MAX, eine Feldhoehe). Die PNGs der Tile-Bilder
# folgen denselben Regeln fuer Name und Groesse wie die der Einheitenbilder
# (SPRITE_FILE_NAME_PATTERN, SPRITE_IMAGE_BYTES_MAX).
TERRAIN_DECOR_MAX = 2
TERRAIN_DECOR_OFFSET_MAX = 128

# Grenzen einer effect.json (radish/client/src/include/radish/view/rendering/
# iso_effect_sprite.h, radish/client/schema/effect.schema.json): mit welcher
# Zelle die Folge einer Richtung in ihrem Atlas beginnt
# (RAD_ISO_EFFECT_FIRST_FRAME_MAX) -- mehrere Richtungen duerfen sich einen
# Atlas teilen, hintereinander. Zellgroesse, Zahl der Bilder, Zeichenbreite und
# Dauer folgen den Grenzen der sprite.json, Name und Groesse der PNGs denen der
# Einheitenbilder.
EFFECT_FIRST_FRAME_MAX = 4095

# Waffen einer Entitaet, gleich welcher Klasse (RAD_UNIT_MAX_WEAPONS). Die
# Slots eines Profils duerfen zusammen nicht mehr ergeben.
MAX_WEAPONS_PER_ENTITY = 4


@deconstructible
class MaxUtf8BytesValidator:
    """
    Weist einen Text ab, der in UTF-8 laenger als "limit" Bytes ist.
    deconstructible, weil er an Modellfeldern haengt und damit in die
    Migrationen geschrieben wird.
    """

    def __init__(self, limit: int) -> None:
        self.limit = limit

    def __call__(self, value: str) -> None:
        length: int = len(value.encode("utf-8"))
        if length > self.limit:
            raise ValidationError(
                f"Hoechstens {self.limit} Bytes (UTF-8), sind aber {length} -- "
                f"Umlaute und Sonderzeichen zaehlen doppelt.",
                code="max_utf8_bytes",
            )

    def __eq__(self, other: object) -> bool:
        return isinstance(other, MaxUtf8BytesValidator) and self.limit == other.limit

    def __hash__(self) -> int:
        return hash(self.limit)
