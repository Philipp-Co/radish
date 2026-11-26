from collections.abc import Iterable

from django.db import models

from api.models.entity_profile import EntityProfile
from api.models.unit_type import UnitType


class ProfileRestricted(models.Model):
    """
    Ein Katalogeintrag (Waffe, Ausruestung), den nicht jede Entitaet seiner
    Spezies tragen darf. Ohne Einschraenkung darf es jede; sonst jede
    Entitaet eines der unit_types und zusaetzlich jede mit einem der
    profiles (siehe allows). Beide gehoeren zur Spezies des Eintrags -- das
    prueft die Admin-API.
    """

    # Leer heisst: keine Einschraenkung ueber diesen Weg. Kein PROTECT noetig
    # -- ein geloeschter Einheitentyp faellt einfach aus der Liste.
    unit_types = models.ManyToManyField(UnitType, blank=True, related_name="+")
    profiles = models.ManyToManyField(EntityProfile, blank=True, related_name="+")

    class Meta:
        abstract = True

    def allows(self, profile: EntityProfile) -> bool:
        """
        Ob eine Entitaet mit diesem Profil den Eintrag tragen darf. Liest die
        Einschraenkungen ueber .all(), nutzt also ein prefetch_related. Die
        Spezies prueft der Aufrufer.
        """
        unit_type_ids: set[int] = _pks(self.unit_types.all())
        profile_ids: set[int] = _pks(self.profiles.all())
        if not unit_type_ids and not profile_ids:
            return True
        return profile.unit_type_id in unit_type_ids or profile.pk in profile_ids


def _pks(entries: Iterable[models.Model]) -> set[int]:
    return {entry.pk for entry in entries}
