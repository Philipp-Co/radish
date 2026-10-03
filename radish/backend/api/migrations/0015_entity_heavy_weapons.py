"""
Schwere und super-schwere Waffen tragen jetzt einzelne Entitaeten statt
der Einheit.

- EntityProfile bekommt heavy_weapon_slots/super_heavy_weapon_slots; die
  bisherigen Slotzahlen eines Einheitentyps gehen an alle seine Profile --
  so bleiben bestehende Armeen gueltig, der Admin kann danach enger fassen.
- UnitEntityWeapon bekommt die Klasse ihres Slots (weapon_class); die
  bisherigen Einheitswaffen (UnitWeapon) wandern an die erste Entitaet
  ihrer Einheit.
- UnitType.heavy_weapon_slots/super_heavy_weapon_slots und UnitWeapon
  entfallen.
"""

from django.db import migrations, models

WEAPON_CLASS_CHOICES = [
    ("standard", "Waffe"),
    ("heavy", "Schwere Waffe"),
    ("super_heavy", "Super-schwere Waffe"),
]


def forwards(apps, schema_editor):
    UnitType = apps.get_model("api", "UnitType")
    EntityProfile = apps.get_model("api", "EntityProfile")
    UnitEntity = apps.get_model("api", "UnitEntity")
    UnitWeapon = apps.get_model("api", "UnitWeapon")
    UnitEntityWeapon = apps.get_model("api", "UnitEntityWeapon")

    for unit_type in UnitType.objects.exclude(heavy_weapon_slots=0, super_heavy_weapon_slots=0):
        EntityProfile.objects.filter(unit_type=unit_type).update(
            heavy_weapon_slots=unit_type.heavy_weapon_slots,
            super_heavy_weapon_slots=unit_type.super_heavy_weapon_slots,
        )

    first_entity = {}
    for entity in UnitEntity.objects.order_by("unit_id", "position"):
        first_entity.setdefault(entity.unit_id, entity.id)
    UnitEntityWeapon.objects.bulk_create(
        UnitEntityWeapon(
            entity_id=first_entity[slot.unit_id],
            weapon_class=slot.weapon_class,
            slot=slot.slot,
            weapon_id=slot.weapon_id,
        )
        for slot in UnitWeapon.objects.all()
        if slot.unit_id in first_entity
    )


def backwards(apps, schema_editor):
    UnitType = apps.get_model("api", "UnitType")
    EntityProfile = apps.get_model("api", "EntityProfile")
    UnitWeapon = apps.get_model("api", "UnitWeapon")
    UnitEntityWeapon = apps.get_model("api", "UnitEntityWeapon")

    # Slots zurueck an den Typ: so viele, wie das groesste Profil hatte.
    for unit_type in UnitType.objects.all():
        aggregate = EntityProfile.objects.filter(unit_type=unit_type).aggregate(
            heavy=models.Max("heavy_weapon_slots"),
            super_heavy=models.Max("super_heavy_weapon_slots"),
        )
        unit_type.heavy_weapon_slots = aggregate["heavy"] or 0
        unit_type.super_heavy_weapon_slots = aggregate["super_heavy"] or 0
        unit_type.save(update_fields=["heavy_weapon_slots", "super_heavy_weapon_slots"])

    # Schwere Waffen aller Entitaeten zurueck an die Einheit, je Klasse neu
    # durchnummeriert.
    next_slot = {}
    heavy = UnitEntityWeapon.objects.exclude(weapon_class="standard").select_related("entity")
    for slot in heavy.order_by("entity__unit_id", "entity__position", "weapon_class", "slot"):
        key = (slot.entity.unit_id, slot.weapon_class)
        UnitWeapon.objects.create(
            unit_id=slot.entity.unit_id,
            weapon_class=slot.weapon_class,
            slot=next_slot.get(key, 0),
            weapon_id=slot.weapon_id,
        )
        next_slot[key] = next_slot.get(key, 0) + 1
    heavy.delete()


class Migration(migrations.Migration):

    dependencies = [
        ("api", "0014_game_armies"),
    ]

    operations = [
        migrations.AddField(
            model_name="entityprofile",
            name="heavy_weapon_slots",
            field=models.PositiveSmallIntegerField(default=0),
        ),
        migrations.AddField(
            model_name="entityprofile",
            name="super_heavy_weapon_slots",
            field=models.PositiveSmallIntegerField(default=0),
        ),
        migrations.RemoveConstraint(
            model_name="unitentityweapon",
            name="unique_weapon_slot_per_entity",
        ),
        migrations.AddField(
            model_name="unitentityweapon",
            name="weapon_class",
            field=models.CharField(
                choices=WEAPON_CLASS_CHOICES, default="standard", max_length=20
            ),
        ),
        migrations.AddConstraint(
            model_name="unitentityweapon",
            constraint=models.UniqueConstraint(
                fields=("entity", "weapon_class", "slot"), name="unique_weapon_slot_per_entity"
            ),
        ),
        migrations.AlterModelOptions(
            name="unitentityweapon",
            options={"ordering": ["entity", "weapon_class", "slot"]},
        ),
        migrations.RunPython(forwards, backwards),
        migrations.RemoveField(model_name="unittype", name="heavy_weapon_slots"),
        migrations.RemoveField(model_name="unittype", name="super_heavy_weapon_slots"),
        migrations.DeleteModel(name="UnitWeapon"),
    ]
