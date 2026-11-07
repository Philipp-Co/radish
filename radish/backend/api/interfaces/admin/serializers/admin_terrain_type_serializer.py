from rest_framework import serializers

from ....models import TerrainType


class AdminTerrainTypeSerializer(serializers.ModelSerializer):
    """
    Ein Tile-Typ mit seinen Werten. Der Typ selbst (code) steht fest -- er
    gehoert zum Spiel --, aenderbar sind nur MV, CV, unbegehbar und sichtblockierend. "label"
    ist die Bezeichnung fuer den Adminbereich.
    """

    label = serializers.CharField(source="get_code_display", read_only=True)

    class Meta:
        model = TerrainType
        fields = ["code", "label", "movement_modifier", "cover", "impassable", "blocks_sight"]
        read_only_fields = ["code"]
