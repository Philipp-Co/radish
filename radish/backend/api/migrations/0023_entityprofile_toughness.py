from django.db import migrations, models


class Migration(migrations.Migration):
    """
    Der Widerstand eines Entitaetsprofils. Das Modell hat keinen Default --
    wie die uebrigen Werte traegt ihn der Admin ein. Bestehende Profile
    bekommen 0: jeder Schuss, der die Ruestung durchschlaegt, wirkt dann,
    bis ein Wert eingetragen ist.
    """

    dependencies = [
        ("api", "0022_game_zucchini_codes"),
    ]

    operations = [
        migrations.AddField(
            model_name="entityprofile",
            name="toughness",
            field=models.PositiveSmallIntegerField(default=0),
            preserve_default=False,
        ),
    ]
