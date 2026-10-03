"""
Waffenklassen und Einheitswaffen-Slots.

- Weapon.is_heavy wird zu weapon_class (standard/heavy/super_heavy);
  is_heavy=True wird zu "heavy".
- Weapon.range wird zu max_range, dazu min_range (Default 0 = keine
  Mindestreichweite).
- UnitType.can_carry_heavy_weapon wird zu heavy_weapon_slots (1 bzw. 0),
  dazu super_heavy_weapon_slots (0).
- Unit.heavy_weapon wird zu einem UnitWeapon in Slot 0 der Klasse "heavy".
"""

import django.db.models.deletion
from django.db import migrations, models

WEAPON_CLASS_CHOICES = [
    ("standard", "Waffe"),
    ("heavy", "Schwere Waffe"),
    ("super_heavy", "Super-schwere Waffe"),
]


def forwards(apps, schema_editor):
    Weapon = apps.get_model("api", "Weapon")
    UnitType = apps.get_model("api", "UnitType")
    Unit = apps.get_model("api", "Unit")
    UnitWeapon = apps.get_model("api", "UnitWeapon")

    Weapon.objects.filter(is_heavy=True).update(weapon_class="heavy")
    UnitType.objects.filter(can_carry_heavy_weapon=True).update(heavy_weapon_slots=1)
    UnitWeapon.objects.bulk_create(
        UnitWeapon(unit_id=unit.id, weapon_class="heavy", slot=0, weapon_id=unit.heavy_weapon_id)
        for unit in Unit.objects.exclude(heavy_weapon=None)
    )


def backwards(apps, schema_editor):
    Weapon = apps.get_model("api", "Weapon")
    UnitType = apps.get_model("api", "UnitType")
    Unit = apps.get_model("api", "Unit")
    UnitWeapon = apps.get_model("api", "UnitWeapon")

    # Super-schwere Waffen kennt das alte Schema nicht -- sie zaehlen dort
    # als schwer; von mehreren Einheitswaffen bleibt nur die erste.
    Weapon.objects.exclude(weapon_class="standard").update(is_heavy=True)
    UnitType.objects.filter(heavy_weapon_slots__gt=0).update(can_carry_heavy_weapon=True)
    for slot in UnitWeapon.objects.order_by("unit_id", "weapon_class", "slot"):
        Unit.objects.filter(pk=slot.unit_id, heavy_weapon=None).update(
            heavy_weapon_id=slot.weapon_id
        )


class Migration(migrations.Migration):

    dependencies = [
        ("api", "0010_costs"),
    ]

    operations = [
        migrations.AddField(
            model_name="weapon",
            name="weapon_class",
            field=models.CharField(
                choices=WEAPON_CLASS_CHOICES, default="standard", max_length=20
            ),
        ),
        migrations.RenameField(model_name="weapon", old_name="range", new_name="max_range"),
        migrations.AddField(
            model_name="weapon",
            name="min_range",
            field=models.PositiveSmallIntegerField(default=0),
        ),
        migrations.AddField(
            model_name="unittype",
            name="heavy_weapon_slots",
            field=models.PositiveSmallIntegerField(default=0),
        ),
        migrations.AddField(
            model_name="unittype",
            name="super_heavy_weapon_slots",
            field=models.PositiveSmallIntegerField(default=0),
        ),
        migrations.CreateModel(
            name="UnitWeapon",
            fields=[
                ("id", models.BigAutoField(auto_created=True, primary_key=True, serialize=False, verbose_name="ID")),
                ("weapon_class", models.CharField(choices=WEAPON_CLASS_CHOICES, max_length=20)),
                ("slot", models.PositiveSmallIntegerField()),
                (
                    "unit",
                    models.ForeignKey(
                        on_delete=django.db.models.deletion.CASCADE,
                        related_name="weapons",
                        to="api.unit",
                    ),
                ),
                (
                    "weapon",
                    models.ForeignKey(
                        on_delete=django.db.models.deletion.PROTECT,
                        related_name="+",
                        to="api.weapon",
                    ),
                ),
            ],
            options={
                "ordering": ["unit", "weapon_class", "slot"],
                "constraints": [
                    models.UniqueConstraint(
                        fields=("unit", "weapon_class", "slot"), name="unique_unit_weapon_slot"
                    )
                ],
            },
        ),
        migrations.RunPython(forwards, backwards),
        migrations.RemoveField(model_name="weapon", name="is_heavy"),
        migrations.RemoveField(model_name="unittype", name="can_carry_heavy_weapon"),
        migrations.RemoveField(model_name="unit", name="heavy_weapon"),
        migrations.AlterModelOptions(
            name="weapon",
            options={"ordering": ["species", "name"]},
        ),
    ]
