"""
Hochgeladene Einheitenbilder fuer den Adminbereich (radish/frontend/src/app/
pages/games/admin/sprites/): anlegen, aendern, loeschen, PNGs hochladen und
ersetzen, Atlanten setzen. Nur mit der Rolle "Radish-Admin" (HasAdminRole), wie
der Katalog in admin_views.py.

Ein vollstaendiges Bild erscheint ohne weiteres Zutun im Asset-Manifest des
Clients (api/domain/sprites/publishing.py, web/interfaces/sprite_views.py) --
damit in der Bild-Auswahl der Einheitentypen und im Spiel.
"""

from typing import Any, cast

from django.db import IntegrityError, transaction
from django.db.models import Prefetch, ProtectedError, QuerySet
from django.http import HttpResponse
from django.shortcuts import get_object_or_404
from rest_framework import generics, status
from rest_framework.exceptions import ValidationError
from rest_framework.parsers import FormParser, MultiPartParser
from rest_framework.permissions import IsAuthenticated
from rest_framework.request import Request
from rest_framework.response import Response
from rest_framework.views import APIView

from api.interfaces.admin.roles.has_admin_role import HasAdminRole
from api.interfaces.admin.serializers.admin_sprite_serializer import (
    AdminSpriteAtlasSerializer,
    AdminSpriteSerializer,
    SpriteImageUploadSerializer,
    UploadedPng,
)
from api.interfaces.admin.views.admin_views import ConflictError
from api.models import UnitSprite, UnitSpriteAtlas, UnitSpriteImage, UnitType


def _sprites() -> QuerySet[UnitSprite]:
    """Die Bilder samt PNGs (ohne Bytes) und Atlanten -- eine Abfrage je Beziehung."""
    return UnitSprite.objects.prefetch_related(
        Prefetch("images", queryset=UnitSpriteImage.objects.defer("data")),
        Prefetch("atlases", queryset=UnitSpriteAtlas.objects.select_related("image").defer("image__data")),
    )


def _sprite_response(sprite_id: int, status_code: int = status.HTTP_200_OK) -> Response:
    """Das Bild frisch geladen -- mit dem Stand nach einer Aenderung."""
    return Response(AdminSpriteSerializer(_sprites().get(pk=sprite_id)).data, status=status_code)


