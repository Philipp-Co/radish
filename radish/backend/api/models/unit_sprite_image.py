from django.db import models

from api.models.png_file import PngFile
from api.models.unit_sprite import UnitSprite


class UnitSpriteImage(PngFile):
    """
    Eine PNG eines hochgeladenen Einheitenbildes (UnitSprite), in der Datenbank
    statt im Dateisystem: so laeuft sie mit Backup und Seed mit, und es braucht
    kein Volume. "name" ist ihr Dateiname im Ordner des Bildes, wie ihn die
    sprite.json nennt; Breite und Hoehe stehen im Kopf der PNG
    (api/domain/sprites/png.py) und werden beim Hochladen gelesen.

    Ausgeliefert wird sie mit dem Anfang von "sha256" im Namen
    (web/interfaces/sprite_views.py) -- eine neue Fassung bekommt einen neuen
    Namen, und der Browser darf jede dauerhaft cachen.
    """

    sprite = models.ForeignKey(UnitSprite, on_delete=models.CASCADE, related_name="images")

    class Meta(PngFile.Meta):
        ordering = ["sprite", "name"]
        constraints = [
            models.UniqueConstraint(fields=["sprite", "name"], name="unique_image_name_per_sprite"),
        ]

    def __str__(self) -> str:
        return f"{self.sprite.key}/{self.name}"
