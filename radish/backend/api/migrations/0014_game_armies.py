"""
Punktelimit und Armeen fuer Spiele. Laufende Spiele aus der Zeit vor den
Armeen bekommen einmalig das Punktelimit 0 und keine Armeen -- im Modell
hat points_limit bewusst keinen Default, neue Spiele geben es immer an.
"""

import django.db.models.deletion
from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ("api", "0013_entityprofile_accuracy"),
    ]

    operations = [
        migrations.AddField(
            model_name="game",
            name="points_limit",
            field=models.PositiveIntegerField(default=0),
            preserve_default=False,
        ),
        migrations.AddField(
            model_name="game",
            name="host_army",
            field=models.ForeignKey(
                blank=True,
                null=True,
                on_delete=django.db.models.deletion.PROTECT,
                related_name="+",
                to="api.army",
            ),
        ),
        migrations.AddField(
            model_name="game",
            name="second_player_army",
            field=models.ForeignKey(
                blank=True,
                null=True,
                on_delete=django.db.models.deletion.PROTECT,
                related_name="+",
                to="api.army",
            ),
        ),
    ]
