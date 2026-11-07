from django.db import models


class TerrainCode(models.TextChoices):
    """
    Die Tile-Typen des Spiels (RAD_TileType_t in radish/game/include/radish/
    game/model/model.h), unter dem Namen, unter dem sie im Spielstart stehen
    (radish/game/schema/spielstart.schema.json, "gelaende"). Ein neuer Typ
    kommt im Spiel, im Schema und hier dazu -- und mit seiner Zeile in den
    Dump api/fixtures/seed.json (die Migrationen enthalten nur das Schema).

    Die Bezeichnungen sind fuer den Adminbereich; der Spiel-Client fuehrt
    seine eigenen Anzeigenamen.
    """

    VOID = "void", "Leere"
    GROUND = "ground", "Boden"
    WATER_SHALLOW = "water_shallow", "Seichtes Wasser"
    MUD = "mud", "Matsch"
    MEADOW = "meadow", "Wiese"
    FOREST = "forest", "Wald"
    WATER_DEEP = "water_deep", "Tiefes Wasser"
    CITY = "city", "Stadt"
    ROAD = "road", "Strasse"
    FORTIFICATION = "fortification", "Befestigte Stellung"
    DRAGONS_TEETH = "dragons_teeth", "Drachenzaehne"
    HIGH_GRASS = "high_grass", "Hohes Gras"
