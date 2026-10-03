from django.db import migrations, models


class Migration(migrations.Migration):
    """
    Kosten fuer Entitaetsprofile, Waffen und Ausruestung. Der Katalog ist
    bis hierhin leer (siehe 0009), die 0 fuer bestehende Zeilen greift also
    nur theoretisch -- im Modell gibt es bewusst keinen Default.
    """

    dependencies = [
        ("api", "0009_seed_species_menschen"),
    ]

    operations = [
        migrations.AddField(
            model_name="entityprofile",
            name="cost",
            field=models.PositiveIntegerField(default=0),
            preserve_default=False,
        ),
        migrations.AddField(
            model_name="weapon",
            name="cost",
            field=models.PositiveIntegerField(default=0),
            preserve_default=False,
        ),
        migrations.AddField(
            model_name="equipment",
            name="cost",
            field=models.PositiveIntegerField(default=0),
            preserve_default=False,
        ),
    ]
