from django.core.validators import MaxValueValidator, MinValueValidator
from django.db import models

from api.models.limits import SPRITE_FPS_MAX, SPRITE_FRAME_SIZE_MAX, SPRITE_FRAMES_MAX
from api.models.unit_sprite import UnitSprite
from api.models.unit_sprite_image import UnitSpriteImage


class SpriteAnimation(models.TextChoices):
    """
    Was eine Figur kann -- jede bietet alle an (RAD_IsoUnitAnimation_t im
    Client). TURN und AIM sind Drehungen mit Schluesselbildern, die uebrigen
    Folgen mit Bildern je Sekunde.
    """

    TURN = "turn", "Drehen"
    AIM = "aim", "Zielen"
    MOVE = "move", "Ziehen"
    FIRE = "fire", "Schiessen"
    DESTROYED = "destroyed", "Zerstoert"


# Die Drehungen; die uebrigen Animationen sind Folgen.
ROTATIONS: frozenset[str] = frozenset({SpriteAnimation.TURN, SpriteAnimation.AIM})


class SpriteFacing(models.TextChoices):
    """
    Blickrichtung auf dem Bildschirm, wie in der sprite.json: sw = -x, se = +y,
    ne = +x, nw = -y. Die Drehung (TURN) hat nur einen Atlas fuer alle; ihre
    Richtung ist leer.
    """

    NONE = "", "alle"
    SW = "sw", "links unten"
    SE = "se", "rechts unten"
    NE = "ne", "rechts oben"
    NW = "nw", "links oben"


class UnitSpriteAtlas(models.Model):
    """
    Ein Atlas eines hochgeladenen Einheitenbildes: welche PNG (UnitSpriteImage)
    eine Animation in einer Blickrichtung zeigt und wie sie aufgeteilt ist --
    zeilenweise von links oben, Zellen von frame_width x frame_height. Eine
    Drehung nennt ihre vier Schluesselbilder ("key_frames"), eine Folge ihre
    Bilder je Sekunde ("fps"); das jeweils andere bleibt leer.

    Ob die Angaben zueinander und zur PNG passen, prueft
    api/domain/sprites/problems.py -- ein Bild mit Fehlern bleibt ein Entwurf.
    """

    sprite = models.ForeignKey(UnitSprite, on_delete=models.CASCADE, related_name="atlases")
    animation = models.CharField(max_length=16, choices=SpriteAnimation.choices)
    facing = models.CharField(max_length=2, choices=SpriteFacing.choices, blank=True, default="")
    # PROTECT: eine PNG, die ein Atlas zeigt, laesst sich nicht loeschen.
    image = models.ForeignKey(UnitSpriteImage, on_delete=models.PROTECT, related_name="atlases")
    frame_width = models.PositiveSmallIntegerField(
        validators=[MinValueValidator(1), MaxValueValidator(SPRITE_FRAME_SIZE_MAX)]
    )
    frame_height = models.PositiveSmallIntegerField(
        validators=[MinValueValidator(1), MaxValueValidator(SPRITE_FRAME_SIZE_MAX)]
    )
    frame_count = models.PositiveSmallIntegerField(
        validators=[MinValueValidator(1), MaxValueValidator(SPRITE_FRAMES_MAX)]
    )
    key_frames = models.JSONField(null=True, blank=True)
    fps = models.FloatField(
        null=True, blank=True, validators=[MinValueValidator(0.001), MaxValueValidator(SPRITE_FPS_MAX)]
    )

    class Meta:
        ordering = ["sprite", "animation", "facing"]
        constraints = [
            models.UniqueConstraint(
                fields=["sprite", "animation", "facing"], name="unique_atlas_per_sprite_animation_facing"
            ),
        ]

    def __str__(self) -> str:
        return f"{self.sprite.key} {self.animation} {self.facing or 'alle'}"
