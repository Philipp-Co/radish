from rest_framework import serializers

from api.interfaces.player.serializers.equipment_serializer import EquipmentSerializer
from api.interfaces.player.serializers.unit_type_serializer import UnitTypeSerializer
from api.interfaces.player.serializers.weapon_serializer import WeaponSerializer
from api.models import Species

# ---------------------------------------------------------------------------
# Katalog (nur lesend -- Schreiben kommt mit den Admin-Schnittstellen)
# ---------------------------------------------------------------------------


class SpeciesCatalogSerializer(serializers.ModelSerializer):
    """
    Eine Spezies mit allem, was eine Armee von ihr enthalten kann -- genau
    das, was der Armee-Editor im Frontend zur Auswahl braucht.
    """

    unit_types = UnitTypeSerializer(many=True, read_only=True)
    weapons = WeaponSerializer(many=True, read_only=True)
    equipment = EquipmentSerializer(many=True, read_only=True)

    class Meta:
        model = Species
        fields = ["id", "name", "unit_types", "weapons", "equipment"]
