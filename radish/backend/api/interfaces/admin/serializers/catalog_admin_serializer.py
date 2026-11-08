from collections.abc import Callable, Mapping
from typing import TYPE_CHECKING

from django.db.models import Field, ForeignObjectRel, Model
from rest_framework import serializers

# Nur fuer die Annotation in _changed (Rueckgabetyp von Options.get_field);
# zur Laufzeit wuerde der Import die contenttypes-Models laden, und
# Annotationen lokaler Variablen werden ohnehin nie ausgewertet.
if TYPE_CHECKING:
    from django.contrib.contenttypes.fields import GenericForeignKey


# ---------------------------------------------------------------------------
# Katalog-Pflege fuer Admins (siehe admin_views.py)
#
# Der Admin darf jeden Eintrag jederzeit aendern, auch wenn Spieler ihn in
# ihren Armeen verwenden -- Aenderungen am Katalog sind Aenderungen an
# Werten, keine strukturellen, deshalb werden gespeicherte Armeen danach
# nicht erneut geprueft. Fest bleibt nur:
# - immutable_fields: die Zuordnung (Spezies bzw. Einheitentyp) steht nach
#   dem Anlegen fest -- sonst gehoerte z.B. eine Waffe ploetzlich einer
#   anderen Spezies, und der Katalog der Spezies waere nicht mehr eindeutig.
# - Loeschen: ein verwendeter Eintrag laesst sich nicht loeschen (PROTECT,
#   siehe admin_views), sonst verschwaenden Einheiten aus Spielerarmeen.
#
# Ob ein Eintrag verwendet wird, liefert die View als Annotation "in_use"
# (siehe admin_views.IN_USE); frisch angelegte Eintraege sind es nie.
# ---------------------------------------------------------------------------


def _current_or_new[M: Model](instance: M | None, model: type[M]) -> M:
    """Das gespeicherte Objekt, beim Anlegen ein neues mit den Defaults des Modells."""
    return instance if instance is not None else model()


def _value[T](attrs: Mapping[str, object], name: str, current: Callable[[], T], expected: type[T]) -> T:
    """
    Wert aus der Eingabe; fehlt er, current() -- der bisherige Wert bei PATCH
    bzw. beim Anlegen der Default des Modells (z.B. Weapon.min_range = 0, aus
    einem ungespeicherten Weapon()). current wird nur aufgerufen, wenn der Wert
    fehlt: ein ungespeichertes Objekt hat noch keinen ForeignKey.

    expected ist der Typ des Feldes (int fuer die Zahlenfelder, das Model
    bei einem ForeignKey). Die Felder sind alle nicht nullable, und ein
    Pflichtfeld ohne Default steht beim Anlegen immer in attrs -- der Wert
    hat also immer diesen Typ; das assert macht das fuer mypy sichtbar.
    """
    value: object = attrs[name] if name in attrs else current()
    assert isinstance(value, expected)
    return value


def _changed(instance: Model, name: str, new: object) -> bool:
    """
    Ob new vom gespeicherten Wert des Feldes name abweicht. Bei einem
    ForeignKey zaehlt die id: value_from_object liefert sie, ohne das
    verknuepfte Objekt zu laden.
    """
    field: Field[object, object] | ForeignObjectRel | GenericForeignKey = instance._meta.get_field(name)
    # immutable_fields nennt nur konkrete Modellfelder, nie Rueckwaertsrelationen
    # oder GenericForeignKeys.
    assert isinstance(field, Field)
    stored: object = field.value_from_object(instance)
    return (new.pk if isinstance(new, Model) else new) != stored


class _CatalogAdminSerializer(serializers.ModelSerializer):
    in_use = serializers.BooleanField(read_only=True, default=False)

    immutable_fields: tuple[str, ...] = ()

    # Die Stubs typisieren attrs als Any; es sind die validierten Daten, ein
    # dict von Feldname auf Wert.
    def validate(self, attrs: dict[str, object]) -> dict[str, object]:
        if self.instance is None:
            return attrs
        field: str
        for field in self.immutable_fields:
            if field in attrs and _changed(self.instance, field, attrs[field]):
                raise serializers.ValidationError(
                    {field: "Kann nach dem Anlegen nicht mehr geaendert werden."}
                )
        return attrs
