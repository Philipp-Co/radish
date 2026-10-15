import django.core.validators
from django.db import migrations, models


# Die Werte, die bis dahin als Platzhalter im Spiel-Client standen
# (radish/client/src/control/terrain.c). Tiefes Wasser war dort nur teuer,
# weil es kein Unbegehbar gab -- jetzt ist es unbegehbar, wie die Leere.
# In der Reihenfolge der Typen im Spiel (RAD_TileType_t).
TERRAIN = [
    # code, movement_modifier, cover, impassable
    ("void", 0, 0, True),
    ("ground", 0, 1, False),
    ("water_shallow", -1, 0, False),
    ("mud", -1, 0, False),
    ("meadow", 0, 0, False),
    ("forest", -1, 2, False),
    ("water_deep", 0, 0, True),
    ("city", 0, 2, False),
    ("road", 1, 0, False),
    ("fortification", -1, 3, False),
]


def create_terrain(apps, schema_editor):
    TerrainType = apps.get_model("api", "TerrainType")
    for code, movement_modifier, cover, impassable in TERRAIN:
        TerrainType.objects.create(
            code=code,
            movement_modifier=movement_modifier,
            cover=cover,
            impassable=impassable,
        )


def delete_terrain(apps, schema_editor):
    apps.get_model("api", "TerrainType").objects.all().delete()


class Migration(migrations.Migration):
    """
    Die Werte der Tile-Typen (TerrainType), eine Zeile je Typ: aus dem Client
    ins Backend, damit der Admin sie pflegt und jeder Spielstart sie mitnimmt.
    """

    dependencies = [
        ("api", "0023_entityprofile_toughness"),
    ]

    operations = [
        migrations.CreateModel(
            name="TerrainType",
            fields=[
                ("id", models.BigAutoField(auto_created=True, primary_key=True, serialize=False, verbose_name="ID")),
                (
                    "code",
                    models.CharField(
                        choices=[
                            ("void", "Leere"),
                            ("ground", "Boden"),
                            ("water_shallow", "Seichtes Wasser"),
                            ("mud", "Matsch"),
                            ("meadow", "Wiese"),
                            ("forest", "Wald"),
                            ("water_deep", "Tiefes Wasser"),
                            ("city", "Stadt"),
                            ("road", "Strasse"),
                            ("fortification", "Befestigte Stellung"),
                        ],
                        max_length=20,
                        unique=True,
                    ),
                ),
                (
                    "movement_modifier",
                    models.SmallIntegerField(
                        default=0,
                        validators=[
                            django.core.validators.MinValueValidator(-10),
                            django.core.validators.MaxValueValidator(10),
                        ],
                    ),
                ),
                (
                    "cover",
                    models.PositiveSmallIntegerField(
                        default=0, validators=[django.core.validators.MaxValueValidator(10)]
                    ),
                ),
                ("impassable", models.BooleanField(default=False)),
            ],
            options={
                "ordering": ["id"],
            },
        ),
        migrations.RunPython(create_terrain, delete_terrain),
    ]
