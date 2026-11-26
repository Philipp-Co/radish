from django.core.validators import RegexValidator
from django.db import models

from api.models.limits import SPRITE_FILE_NAME_PATTERN


class PngFile(models.Model):
    """
    Eine PNG in der Datenbank statt im Dateisystem, wie sie die Einheitenbilder
    (UnitSpriteImage) und die Tile-Bilder (TerrainImage) hochladen: "name" ist
    ihr Dateiname im Ordner des Bildes im Client, Breite und Hoehe stehen im
    Kopf der PNG (api/domain/sprites/png.py) und werden beim Hochladen gelesen.
    Ohne eigene Tabelle -- wem sie gehoert, sagt die Unterklasse.
    """

    name = models.CharField(
        max_length=31,
        validators=[RegexValidator(SPRITE_FILE_NAME_PATTERN, "Nur a-z, 0-9 und _, mit .png, hoechstens 31 Zeichen.")],
    )
    data = models.BinaryField()
    sha256 = models.CharField(max_length=64)
    width = models.PositiveIntegerField()
    height = models.PositiveIntegerField()

    class Meta:
        abstract = True
