from django.db import models
from django.db.models import QuerySet

from api.models import Army, Unit, UnitEntity, UnitEntityEquipment, UnitEntityWeapon


def armies_with_costs() -> QuerySet[Army]:
    """
    Armeen samt allem, was fuer Army.total_cost und die Ausgabe noetig ist
    -- inklusive der Katalogeintraege mit ihren Preisen, damit keine Abfrage
    je Einheit, Entitaet oder Slot entsteht.
    """
    return Army.objects.select_related("species").prefetch_related(
        # Der Einheitentyp gleich mit -- fuer die Spieldaten beim Start
        # (instance_client.game_start_document) und die Ausgabe der Armee.
        models.Prefetch("units", queryset=Unit.objects.select_related("unit_type")),
        models.Prefetch(
            "units__entities",
            queryset=UnitEntity.objects.select_related("profile").prefetch_related(
                models.Prefetch(
                    "weapons", queryset=UnitEntityWeapon.objects.select_related("weapon")
                ),
                models.Prefetch(
                    "equipment",
                    queryset=UnitEntityEquipment.objects.select_related("equipment"),
                ),
            ),
        ),
    )
