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

# Schluessel des Bildes eines Einheitentyps (UnitType.sprite): der Name seines
# Ordners unter radish/assets/units/, in dem sprite.json liegt. Nur
# Kleinbuchstaben, Ziffern und "_", damit er ohne Pruefung in einen Pfad passt;
# hoechstens 31 Zeichen, denn das Spiel legt ihn wie einen Namen in 32 Bytes
# samt Null ab (RAD_UNIT_SPRITE_KEY_MAX). Leer heisst: kein Bild, die Einheit
# wird als Rechteck gezeichnet.
MAX_SPRITE_KEY_LENGTH = 31
SPRITE_KEY_PATTERN = r"^[a-z0-9_]{1,31}$"

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
