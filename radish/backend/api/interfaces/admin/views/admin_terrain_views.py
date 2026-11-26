"""
Wie die Tile-Typen im Spiel aussehen, fuer den Adminbereich (radish/frontend/
src/app/pages/games/admin/terrain/): PNGs je Typ hochladen, ersetzen und
loeschen, dazu Bild und Dekorationen setzen. Nur mit der Rolle "Radish-Admin"
(HasAdminRole), wie die Werte der Typen in admin_views.py.

Jeder gespeicherte Stand erscheint ohne weiteres Zutun im Asset-Manifest des
Clients (api/domain/tiles/publishing.py, web/interfaces/sprite_views.py) --
ab dem naechsten Laden des Spiels.
"""

from django.db import IntegrityError, transaction
from django.db.models import ProtectedError
from django.http import HttpResponse
from django.shortcuts import get_object_or_404
from rest_framework import status
from rest_framework.exceptions import ValidationError
from rest_framework.parsers import FormParser, MultiPartParser
from rest_framework.permissions import IsAuthenticated
from rest_framework.request import Request
from rest_framework.response import Response
from rest_framework.views import APIView

from api.interfaces.admin.roles.has_admin_role import HasAdminRole
from api.interfaces.admin.serializers.admin_sprite_serializer import (
    REPLACED_PNG_FIELDS,
    SpriteImageUploadSerializer,
    replace_png,
)
from api.interfaces.admin.serializers.admin_terrain_type_serializer import (
    AdminTerrainLookSerializer,
    AdminTerrainTypeSerializer,
    terrain_for_admin,
)
from api.interfaces.admin.views.admin_views import ConflictError
from api.models import TerrainDecor, TerrainImage, TerrainType


def _terrain_response(code: str, status_code: int = status.HTTP_200_OK) -> Response:
    """Der Typ frisch geladen -- mit dem Stand nach einer Aenderung."""
    return Response(AdminTerrainTypeSerializer(terrain_for_admin().get(code=code)).data, status=status_code)


class AdminTerrainImageListView(APIView):
    """Laedt eine PNG zu einem Typ hoch (multipart: "file", wahlweise "name")."""

    permission_classes = [IsAuthenticated, HasAdminRole]
    parser_classes = [MultiPartParser, FormParser]

    def post(self, request: Request, code: str) -> Response:
        terrain: TerrainType = get_object_or_404(TerrainType, code=code)
        upload: SpriteImageUploadSerializer = SpriteImageUploadSerializer(data=request.data)
        upload.is_valid(raise_exception=True)
        try:
            with transaction.atomic():
                TerrainImage.objects.create(terrain=terrain, **upload.validated_data)
        except IntegrityError as exc:
            name: str = upload.validated_data["name"]
            raise ValidationError(
                {"name": f"„{name}“ gibt es bei diesem Typ schon -- ersetzen statt neu hochladen."}
            ) from exc
        return _terrain_response(code, status.HTTP_201_CREATED)


class AdminTerrainImageDetailView(APIView):
    """
    Eine PNG eines Typs: PUT ersetzt ihren Inhalt (multipart: "file"), der Name
    bleibt; DELETE loescht sie, solange der Typ sie nicht zeigt -- weder als
    Grundbild noch als Dekoration (sonst 409).
    """

    permission_classes = [IsAuthenticated, HasAdminRole]
    parser_classes = [MultiPartParser, FormParser]

    def put(self, request: Request, code: str, image_id: int) -> Response:
        image: TerrainImage = get_object_or_404(TerrainImage, pk=image_id, terrain__code=code)
        replace_png(image, request.data.get("file"))
        image.save(update_fields=REPLACED_PNG_FIELDS)
        return _terrain_response(code)

    def delete(self, request: Request, code: str, image_id: int) -> Response:
        image: TerrainImage = get_object_or_404(TerrainImage, pk=image_id, terrain__code=code)
        shown: str = f"{image.name} wird noch als Grundbild oder Dekoration gezeigt und kann nicht geloescht werden."
        if image.is_base:
            raise ConflictError(shown)
        try:
            image.delete()
        except ProtectedError as exc:
            raise ConflictError(shown) from exc
        return Response(status=status.HTTP_204_NO_CONTENT)


class AdminTerrainImageFileView(APIView):
    """
    Die Bytes einer PNG, fuer die Vorschau im Adminbereich -- auch fuer eine,
    die der Typ (noch) nicht zeigt und die deshalb nicht unter /wasm/ steht.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]

    def get(self, request: Request, code: str, image_id: int) -> HttpResponse:
        image: TerrainImage = get_object_or_404(TerrainImage, pk=image_id, terrain__code=code)
        response: HttpResponse = HttpResponse(bytes(image.data), content_type="image/png")
        response["Cache-Control"] = "private, no-cache"
        return response


class AdminTerrainLookView(APIView):
    """
    Setzt Bild und Dekorationen eines Typs auf einmal (PUT, wie in
    AdminTerrainLookSerializer). Die Dekorationen bekommen ihre Ebene (layer) aus der
    Reihenfolge der Liste.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]

    def put(self, request: Request, code: str) -> Response:
        terrain: TerrainType = get_object_or_404(TerrainType, code=code)
        serializer: AdminTerrainLookSerializer = AdminTerrainLookSerializer(
            data=request.data, context={"terrain": terrain}
        )
        serializer.is_valid(raise_exception=True)
        image: TerrainImage | None = serializer.validated_data["image_id"]
        entries: list[dict[str, object]] = serializer.validated_data["decor"]

        with transaction.atomic():
            terrain.decor.all().delete()
            TerrainDecor.objects.bulk_create(
                TerrainDecor(terrain=terrain, layer=layer, **entry) for layer, entry in enumerate(entries)
            )
            # Erst alle zuruecksetzen: es darf nie zwei Grundbilder geben (one_base_image_per_terrain).
            terrain.images.update(is_base=False)
            if image is not None:
                TerrainImage.objects.filter(pk=image.pk).update(is_base=True)
        return _terrain_response(code)
