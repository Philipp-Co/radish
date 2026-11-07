from rest_framework import serializers


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


def _value(serializer, attrs, name):
    """
    Wert aus der Eingabe; fehlt er, bei PATCH der bisherige und beim
    Anlegen der Default des Modells (z.B. Weapon.min_range = 0).
    """
    if name in attrs:
        return attrs[name]
    if serializer.instance is not None:
        return getattr(serializer.instance, name)
    return serializer.Meta.model._meta.get_field(name).get_default()


class _CatalogAdminSerializer(serializers.ModelSerializer):
    in_use = serializers.BooleanField(read_only=True, default=False)

    immutable_fields = ()

    def validate(self, attrs):
        if self.instance is None:
            return attrs
        for field in self.immutable_fields:
            if field in attrs and attrs[field] != getattr(self.instance, field):
                raise serializers.ValidationError(
                    {field: "Kann nach dem Anlegen nicht mehr geaendert werden."}
                )
        return attrs
