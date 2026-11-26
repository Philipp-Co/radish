"""
Effekte im Adminbereich (siehe admin_effect_views.py): der Effekt mit seinen
Werten, seinen PNGs und je Richtung einem Atlas, dazu, ob er vollstaendig ist
und was ihm fehlt (api/domain/effects/problems.py).
"""

from rest_framework import serializers

from api.domain.effects.problems import effect_problems
from api.domain.sprites.problems import FACINGS
from api.models import (
    EFFECT_FIRST_FRAME_MAX,
    SPRITE_FRAME_SIZE_MAX,
    SPRITE_FRAMES_MAX,
    Effect,
    EffectAtlas,
    EffectImage,
)


class AdminEffectImageSerializer(serializers.ModelSerializer):
    """Eine PNG, ohne ihre Bytes -- die liefert admin_effect_views.AdminEffectImageFileView."""

    class Meta:
        model = EffectImage
        fields = ["id", "name", "width", "height", "sha256"]
        read_only_fields = fields


class AdminEffectAtlasSerializer(serializers.ModelSerializer):
    """
    Die Folge einer Richtung. Geprueft wird hier nur, was jede fuer sich braucht
    -- gueltige Richtung, eine PNG dieses Effekts, Zahlen im Rahmen. Ob alle
    Richtungen da sind und zur PNG passen, sagt "problems" am Effekt: ein
    Entwurf darf unfertig gespeichert werden.

    Erwartet im Kontext "effect".
    """

    image_id = serializers.PrimaryKeyRelatedField(source="image", queryset=EffectImage.objects.none())

    class Meta:
        model = EffectAtlas
        fields = [
            "facing",
            "image_id",
            "frame_width",
            "frame_height",
            "first_frame",
            "frame_count",
            "anchor_x",
            "anchor_y",
        ]
        extra_kwargs = {
            "frame_width": {"min_value": 1, "max_value": SPRITE_FRAME_SIZE_MAX},
            "frame_height": {"min_value": 1, "max_value": SPRITE_FRAME_SIZE_MAX},
            "first_frame": {"min_value": 0, "max_value": EFFECT_FIRST_FRAME_MAX},
            "frame_count": {"min_value": 1, "max_value": SPRITE_FRAMES_MAX},
            "anchor_x": {"min_value": 0.0, "max_value": 1.0},
            "anchor_y": {"min_value": 0.0, "max_value": 1.0},
        }

    def __init__(self, *args: object, **kwargs: object) -> None:
        super().__init__(*args, **kwargs)  # type: ignore[arg-type]
        effect: Effect | None = self.context.get("effect")
        image_field = self.fields["image_id"]
        assert isinstance(image_field, serializers.PrimaryKeyRelatedField)
        image_field.queryset = EffectImage.objects.filter(effect=effect).defer("data")


class AdminEffectSerializer(serializers.ModelSerializer):
    """
    Ein Effekt mit allem, was der Adminbereich zeigt. Der Schluessel steht nach
    dem Anlegen fest (Effect).
    """

    images = AdminEffectImageSerializer(many=True, read_only=True)
    atlases = AdminEffectAtlasSerializer(many=True, read_only=True)
    complete = serializers.SerializerMethodField()
    problems = serializers.SerializerMethodField()

    class Meta:
        model = Effect
        fields = [
            "id",
            "key",
            "kind",
            "draw_width",
            "duration_ms",
            "images",
            "atlases",
            "complete",
            "problems",
            "updated_at",
        ]
        read_only_fields = ["updated_at"]

    def get_problems(self, effect: Effect) -> list[str]:
        return effect_problems(effect, list(effect.atlases.all()))

    def get_complete(self, effect: Effect) -> bool:
        return not self.get_problems(effect)

    def validate_key(self, value: str) -> str:
        if self.instance is not None and value != self.instance.key:
            raise serializers.ValidationError("Der Schluessel aendert sich nach dem Anlegen nicht mehr.")
        return value


def check_one_atlas_per_facing(entries: list[dict[str, object]]) -> None:
    """Jede Richtung hoechstens einmal (unique_atlas_per_effect_facing), mit Meldung statt IntegrityError."""
    facings: list[object] = [entry["facing"] for entry in entries]
    if len(set(facings)) != len(facings):
        raise serializers.ValidationError(f"Jede Richtung nur einmal: {', '.join(FACINGS)}.")
