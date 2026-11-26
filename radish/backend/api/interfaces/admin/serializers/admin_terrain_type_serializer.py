from django.db.models import Prefetch, QuerySet
from rest_framework import serializers

from api.models import TERRAIN_DECOR_MAX, TERRAIN_DECOR_OFFSET_MAX, TerrainDecor, TerrainImage, TerrainType


def terrain_for_admin() -> QuerySet[TerrainType]:
    """Die Typen samt PNGs (ohne Bytes) und Dekorationen -- eine Abfrage je Beziehung."""
    return TerrainType.objects.prefetch_related(
        Prefetch("images", queryset=TerrainImage.objects.defer("data")),
        Prefetch("decor", queryset=TerrainDecor.objects.select_related("image").defer("image__data")),
    )


class AdminTerrainImageSerializer(serializers.ModelSerializer):
    """Eine PNG eines Typs, ohne ihre Bytes -- die liefert admin_terrain_views.AdminTerrainImageFileView."""

    class Meta:
        model = TerrainImage
        fields = ["id", "name", "width", "height", "sha256"]
        read_only_fields = fields


class AdminTerrainDecorSerializer(serializers.ModelSerializer):
    """
    Eine Dekoration: welche PNG des Typs, wie weit verschoben, ob vor den
    Figuren. Die Ebene ergibt sich aus der Reihenfolge der Liste
    (AdminTerrainLookSerializer).

    Erwartet im Kontext "terrain", damit nur dessen PNGs gehen.
    """

    image_id = serializers.PrimaryKeyRelatedField(source="image", queryset=TerrainImage.objects.none())

    class Meta:
        model = TerrainDecor
        fields = ["image_id", "offset_x", "offset_y", "in_front"]
        extra_kwargs = {
            "offset_x": {"min_value": -TERRAIN_DECOR_OFFSET_MAX, "max_value": TERRAIN_DECOR_OFFSET_MAX},
            "offset_y": {"min_value": -TERRAIN_DECOR_OFFSET_MAX, "max_value": TERRAIN_DECOR_OFFSET_MAX},
        }

    def __init__(self, *args: object, **kwargs: object) -> None:
        super().__init__(*args, **kwargs)  # type: ignore[arg-type]
        _limit_to_terrain(self.fields["image_id"], self.context.get("terrain"))


class AdminTerrainLookSerializer(serializers.Serializer):
    """
    Wie ein Typ aussieht, auf einmal gesetzt (admin_terrain_views.
    AdminTerrainLookView): das Bild statt des Wuerfels ("image_id", null fuer
    keins) und die Dekorationen in der Reihenfolge, in der sie gezeichnet
    werden -- was fehlt, gibt es danach nicht mehr.

    Erwartet im Kontext "terrain".
    """

    image_id = serializers.PrimaryKeyRelatedField(queryset=TerrainImage.objects.none(), allow_null=True)
    decor = serializers.ListField(child=serializers.DictField(), max_length=TERRAIN_DECOR_MAX)

    def __init__(self, *args: object, **kwargs: object) -> None:
        super().__init__(*args, **kwargs)  # type: ignore[arg-type]
        _limit_to_terrain(self.fields["image_id"], self.context.get("terrain"))

    def validate_decor(self, value: list[dict[str, object]]) -> list[dict[str, object]]:
        """Jede Dekoration fuer sich, mit den Meldungen nach Stelle."""
        entries: AdminTerrainDecorSerializer = AdminTerrainDecorSerializer(
            data=value, many=True, context=self.context
        )
        entries.is_valid(raise_exception=True)
        validated: list[dict[str, object]] = entries.validated_data
        return validated


def _limit_to_terrain(field: serializers.Field, terrain: TerrainType | None) -> None:
    """Nur die PNGs von "terrain" -- eine fremde ist so gut wie keine."""
    assert isinstance(field, serializers.PrimaryKeyRelatedField)
    field.queryset = TerrainImage.objects.filter(terrain=terrain).defer("data")


class AdminTerrainTypeSerializer(serializers.ModelSerializer):
    """
    Ein Tile-Typ mit seinen Werten. Der Typ selbst (code) steht fest -- er
    gehoert zum Spiel --, aenderbar sind nur MV, CV, unbegehbar und sichtblockierend. "label"
    ist die Bezeichnung fuer den Adminbereich.

    Dazu, nur lesend, wie er aussieht: seine PNGs, das Grundbild statt des
    Wuerfels ("image_id", TerrainImage.is_base) und die Dekorationen -- geaendert ueber eigene Endpunkte
    (admin_terrain_views.py).
    """

    # In den Stubs ist label das Beschriftungsattribut jedes Feldes (str | None).
    # Zur Laufzeit entfernt der SerializerMetaclass deklarierte Felder aus den
    # Klassenattributen, es gibt also keine Kollision -- nur mypy sieht eine;
    # deshalb das ignore statt eines Any, der Typ des Feldes bleibt sichtbar.
    label = serializers.CharField(source="get_code_display", read_only=True)  # type: ignore[assignment]
    images = AdminTerrainImageSerializer(many=True, read_only=True)
    decor = AdminTerrainDecorSerializer(many=True, read_only=True)
    image_id = serializers.SerializerMethodField()

    class Meta:
        model = TerrainType
        fields = [
            "code",
            "label",
            "movement_modifier",
            "cover",
            "impassable",
            "blocks_sight",
            "image_id",
            "images",
            "decor",
        ]
        read_only_fields = ["code"]

    def get_image_id(self, terrain: TerrainType) -> int | None:
        """Das Grundbild (TerrainImage.is_base) aus den schon geladenen PNGs."""
        return next((image.pk for image in terrain.images.all() if image.is_base), None)
