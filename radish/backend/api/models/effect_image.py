from django.db import models

from api.models.effect import Effect
from api.models.png_file import PngFile


class EffectImage(PngFile):
    """
    Eine PNG eines Effekts (Effect), in der Datenbank statt im Dateisystem --
    wie UnitSpriteImage bei den Einheitenbildern: so laeuft sie mit Backup und
    Seed mit. "name" ist ihr Dateiname im Ordner des Effekts im Client
    (assets/effects/<key>/), wie ihn die effect.json nennt. Ausgeliefert wird
    sie mit dem Anfang von "sha256" im Namen (api/domain/effects/publishing.py).
    """

    effect = models.ForeignKey(Effect, on_delete=models.CASCADE, related_name="images")

    class Meta(PngFile.Meta):
        ordering = ["effect", "name"]
        constraints = [
            models.UniqueConstraint(fields=["effect", "name"], name="unique_image_name_per_effect"),
        ]

    def __str__(self) -> str:
        return f"{self.effect.key}/{self.name}"
