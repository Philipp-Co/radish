from django.core.validators import MaxValueValidator, MinValueValidator, RegexValidator
from django.db import models

from api.models.limits import (
    MAX_SPRITE_KEY_LENGTH,
    SPRITE_DRAW_WIDTH_MAX,
    SPRITE_DURATION_MS_MAX,
    SPRITE_KEY_PATTERN,
)


def _duration() -> models.FloatField:
    """Eine Dauer der sprite.json in ms; leer heisst: der Standard des Clients."""
    return models.FloatField(
        null=True,
        blank=True,
        validators=[MinValueValidator(0.001), MaxValueValidator(SPRITE_DURATION_MS_MAX)],
    )


class UnitSprite(models.Model):
    """
    Ein hochgeladenes Einheitenbild: was im Client in
    assets/units/<key>/sprite.json steht (radish/client/schema/sprite.schema.json),
    hier als Felder statt als Datei, dazu seine PNGs (UnitSpriteImage) und je
    Animation und Blickrichtung ein Atlas (UnitSpriteAtlas). Die sprite.json
    erzeugt das Backend daraus (api/domain/sprites/document.py).

    Ein Einheitentyp nennt es ueber "key" (UnitType.sprite) -- ein String und
    kein Fremdschluessel, weil derselbe Schluessel auch ein Bild aus dem Build
    nennen kann (radish/assets/units/). Der Schluessel aendert sich deshalb nach
    dem Anlegen nicht mehr.

    Im Spiel erscheint es erst, wenn es vollstaendig ist -- alle Atlanten da und
    gueltig (api/domain/sprites/problems.py); bis dahin ist es ein Entwurf.
    """

    key = models.CharField(
        max_length=MAX_SPRITE_KEY_LENGTH,
        unique=True,
        validators=[RegexValidator(SPRITE_KEY_PATTERN, "Nur Kleinbuchstaben, Ziffern und _, hoechstens 31 Zeichen.")],
    )
    # Wo die Figur in ihrer Zelle auf dem Boden steht, als Anteil an Breite und
    # Hoehe der Zelle.
    anchor_x = models.FloatField(default=0.5, validators=[MinValueValidator(0.0), MaxValueValidator(1.0)])
    anchor_y = models.FloatField(default=0.8, validators=[MinValueValidator(0.0), MaxValueValidator(1.0)])
    # Breite einer Zelle auf dem Bildschirm in Pixeln.
    draw_width = models.PositiveSmallIntegerField(
        default=128, validators=[MinValueValidator(1), MaxValueValidator(SPRITE_DRAW_WIDTH_MAX)]
    )
    # Ob der Schatten schon in den Bildern steckt.
    own_shadow = models.BooleanField(default=False)
    step_ms = _duration()
    turn_ms = _duration()
    aim_ms = _duration()
    aim_hold_ms = _duration()
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        ordering = ["key"]

    def __str__(self) -> str:
        return self.key
