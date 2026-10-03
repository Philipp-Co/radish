from django.db import migrations, models


class Migration(migrations.Migration):
    """
    Die geheimen Zucchini-Codes der beiden Spieler am Spiel (siehe
    access.py). Bestehende Spiele bleiben ohne -- sie bekommen beim
    naechsten Start welche.
    """

    dependencies = [
        ("api", "0021_player_name"),
    ]

    operations = [
        migrations.AddField(
            model_name="game",
            name="host_zucchini_code",
            field=models.CharField(blank=True, default="", max_length=16),
        ),
        migrations.AddField(
            model_name="game",
            name="second_player_zucchini_code",
            field=models.CharField(blank=True, default="", max_length=16),
        ),
    ]
