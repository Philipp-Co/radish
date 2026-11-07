from collections.abc import Mapping
from typing import TYPE_CHECKING

from django.db.models import Field, ForeignObjectRel, Model
from rest_framework import serializers

# Nur fuer die Annotation in _value (Rueckgabetyp von Options.get_field);
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


def _value[T](
    serializer: serializers.ModelSerializer[Model],
    attrs: Mapping[str, object],
    name: str,
    expected: type[T],
) -> T:
    """
    Wert aus der Eingabe; fehlt er, bei PATCH der bisherige und beim
    Anlegen der Default des Modells (z.B. Weapon.min_range = 0).

    expected ist der Typ des Feldes (int fuer die Zahlenfelder, das Model
    bei einem ForeignKey). Die Felder sind alle nicht nullable, und ein
    Pflichtfeld ohne Default steht beim Anlegen immer in attrs -- der Wert
    hat also immer diesen Typ; das assert macht das fuer mypy sichtbar.
    """
    value: object
    if name in attrs:
        value = attrs[name]
    elif serializer.instance is not None:
        value = getattr(serializer.instance, name)
    else:
        # Meta.model ist in den Stubs nur ueber die TypeVar typisiert.
        model: type[Model] = serializer.Meta.model
        field: Field[object, object] | ForeignObjectRel | GenericForeignKey = model._meta.get_field(name)
        # _value wird nur mit konkreten Modellfeldern aufgerufen, nie mit
        # Rueckwaertsrelationen oder GenericForeignKeys.
        assert isinstance(field, Field)
        value = field.get_default()
    assert isinstance(value, expected)
    return value


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
            if field in attrs and attrs[field] != getattr(self.instance, field):
                raise serializers.ValidationError(
                    {field: "Kann nach dem Anlegen nicht mehr geaendert werden."}
                )
        return attrs
