from django.db import migrations, models


class Migration(migrations.Migration):
    """
    Treffsicherheit fuer Entitaetsprofile. Wie bei den Kosten (0010) im
    Modell bewusst ohne Default; bestehende Profile bekommen einmalig 0 und
    muessen im Katalog nachgetragen werden.
    """

    dependencies = [
        ("api", "0012_transport_capacity"),
    ]

    operations = [
        migrations.AddField(
            model_name="entityprofile",
            name="accuracy",
            field=models.PositiveSmallIntegerField(default=0),
            preserve_default=False,
        ),
    ]
