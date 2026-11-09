from django.core.validators import MaxValueValidator, MinValueValidator, RegexValidator
from django.db import models

from api.models.limits import (
    MAX_NAME_BYTES,
    MAX_SPRITE_KEY_LENGTH,
    MAX_UNIT_ENTITIES,
    SPRITE_KEY_PATTERN,
    MaxUtf8BytesValidator,
)
from api.models.species import Species

# Obergrenze der Sichtweite: im Spiel ein Byte (RAD_Unit_t.sight_range), und
# mehr nimmt der Spielserver beim Start nicht an (spielstart.schema.json).
MAX_SIGHT_RANGE = 255


class UnitType(models.Model):
    """
    Ein Einheitentyp im Katalog einer Spezies. Die Werte der einzelnen
    Entitaeten stehen nicht hier, sondern in den EntityProfile-Eintraegen
    darunter -- eine Einheit darf Entitaeten mit abweichenden Profilen
    enthalten (etwa einen Anfuehrer neben seinen Soldaten).
    """

    species = models.ForeignKey(
        Species, on_delete=models.PROTECT, related_name="unit_types"
    )
    # Geht ins Spiel; Grenze in Bytes, siehe limits.py.
    name = models.CharField(
        max_length=MAX_NAME_BYTES, validators=[MaxUtf8BytesValidator(MAX_NAME_BYTES)]
    )
    min_entities = models.PositiveSmallIntegerField(
        default=1, validators=[MinValueValidator(1), MaxValueValidator(MAX_UNIT_ENTITIES)]
    )
    max_entities = models.PositiveSmallIntegerField(
        default=MAX_UNIT_ENTITIES,
        validators=[MinValueValidator(1), MaxValueValidator(MAX_UNIT_ENTITIES)],
    )
    # Wie viele schwere bzw. super-schwere Waffen die Einheit hoechstens
    # gleichzeitig fuehren darf, ueber alle Entitaeten zusammen -- zusaetzlich
    # zu den Slots der Profile. None = keine Grenze auf Einheitenebene.
    max_heavy_weapons = models.PositiveSmallIntegerField(null=True, blank=True)
    max_super_heavy_weapons = models.PositiveSmallIntegerField(null=True, blank=True)
    # Wie viele andere Einheiten eine Einheit dieses Typs transportieren
    # kann (0 = kein Transporter).
    transport_capacity = models.PositiveSmallIntegerField(default=0)
    # Ob Einheiten dieses Typs Ziele einnehmen koennen.
    can_capture_objectives = models.BooleanField(default=False)
    # Bewegungsradius: wie viele Felder weit sich die Einheit bewegen kann.
    # Wie die Werte der Profile ohne Default -- der Admin traegt ihn ein.
    movement = models.PositiveSmallIntegerField()
    # Sichtweite: wie viele Felder weit die Einheit sieht, waagerecht plus
    # senkrecht; 0 = nur ihr eigenes Feld. Wie movement ohne Default.
    sight_range = models.PositiveSmallIntegerField(validators=[MaxValueValidator(MAX_SIGHT_RANGE)])
    # Welches Bild das Spiel fuer Einheiten dieses Typs zeichnet: der Ordner
    # unter radish/assets/units/ (siehe limits.py). Leer, solange es keins
    # gibt -- dann zeichnet der Client ein Rechteck. Mehrere Typen duerfen
    # dasselbe Bild nennen, und der Name des Typs kann sich aendern, ohne
    # dass das Bild verloren geht.
    sprite = models.CharField(
        max_length=MAX_SPRITE_KEY_LENGTH,
        blank=True,
        default="",
        validators=[
            RegexValidator(
                SPRITE_KEY_PATTERN,
                "Nur Kleinbuchstaben, Ziffern und _, hoechstens 31 Zeichen.",
            )
        ],
    )

    class Meta:
        ordering = ["species", "name"]
        constraints = [
            models.UniqueConstraint(
                fields=["species", "name"], name="unique_unit_type_per_species"
            ),
        ]

    def __str__(self) -> str:
        return f"{self.name} ({self.species})"
