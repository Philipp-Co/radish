"""
Effekte -- etwa die Explosionen -- fuer den Adminbereich (radish/frontend/src/
app/pages/games/admin/effects/): anlegen, aendern, loeschen, PNGs hochladen und
ersetzen, je Richtung einen Atlas setzen. Nur mit der Rolle "Radish-Admin"
(HasAdminRole), wie die Einheitenbilder in admin_sprite_views.py.

Ein vollstaendiger Effekt erscheint ohne weiteres Zutun im Asset-Manifest des
Clients (api/domain/effects/publishing.py, web/interfaces/sprite_views.py) --
ab dem naechsten Laden des Spiels.
"""

from typing import Any

from django.db import IntegrityError, transaction
from django.db.models import Prefetch, QuerySet
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
from api.interfaces.admin.serializers.admin_effect_serializer import (
    AdminEffectAtlasSerializer,
    AdminEffectSerializer,
    check_one_atlas_per_facing,
)
from api.interfaces.admin.serializers.admin_sprite_serializer import (
    REPLACED_PNG_FIELDS,
    SpriteImageUploadSerializer,
    replace_png,
)
from api.interfaces.admin.views.admin_sprite_views import delete_png
from api.models import Effect, EffectAtlas, EffectImage


def _effects() -> QuerySet[Effect]:
    """Die Effekte samt PNGs (ohne Bytes) und Atlanten -- eine Abfrage je Beziehung."""
    return Effect.objects.prefetch_related(
        Prefetch("images", queryset=EffectImage.objects.defer("data")),
        Prefetch("atlases", queryset=EffectAtlas.objects.select_related("image").defer("image__data")),
    )


def _effect_response(effect_id: int, status_code: int = status.HTTP_200_OK) -> Response:
    """Der Effekt frisch geladen -- mit dem Stand nach einer Aenderung."""
    return Response(AdminEffectSerializer(_effects().get(pk=effect_id)).data, status=status_code)


class AdminEffectListView(generics.ListCreateAPIView):
    """Alle Effekte unter {"effects": [...]}; POST legt einen an."""

    permission_classes = [IsAuthenticated, HasAdminRole]
    serializer_class = AdminEffectSerializer

    def get_queryset(self) -> QuerySet[Effect]:
        return _effects()

    # Any: Signatur von ListModelMixin.list in den DRF-Stubs.
    def list(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        return Response({"effects": self.get_serializer(self.get_queryset(), many=True).data})

    # Any: Signatur von CreateModelMixin.create in den DRF-Stubs.
    def create(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        serializer: AdminEffectSerializer = AdminEffectSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        effect: Effect = serializer.save()
        return _effect_response(effect.pk, status.HTTP_201_CREATED)


class AdminEffectDetailView(generics.RetrieveUpdateDestroyAPIView):
    """
    Ein Effekt. Aendern betrifft seine Werte (Art, Breite, Dauer) -- der
    Schluessel steht fest. Loeschen nimmt seine PNGs und Atlanten mit; kein
    Katalogeintrag nennt ihn.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]
    serializer_class = AdminEffectSerializer

    def get_queryset(self) -> QuerySet[Effect]:
        return _effects()

    # Any: Signatur von UpdateModelMixin.update in den DRF-Stubs.
    def update(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        serializer: AdminEffectSerializer = AdminEffectSerializer(
            self.get_object(), data=request.data, partial=kwargs.pop("partial", False)
        )
        serializer.is_valid(raise_exception=True)
        effect: Effect = serializer.save()
        return _effect_response(effect.pk)

    def perform_destroy(self, instance: Effect) -> None:
        with transaction.atomic():
            # Erst die Atlanten: sie schuetzen ihre PNGs (PROTECT).
            instance.atlases.all().delete()
            instance.delete()


class AdminEffectImageListView(APIView):
    """Laedt eine PNG zu einem Effekt hoch (multipart: "file", wahlweise "name")."""

    permission_classes = [IsAuthenticated, HasAdminRole]
    parser_classes = [MultiPartParser, FormParser]

    def post(self, request: Request, effect_id: int) -> Response:
        effect: Effect = get_object_or_404(Effect, pk=effect_id)
        upload: SpriteImageUploadSerializer = SpriteImageUploadSerializer(data=request.data)
        upload.is_valid(raise_exception=True)
        try:
            with transaction.atomic():
                EffectImage.objects.create(effect=effect, **upload.validated_data)
        except IntegrityError as exc:
            name: str = upload.validated_data["name"]
            raise ValidationError(
                {"name": f"„{name}“ gibt es bei diesem Effekt schon -- ersetzen statt neu hochladen."}
            ) from exc
        return _effect_response(effect.pk, status.HTTP_201_CREATED)


class AdminEffectImageDetailView(APIView):
    """
    Eine PNG eines Effekts: PUT ersetzt ihren Inhalt (multipart: "file"), der
    Name bleibt; DELETE loescht sie, solange kein Atlas sie zeigt (sonst 409).
    """

    permission_classes = [IsAuthenticated, HasAdminRole]
    parser_classes = [MultiPartParser, FormParser]

    def put(self, request: Request, effect_id: int, image_id: int) -> Response:
        image: EffectImage = get_object_or_404(EffectImage, pk=image_id, effect_id=effect_id)
        replace_png(image, request.data.get("file"))
        with transaction.atomic():
            image.save(update_fields=REPLACED_PNG_FIELDS)
            image.effect.save(update_fields=["updated_at"])
        return _effect_response(effect_id)

    def delete(self, request: Request, effect_id: int, image_id: int) -> Response:
        return delete_png(get_object_or_404(EffectImage, pk=image_id, effect_id=effect_id))


class AdminEffectImageFileView(APIView):
    """
    Die Bytes einer PNG, fuer die Vorschau im Adminbereich -- auch fuer einen
    Effekt, der noch nicht vollstaendig ist und deshalb nicht unter /wasm/ steht.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]

    def get(self, request: Request, effect_id: int, image_id: int) -> HttpResponse:
        image: EffectImage = get_object_or_404(EffectImage, pk=image_id, effect_id=effect_id)
        response: HttpResponse = HttpResponse(bytes(image.data), content_type="image/png")
        response["Cache-Control"] = "private, no-cache"
        return response


class AdminEffectAtlasesView(APIView):
    """
    Setzt die Atlanten eines Effekts auf einmal (PUT, eine Liste wie in
    AdminEffectAtlasSerializer) -- was fehlt, gibt es danach nicht mehr. Ein
    unvollstaendiger Satz ist erlaubt; was noch fehlt, steht in "problems".
    """

    permission_classes = [IsAuthenticated, HasAdminRole]

    def put(self, request: Request, effect_id: int) -> Response:
        effect: Effect = get_object_or_404(Effect, pk=effect_id)
        serializer: AdminEffectAtlasSerializer = AdminEffectAtlasSerializer(
            data=request.data, many=True, context={"effect": effect}
        )
        serializer.is_valid(raise_exception=True)
        entries: list[dict[str, object]] = serializer.validated_data
        check_one_atlas_per_facing(entries)

        with transaction.atomic():
            effect.atlases.all().delete()
            EffectAtlas.objects.bulk_create(EffectAtlas(effect=effect, **entry) for entry in entries)
            effect.save(update_fields=["updated_at"])
        return _effect_response(effect.pk)