class AdminSpriteListView(generics.ListCreateAPIView):
    """Alle hochgeladenen Bilder unter {"sprites": [...]}; POST legt eines an."""

    permission_classes = [IsAuthenticated, HasAdminRole]
    serializer_class = AdminSpriteSerializer

    def get_queryset(self) -> QuerySet[UnitSprite]:
        return _sprites()

    # Any: Signatur von ListModelMixin.list in den DRF-Stubs.
    def list(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        return Response({"sprites": self.get_serializer(self.get_queryset(), many=True).data})

    # Any: Signatur von CreateModelMixin.create in den DRF-Stubs.
    def create(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        serializer: AdminSpriteSerializer = AdminSpriteSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        sprite: UnitSprite = serializer.save()
        return _sprite_response(sprite.pk, status.HTTP_201_CREATED)


class AdminSpriteDetailView(generics.RetrieveUpdateDestroyAPIView):
    """
    Ein Bild. Aendern betrifft seine Werte (Anker, Breite, Schatten, Zeiten) --
    der Schluessel steht fest. Loeschen nur, solange kein Einheitentyp es nennt
    (sonst 409); seine PNGs und Atlanten gehen mit.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]
    serializer_class = AdminSpriteSerializer

    def get_queryset(self) -> QuerySet[UnitSprite]:
        return _sprites()

    # Any: Signatur von UpdateModelMixin.update in den DRF-Stubs.
    def update(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        serializer: AdminSpriteSerializer = AdminSpriteSerializer(
            self.get_object(), data=request.data, partial=kwargs.pop("partial", False)
        )
        serializer.is_valid(raise_exception=True)
        sprite: UnitSprite = serializer.save()
        return _sprite_response(sprite.pk)

    def perform_destroy(self, instance: UnitSprite) -> None:
        if UnitType.objects.filter(sprite=instance.key).exists():
            raise ConflictError(
                f"„{instance.key}“ wird noch von Einheitentypen verwendet und kann nicht geloescht werden."
            )
        with transaction.atomic():
            # Erst die Atlanten: sie schuetzen ihre PNGs (PROTECT).
            instance.atlases.all().delete()
            instance.delete()


class AdminSpriteImageListView(APIView):
    """Laedt eine PNG zu einem Bild hoch (multipart: "file", wahlweise "name")."""

    permission_classes = [IsAuthenticated, HasAdminRole]
    parser_classes = [MultiPartParser, FormParser]

    def post(self, request: Request, sprite_id: int) -> Response:
        sprite: UnitSprite = get_object_or_404(UnitSprite, pk=sprite_id)
        upload: SpriteImageUploadSerializer = SpriteImageUploadSerializer(data=request.data)
        upload.is_valid(raise_exception=True)
        try:
            with transaction.atomic():
                UnitSpriteImage.objects.create(sprite=sprite, **upload.validated_data)
        except IntegrityError as exc:
            name: str = upload.validated_data["name"]
            raise ValidationError(
                {"name": f"„{name}“ gibt es bei diesem Bild schon -- ersetzen statt neu hochladen."}
            ) from exc
        return _sprite_response(sprite.pk, status.HTTP_201_CREATED)


class AdminSpriteImageDetailView(APIView):
    """
    Eine PNG eines Bildes: PUT ersetzt ihren Inhalt (multipart: "file"), der Name
    bleibt; DELETE loescht sie, solange kein Atlas sie zeigt (sonst 409).
    """

    permission_classes = [IsAuthenticated, HasAdminRole]
    parser_classes = [MultiPartParser, FormParser]

    def put(self, request: Request, sprite_id: int, image_id: int) -> Response:
        image: UnitSpriteImage = get_object_or_404(UnitSpriteImage, pk=image_id, sprite_id=sprite_id)
        upload: SpriteImageUploadSerializer = SpriteImageUploadSerializer(
            data={"file": request.data.get("file"), "name": image.name}
        )
        upload.is_valid(raise_exception=True)
        data: UploadedPng = cast(UploadedPng, upload.validated_data)
        image.data = data["data"]
        image.sha256 = data["sha256"]
        image.width = data["width"]
        image.height = data["height"]
        with transaction.atomic():
            image.save(update_fields=["data", "sha256", "width", "height"])
            image.sprite.save(update_fields=["updated_at"])
        return _sprite_response(sprite_id)

    def delete(self, request: Request, sprite_id: int, image_id: int) -> Response:
        image: UnitSpriteImage = get_object_or_404(UnitSpriteImage, pk=image_id, sprite_id=sprite_id)
        try:
            image.delete()
        except ProtectedError as exc:
            raise ConflictError(
                f"{image.name} wird noch von einem Atlas gezeigt und kann nicht geloescht werden."
            ) from exc
        return Response(status=status.HTTP_204_NO_CONTENT)


class AdminSpriteImageFileView(APIView):
    """
    Die Bytes einer PNG, fuer die Vorschau im Adminbereich -- auch fuer ein Bild,
    das noch nicht vollstaendig ist und deshalb nicht unter /wasm/ steht. Mit
    Rolle, also nicht per <img src>, sondern ueber den Client mit Token.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]

    def get(self, request: Request, sprite_id: int, image_id: int) -> HttpResponse:
        image: UnitSpriteImage = get_object_or_404(UnitSpriteImage, pk=image_id, sprite_id=sprite_id)
        response: HttpResponse = HttpResponse(bytes(image.data), content_type="image/png")
        response["Cache-Control"] = "private, no-cache"
        return response


class AdminSpriteAtlasesView(APIView):
    """
    Setzt alle Atlanten eines Bildes auf einmal (PUT, eine Liste wie in
    AdminSpriteAtlasSerializer) -- was fehlt, gibt es danach nicht mehr. Ein
    unvollstaendiger Satz ist erlaubt; was noch fehlt, steht in "problems".
    """

    permission_classes = [IsAuthenticated, HasAdminRole]

    def put(self, request: Request, sprite_id: int) -> Response:
        sprite: UnitSprite = get_object_or_404(UnitSprite, pk=sprite_id)
        serializer: AdminSpriteAtlasSerializer = AdminSpriteAtlasSerializer(
            data=request.data, many=True, context={"sprite": sprite}
        )
        serializer.is_valid(raise_exception=True)
        entries: list[dict[str, object]] = serializer.validated_data
        slots: list[tuple[object, object]] = [(e["animation"], e.get("facing", "")) for e in entries]
        if len(set(slots)) != len(slots):
            raise ValidationError("Jede Animation und Richtung nur einmal.")

        with transaction.atomic():
            sprite.atlases.all().delete()
            UnitSpriteAtlas.objects.bulk_create(UnitSpriteAtlas(sprite=sprite, **entry) for entry in entries)
            sprite.save(update_fields=["updated_at"])
        return _sprite_response(sprite.pk)
