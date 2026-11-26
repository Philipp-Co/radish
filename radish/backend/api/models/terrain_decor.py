from django.core.validators import MaxValueValidator, MinValueValidator
from django.db import models

from api.models.limits import TERRAIN_DECOR_MAX, TERRAIN_DECOR_OFFSET_MAX
from api.models.terrain_image import TerrainImage
from api.models.terrain_type import TerrainType


def _offset() -> models.SmallIntegerField:
    """Ein Versatz in Pixeln auf dem Bildschirm, hoechstens eine Feldhoehe."""
    return models.SmallIntegerField(
        default=0,
        validators=[MinValueValidator(-TERRAIN_DECOR_OFFSET_MAX), MaxValueValidator(TERRAIN_DECOR_OFFSET_MAX)],
    )


class TerrainDecor(models.Model):
    """
    Was auf einem Feld eines Tile-Typs steht, ohne darauf zu laufen -- Gras,
    Baeume, Drachenzaehne (RAD_IsoDecor_t im Client). Hoechstens
    TERRAIN_DECOR_MAX je Typ, gezeichnet in der Reihenfolge von "layer".

    Das Bild steht im selben Rahmen wie das Feld, verschoben um offset_x nach
    rechts und offset_y nach unten (negativ: nach links, nach oben). "in_front":
    nach den Figuren gezeichnet, eine Einheit steht dann dahinter -- beim hohen
    Gras die vordere Haelfte.
    """

    terrain = models.ForeignKey(TerrainType, on_delete=models.CASCADE, related_name="decor")
    layer = models.PositiveSmallIntegerField(validators=[MaxValueValidator(TERRAIN_DECOR_MAX - 1)])
    # PROTECT: eine PNG, die eine Dekoration zeigt, laesst sich nicht loeschen.
    image = models.ForeignKey(TerrainImage, on_delete=models.PROTECT, related_name="decor")
    offset_x = _offset()
    offset_y = _offset()
    in_front = models.BooleanField(default=False)

    class Meta:
        ordering = ["terrain", "layer"]
        constraints = [
            models.UniqueConstraint(fields=["terrain", "layer"], name="unique_decor_layer_per_terrain"),
        ]

    def __str__(self) -> str:
        return f"{self.terrain.code} {self.layer}: {self.image.name}"
