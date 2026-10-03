from django.db import migrations, models


def _existing_games_are_running(apps, schema_editor):
    # Spiele von vor der Lobby liefen bereits -- sie sollen nicht
    # nachtraeglich in der Lobby landen.
    Game = apps.get_model("api", "Game")
    Game.objects.update(status="running")


class Migration(migrations.Migration):
    """
    Lobby vor dem Spielstart: Game.status samt Startzeitpunkt, dazu die
    Steuer-URL der Instanz, ueber die das Backend den Start anstoesst.
    """

    dependencies = [
        ("api", "0018_unittype_movement"),
    ]

    operations = [
        migrations.AddField(
            model_name="gameserver",
            name="control_url",
            field=models.CharField(blank=True, default="", max_length=255),
        ),
        migrations.AddField(
            model_name="game",
            name="status",
            field=models.CharField(
                choices=[
                    ("lobby", "Lobby"),
                    ("starting", "Wird gestartet"),
                    ("running", "Laeuft"),
                ],
                default="lobby",
                max_length=20,
            ),
        ),
        migrations.AddField(
            model_name="game",
            name="start_requested_at",
            field=models.DateTimeField(blank=True, null=True),
        ),
        migrations.RunPython(_existing_games_are_running, migrations.RunPython.noop),
    ]
