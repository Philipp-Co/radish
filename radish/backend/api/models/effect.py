from django.core.validators import MaxValueValidator, MinValueValidator, RegexValidator
from django.db import models

from api.models.limits import MAX_SPRITE_KEY_LENGTH, SPRITE_DRAW_WIDTH_MAX, SPRITE_DURATION_MS_MAX, SPRITE_KEY_PATTERN


class EffectKind(models.TextChoices):
    """
    Wofuer der Client einen Effekt nimmt. Bei einem Angriff waehlt er eine der
    Explosionen zufaellig (RAD_IsoEffectRandomExplosion) -- welche, sagt das
    Spiel nicht.
    """

    EXPLOSION = "explosion", "Explosion"


class Effect(models.Model):
    """
    Ein Effekt im Spiel, etwa eine Explosion: was im Client in
    assets/effects/<key>/effect.json steht (radish/client/schema/
    effect.schema.json), hier als Felder statt als Datei, dazu seine PNGs
    (EffectImage) und je Richtung ein Atlas (EffectAtlas). Die effect.json
    erzeugt das Backend daraus (api/domain/effects/document.py) -- wie bei den
    Einheitenbildern (UnitSprite).

    Der Schluessel ist der Ordner im Client (assets/effects/<key>/) und aendert
    sich nach dem Anlegen nicht mehr. Kein Katalogeintrag nennt ihn: der Client
    findet jeden Effekt im Manifest selbst.

    Im Spiel erscheint er erst, wenn er vollstaendig ist -- alle vier Atlanten da
    und gueltig (api/domain/effects/problems.py); bis dahin ist er ein Entwurf.
    """

    key = models.CharField(
        max_length=MAX_SPRITE_KEY_LENGTH,
        unique=True,
        validators=[RegexValidator(SPRITE_KEY_PATTERN, "Nur Kleinbuchstaben, Ziffern und _, hoechstens 31 Zeichen.")],
    )
    kind = models.CharField(max_length=16, choices=EffectKind.choices, default=EffectKind.EXPLOSION)
    # Breite einer Zelle auf dem Bildschirm in Pixeln; die Hoehe folgt im
    # Seitenverhaeltnis der Zelle.
    draw_width = models.PositiveSmallIntegerField(
        default=256, validators=[MinValueValidator(1), MaxValueValidator(SPRITE_DRAW_WIDTH_MAX)]
    )
    # Wie lange der Effekt einmal durchlaeuft. Bei einer Explosion bleibt der
    # Turm so lange auf dem Ziel.
    duration_ms = models.FloatField(
        default=2500.0, validators=[MinValueValidator(0.001), MaxValueValidator(SPRITE_DURATION_MS_MAX)]
    )
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        ordering = ["key"]

    def __str__(self) -> str:
        return self.key
