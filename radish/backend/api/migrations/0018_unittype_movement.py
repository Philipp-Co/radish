from django.db import migrations, models


class Migration(migrations.Migration):
    """
    Bewegungsradius fuer Einheitentypen. Im Modell bewusst ohne Default;
    bestehende Einheitentypen bekommen einmalig 0 und muessen im Katalog
    nachgetragen werden.
    """

    dependencies = [
        ("api", "0017_unit_type_capture_objectives"),
    ]

    operations = [
        migrations.AddField(
            model_name="unittype",
            name="movement",
            field=models.PositiveSmallIntegerField(default=0),
            preserve_default=False,
        ),
    ]
