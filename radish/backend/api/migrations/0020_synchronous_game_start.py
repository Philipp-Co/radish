from django.db import migrations, models


def _starting_games_back_to_lobby(apps, schema_editor):
    # Ein Spiel, das noch auf die (inzwischen entfallene) Bereit-Meldung
    # wartete, kommt nie mehr an -- es geht zurueck in die Lobby.
    Game = apps.get_model("api", "Game")
    Game.objects.filter(status="starting").update(status="lobby")


class Migration(migrations.Migration):
    """
    Der Start ist jetzt synchron (siehe client_views.GameStartView): ohne
    Zwischenzustand "starting" und ohne Startzeitpunkt fuer den Timeout.
    """

    dependencies = [
        ("api", "0019_game_lobby"),
    ]

    operations = [
        migrations.RunPython(_starting_games_back_to_lobby, migrations.RunPython.noop),
        migrations.RemoveField(
            model_name="game",
            name="start_requested_at",
        ),
        migrations.AlterField(
            model_name="game",
            name="status",
            field=models.CharField(
                choices=[("lobby", "Lobby"), ("running", "Laeuft")],
                default="lobby",
                max_length=20,
            ),
        ),
    ]
