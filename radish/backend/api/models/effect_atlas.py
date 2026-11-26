from django.core.validators import MaxValueValidator, MinValueValidator
from django.db import models

from api.models.effect import Effect
from api.models.effect_image import EffectImage
from api.models.limits import EFFECT_FIRST_FRAME_MAX, SPRITE_FRAME_SIZE_MAX, SPRITE_FRAMES_MAX
from api.models.unit_sprite_atlas import SpriteFacing


def _anchor() -> models.FloatField:
    """Ein Anteil an Breite oder Hoehe einer Zelle."""
    return models.FloatField(default=0.5, validators=[MinValueValidator(0.0), MaxValueValidator(1.0)])


class EffectAtlas(models.Model):
    """
    Die Folge eines Effekts in einer Richtung: welche PNG (EffectImage) sie zeigt
    und wie die aufgeteilt ist -- zeilenweise von links oben, Zellen von
    frame_width x frame_height, ab Zelle "first_frame" fuer "frame_count" Bilder.
    So koennen sich Richtungen eine PNG teilen, hintereinander oder, wenn der
    Effekt in alle Richtungen gleich aussieht, mit derselben Folge.

    Die Richtung ist die auf dem Bildschirm wie in der sprite.json (sw = -x,
    se = +y, ne = +x, nw = -y); bei einer Explosion die, in die der Schuss
    fliegt. Der Anker (anchor_x, anchor_y) ist der Einschlag in der Zelle, als
    Anteil an ihrer Breite und Hoehe.

    Ob die Angaben zur PNG passen und alle vier Richtungen da sind, prueft
    api/domain/effects/problems.py -- ein Effekt mit Fehlern bleibt ein Entwurf.
    """

    effect = models.ForeignKey(Effect, on_delete=models.CASCADE, related_name="atlases")
    facing = models.CharField(max_length=2, choices=[(f.value, f.label) for f in SpriteFacing if f.value])
    # PROTECT: eine PNG, die ein Atlas zeigt, laesst sich nicht loeschen.
    image = models.ForeignKey(EffectImage, on_delete=models.PROTECT, related_name="atlases")
    frame_width = models.PositiveSmallIntegerField(
        validators=[MinValueValidator(1), MaxValueValidator(SPRITE_FRAME_SIZE_MAX)]
    )
    frame_height = models.PositiveSmallIntegerField(
        validators=[MinValueValidator(1), MaxValueValidator(SPRITE_FRAME_SIZE_MAX)]
    )
    first_frame = models.PositiveSmallIntegerField(default=0, validators=[MaxValueValidator(EFFECT_FIRST_FRAME_MAX)])
    frame_count = models.PositiveSmallIntegerField(
        validators=[MinValueValidator(1), MaxValueValidator(SPRITE_FRAMES_MAX)]
    )
    anchor_x = _anchor()
    anchor_y = _anchor()

    class Meta:
        ordering = ["effect", "facing"]
        constraints = [
            models.UniqueConstraint(fields=["effect", "facing"], name="unique_atlas_per_effect_facing"),
        ]

    def __str__(self) -> str:
        return f"{self.effect.key} {self.facing}"
