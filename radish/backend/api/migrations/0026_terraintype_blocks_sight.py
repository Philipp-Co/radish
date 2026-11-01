from django.db import migrations, models


# Was die Sicht auf die Felder dahinter versperrt: Wald, Haeuser und die
# Waelle einer befestigten Stellung. Alles andere ist offen.
BLOCKS_SIGHT = ["forest", "city", "fortification"]


def set_blocks_sight(apps, schema_editor):
    apps.get_model("api", "TerrainType").objects.filter(code__in=BLOCKS_SIGHT).update(blocks_sight=True)


class Migration(migrations.Migration):
    """
    Sichtblockierendes Gelaende (TerrainType.blocks_sight): ein Feld dieses
    Typs zwischen Schuetze und Ziel versperrt den Angriff.
    """

    dependencies = [
        ("api", "0025_gameserver_instance_name"),
    ]

    operations = [
        migrations.AddField(
            model_name="terraintype",
            name="blocks_sight",
            field=models.BooleanField(default=False),
        ),
        migrations.RunPython(set_blocks_sight, migrations.RunPython.noop),
    ]
