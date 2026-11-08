from rest_framework import serializers

from api.interfaces.player.serializers.unit_read_serializer import UnitReadSerializer
from api.models import Army

# ---------------------------------------------------------------------------
# Armeen
#
# Nach aussen verweist eine Armee nur per id auf den Katalog (species_id,
# unit_type_id, profile_id, weapon_ids, ...) -- die Werte dazu holt sich der
# Client einmal ueber den Katalog-Endpunkt, statt sie in jeder Armee
# mitzuschicken. weapon_ids/equipment_ids sind nach Slot geordnet.
#
# total_cost auf Entitaet, Einheit und Armee rechnen die Modelle aus dem
# Katalog aus (siehe models.Army.total_cost) -- nur lesend, der Client
# schickt keine Kosten mit.
# ---------------------------------------------------------------------------


class ArmyDetailSerializer(serializers.ModelSerializer):
    species_name = serializers.CharField(source="species.name", read_only=True)
    units = UnitReadSerializer(many=True, read_only=True)
    total_cost = serializers.IntegerField(read_only=True)

    class Meta:
        model = Army
        fields = ["id", "name", "species_id", "species_name", "units", "total_cost"]
