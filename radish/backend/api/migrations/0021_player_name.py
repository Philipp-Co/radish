from django.db import migrations, models


class Migration(migrations.Migration):
    """
    Keycloak-Benutzername am Player, fuer die Spieldaten beim Spielstart.
    Bestehende Player bekommen ihn beim naechsten Aufruf nachgetragen.
    """

    dependencies = [
        ("api", "0020_synchronous_game_start"),
    ]

    operations = [
        migrations.AddField(
            model_name="player",
            name="name",
            field=models.CharField(blank=True, default="", max_length=255),
        ),
    ]
