"""
Legt die erste Spezies "Menschen" an -- ohne Einheitentypen, Waffen oder
Ausruestung: die traegt spaeter ein Admin ueber eigene Schnittstellen ein.
"""

from django.db import migrations

SPECIES_NAME = "Menschen"


def create_species(apps, schema_editor):
    Species = apps.get_model("api", "Species")
    Species.objects.get_or_create(name=SPECIES_NAME)


def delete_species(apps, schema_editor):
    Species = apps.get_model("api", "Species")
    Species.objects.filter(name=SPECIES_NAME).delete()


class Migration(migrations.Migration):

    dependencies = [
        ("api", "0008_armies"),
    ]

    operations = [
        migrations.RunPython(create_species, delete_species),
    ]
