"""
Hochgeladene Einheitenbilder im Adminbereich (siehe admin_sprite_views.py):
das Bild mit seinen Werten, seinen PNGs und Atlanten, dazu, ob es vollstaendig
ist und was ihm fehlt (api/domain/sprites/problems.py), und welche
Einheitentypen es nennen.
"""

import hashlib
import re
from typing import TypedDict

from django.core.files.uploadedfile import UploadedFile
from rest_framework import serializers

from api.domain.sprites.builtin import built_sprite_keys
from api.domain.sprites.png import InvalidPng, png_size
from api.domain.sprites.problems import FACINGS, sprite_problems
from api.models import (
    SPRITE_FILE_NAME_PATTERN,
    SPRITE_FRAME_SIZE_MAX,
    SPRITE_FRAMES_MAX,
    SPRITE_IMAGE_BYTES_MAX,
    SpriteAnimation,
    SpriteFacing,
    UnitSprite,
    UnitSpriteAtlas,
    UnitSpriteImage,
    UnitType,
)


class AdminSpriteImageSerializer(serializers.ModelSerializer):
    """Eine PNG, ohne ihre Bytes -- die liefert admin_sprite_views.AdminSpriteImageFileView."""

    class Meta:
        model = UnitSpriteImage
        fields = ["id", "name", "width", "height", "sha256"]
        read_only_fields = fields


class AdminSpriteAtlasSerializer(serializers.ModelSerializer):
    """
    Ein Atlas. Geprueft wird hier nur, was jeder Atlas fuer sich braucht --
    gueltige Animation und Richtung, eine PNG dieses Bildes, Zahlen im Rahmen.
    Ob er vollstaendig ist und zur PNG passt, sagt "problems" am Bild: ein
    Entwurf darf unfertig gespeichert werden.

    Erwartet im Kontext "sprite".
    """

    image_id = serializers.PrimaryKeyRelatedField(source="image", queryset=UnitSpriteImage.objects.none())
    key_frames = serializers.ListField(
        child=serializers.IntegerField(min_value=0, max_value=SPRITE_FRAMES_MAX - 1),
        allow_null=True,
        required=False,
        max_length=len(FACINGS),
    )

    class Meta:
        model = UnitSpriteAtlas
        fields = ["animation", "facing", "image_id", "frame_width", "frame_height", "frame_count", "key_frames", "fps"]
        extra_kwargs = {
            "frame_width": {"min_value": 1, "max_value": SPRITE_FRAME_SIZE_MAX},
            "frame_height": {"min_value": 1, "max_value": SPRITE_FRAME_SIZE_MAX},
            "frame_count": {"min_value": 1, "max_value": SPRITE_FRAMES_MAX},
            "facing": {"allow_blank": True, "required": False},
        }

    def __init__(self, *args: object, **kwargs: object) -> None:
        super().__init__(*args, **kwargs)  # type: ignore[arg-type]
        sprite: UnitSprite | None = self.context.get("sprite")
        image_field = self.fields["image_id"]
        assert isinstance(image_field, serializers.PrimaryKeyRelatedField)
        image_field.queryset = UnitSpriteImage.objects.filter(sprite=sprite).defer("data")

    # Die Stubs typisieren attrs als Any; es sind die validierten Daten.
    def validate(self, attrs: dict[str, object]) -> dict[str, object]:
        animation: object = attrs.get("animation")
        facing: object = attrs.get("facing", SpriteFacing.NONE)
        if animation == SpriteAnimation.TURN and facing != SpriteFacing.NONE:
            raise serializers.ValidationError({"facing": "Die Drehung hat einen Atlas fuer alle Richtungen."})
        if animation != SpriteAnimation.TURN and facing not in FACINGS:
            raise serializers.ValidationError({"facing": "Braucht eine Richtung: sw, se, ne oder nw."})
        return attrs


