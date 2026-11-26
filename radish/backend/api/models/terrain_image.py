from django.db import models

from api.models.png_file import PngFile
from api.models.terrain_type import TerrainType


class TerrainImage(PngFile):
    """
    Eine PNG eines Tile-Typs (TerrainType), in der Datenbank statt im
    Dateisystem -- wie UnitSpriteImage bei den Einheitenbildern: so laeuft sie
    mit Backup und Seed mit. "name" ist ihr Dateiname im Ordner des Typs im
    Client (assets/tiles/<code>/), wie ihn die tile.json nennt; Breite und Hoehe
    stehen im Kopf der PNG (api/domain/sprites/png.py).

    Was der Typ damit zeigt: hoechstens eine ist sein Grundbild, das Bild des
    Feldes statt des Wuerfels ("is_base"); als Dekoration nennt sie
    TerrainDecor. Der Verweis geht bewusst von der PNG aus und nicht vom Typ --
    so verweisen Typ und PNG nicht im Kreis aufeinander, und PNGs und
    Dekorationen lassen sich in eine gepflegte Datenbank nachliefern
    (manage.py seed --table). Ausgeliefert wird sie mit dem Anfang von
    "sha256" im Namen (api/domain/tiles/publishing.py).
    """

    terrain = models.ForeignKey(TerrainType, on_delete=models.CASCADE, related_name="images")
    is_base = models.BooleanField(default=False)

    class Meta(PngFile.Meta):
        ordering = ["terrain", "name"]
        constraints = [
            models.UniqueConstraint(fields=["terrain", "name"], name="unique_image_name_per_terrain"),
            models.UniqueConstraint(
                fields=["terrain"], condition=models.Q(is_base=True), name="one_base_image_per_terrain"
            ),
        ]

    def __str__(self) -> str:
        return f"{self.terrain.code}/{self.name}"