class AdminSpriteSerializer(serializers.ModelSerializer):
    """
    Ein Bild mit allem, was der Adminbereich zeigt. Der Schluessel steht nach
    dem Anlegen fest (UnitSprite) und darf keiner aus dem Build sein.
    """

    images = AdminSpriteImageSerializer(many=True, read_only=True)
    atlases = AdminSpriteAtlasSerializer(many=True, read_only=True)
    complete = serializers.SerializerMethodField()
    problems = serializers.SerializerMethodField()
    used_by = serializers.SerializerMethodField()

    class Meta:
        model = UnitSprite
        fields = [
            "id",
            "key",
            "anchor_x",
            "anchor_y",
            "draw_width",
            "own_shadow",
            "step_ms",
            "turn_ms",
            "aim_ms",
            "aim_hold_ms",
            "images",
            "atlases",
            "complete",
            "problems",
            "used_by",
            "updated_at",
        ]
        read_only_fields = ["updated_at"]

    def get_problems(self, sprite: UnitSprite) -> list[str]:
        return sprite_problems(sprite, list(sprite.atlases.all()))

    def get_complete(self, sprite: UnitSprite) -> bool:
        return not self.get_problems(sprite)

    def get_used_by(self, sprite: UnitSprite) -> list[str]:
        """Die Einheitentypen, die dieses Bild nennen, als "Name (Spezies)"."""
        return [str(unit_type) for unit_type in UnitType.objects.filter(sprite=sprite.key).select_related("species")]

    def validate_key(self, value: str) -> str:
        if self.instance is not None and value != self.instance.key:
            raise serializers.ValidationError("Der Schluessel aendert sich nach dem Anlegen nicht mehr.")
        if self.instance is None and value in built_sprite_keys():
            raise serializers.ValidationError(
                f"„{value}“ ist schon ein Bild aus dem Build (radish/assets/units/) -- bitte einen anderen Schluessel."
            )
        return value


class UploadedPng(TypedDict):
    """validated_data von SpriteImageUploadSerializer."""

    name: str
    data: bytes
    sha256: str
    width: int
    height: int


class SpriteImageUploadSerializer(serializers.Serializer):
    """
    Eine hochgeladene PNG: "file", dazu wahlweise "name" -- sonst der Dateiname
    des Uploads, klein geschrieben. Prueft Groesse, Namen und den Kopf der PNG
    und liefert in validated_data name, data, sha256, width und height.
    """

    file = serializers.FileField()
    name = serializers.CharField(required=False, allow_blank=True)

    # Die Stubs typisieren attrs als Any; es sind die validierten Daten.
    def validate(self, attrs: dict[str, object]) -> dict[str, object]:
        upload: object = attrs["file"]
        assert isinstance(upload, UploadedFile)
        data: bytes = _checked_data(upload)
        name: str = _checked_name(attrs.get("name"), upload)
        try:
            width, height = png_size(data)
        except InvalidPng as exc:
            raise serializers.ValidationError({"file": str(exc)}) from exc

        uploaded: UploadedPng = {
            "name": name,
            "data": data,
            "sha256": hashlib.sha256(data).hexdigest(),
            "width": width,
            "height": height,
        }
        # Die Stubs erwarten dict[str, object]; ein TypedDict ist eines.
        return dict(uploaded)


def _checked_data(upload: UploadedFile) -> bytes:
    """Die Bytes des Uploads, wenn er nicht zu gross ist."""
    if (upload.size or 0) > SPRITE_IMAGE_BYTES_MAX:
        megabytes: int = SPRITE_IMAGE_BYTES_MAX // (1024 * 1024)
        raise serializers.ValidationError({"file": f"Hoechstens {megabytes} MB je Datei."})
    return upload.read()


def _checked_name(given: object, upload: UploadedFile) -> str:
    """Der Dateiname im Ordner des Bildes: "given" oder der des Uploads, klein geschrieben."""
    name: str = (given if isinstance(given, str) and given else (upload.name or "")).strip().lower()
    if not re.fullmatch(SPRITE_FILE_NAME_PATTERN, name):
        raise serializers.ValidationError(
            {"name": f"„{name}“ geht nicht: nur a-z, 0-9 und _, mit .png, hoechstens 31 Zeichen."}
        )
    return name
