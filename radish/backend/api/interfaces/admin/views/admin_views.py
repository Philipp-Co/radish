"""
Endpunkte fuer den Adminbereich des Frontends (siehe radish/frontend/src/app/
pages/games/admin/) -- getrennt von client_views.py (Spieler-Endpunkte) und
server_views.py (Registrierung durch den technischen Server-Account): hier
geht es ausschliesslich um menschliche Admins mit der Keycloak-Realm-Rolle
"Radish-Admin" (siehe has_admin_role.HasAdminRole) -- die Serverliste nur
lesend, der Armee-Katalog (Spezies, Einheitentypen mit ihren Profilen,
Waffen, Ausruestung) unter admin/catalog/ auch schreibend, ebenso die Werte
der Tile-Typen unter admin/terrain/.
"""

from typing import TYPE_CHECKING, Any, Generic, TypeVar

from django.db.models import Exists, Model, OuterRef, Prefetch, QuerySet
from django.db.models.deletion import ProtectedError
from rest_framework import generics, status
from rest_framework.exceptions import APIException, ValidationError
from rest_framework.permissions import IsAuthenticated
from rest_framework.request import Request
from rest_framework.response import Response
from rest_framework.serializers import BaseSerializer
from rest_framework.utils.serializer_helpers import ReturnDict
from rest_framework.views import APIView

from ....models import (
    EntityProfile,
    Equipment,
    GameServer,
    Species,
    TerrainType,
    Unit,
    UnitEntityEquipment,
    UnitEntityWeapon,
    UnitType,
    Weapon,
)
from ..roles.has_admin_role import HasAdminRole
from ...servers.serializers.game_server_serializer import GameServerSerializer
from ..serializers.admin_entity_profile_serializer import AdminEntityProfileSerializer
from ..serializers.admin_equipment_serializer import AdminEquipmentSerializer
from ..serializers.admin_species_serializer import AdminSpeciesSerializer
from ..serializers.admin_terrain_type_serializer import AdminTerrainTypeSerializer
from ..serializers.admin_unit_type_serializer import AdminUnitTypeSerializer
from ..serializers.admin_weapon_serializer import AdminWeaponSerializer


class AdminServerListView(APIView):
    """
    Listet alle aktuell angemeldeten Game-Server auf (siehe
    server_views.ServerView, das dieselben Server registriert/entfernt).

    Eigener Endpunkt statt Wiederverwendung von GET /api/servers/: Letzterer
    ist fuer den technischen Server-Account gedacht (nur IsAuthenticated,
    siehe dortigen Docstring) und bewusst nicht an eine Rolle geknuepft --
    hier soll stattdessen ausschliesslich "Radish-Admin" herankommen.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]

    def get(self, request: Request) -> Response:
        servers: QuerySet[GameServer] = GameServer.objects.all().order_by("name")
        serializer: GameServerSerializer = GameServerSerializer(servers, many=True)
        return Response({"servers": serializer.data})


# ---------------------------------------------------------------------------
# Armee-Katalog
# ---------------------------------------------------------------------------

# Ob ein Katalogeintrag in mindestens einer Armee steckt -- als Annotation
# "in_use", die catalog_admin_serializer._CatalogAdminSerializer fuer die Sperre der
# Strukturfelder liest und die das Frontend zum Ausgrauen nutzt. Ein Profil
# gilt schon als verwendet, wenn sein Einheitentyp es ist: seine Anzahl-
# grenzen betreffen jede Einheit des Typs, auch ohne Entitaet mit dem Profil.
IN_USE: dict[type[Model], Exists] = {
    UnitType: Exists(Unit.objects.filter(unit_type=OuterRef("pk"))),
    EntityProfile: Exists(Unit.objects.filter(unit_type=OuterRef("unit_type"))),
    Weapon: Exists(UnitEntityWeapon.objects.filter(weapon=OuterRef("pk"))),
    Equipment: Exists(UnitEntityEquipment.objects.filter(equipment=OuterRef("pk"))),
}


class Conflict(APIException):
    status_code = status.HTTP_409_CONFLICT
    default_code = "conflict"


# Das Katalog-Model einer View (Species, UnitType, ...).
_M = TypeVar("_M", bound=Model)

# Der Mixin wird nur zusammen mit den generischen DRF-Views verwendet und ruft
# deren get_serializer/get_object auf -- fuer mypy deshalb als GenericAPIView,
# zur Laufzeit bleibt er ein reiner Mixin. Dort steht Generic statt object
# als Basis, damit _CatalogAdminMixin[Species] auch zur Laufzeit geht;
# Generic bringt kein eigenes Verhalten mit.
if TYPE_CHECKING:
    _Base = generics.GenericAPIView
else:
    _Base = Generic


class _CatalogAdminMixin(_Base[_M]):
    """
    Gemeinsames fuer die Katalog-Endpunkte: Rolle, in_use-Annotation, fuer
    die Liste die Filter per Query-Parameter (z.B. ?species=3) und die Suche
    im Namen (?search=gew, ohne Gross-/Kleinschreibung), die Listenantwort unter
    einem eigenen Schluessel wie bei AdminServerListView ({"servers": [...]})
    und 409 statt 500, wenn ein Eintrag noch verwendet wird.

    Nach Anlegen und Aendern wird der Eintrag neu geladen, damit die Antwort
    in_use (und bei Einheitentypen die Profile) wieder korrekt enthaelt.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]
    # Setzt jede konkrete View, deshalb ohne Default (None passte nicht zum Typ).
    model: type[_M]
    list_key: str | None = None
    # Query-Parameter -> Feld, z.B. {"species": "species_id"}.
    filters: dict[str, str] = {}
    # Query-Parameter -> Feld fuer Auswahlfelder, z.B. ?weapon_class=heavy.
    choice_filters: dict[str, str] = {}
    # Fuer species_name/unit_type_name in der Antwort (Kontext in der Suche).
    related: tuple[str, ...] = ()

    def get_queryset(self) -> QuerySet[_M]:
        # _default_manager statt objects: type[Model] kennt in den Stubs kein
        # objects; die Katalog-Models haben keinen eigenen Manager, beides ist
        # also derselbe.
        # _default_manager ist dokumentierte Django-API, trotz Unterstrich.
        # pylint: disable-next=protected-access
        queryset: QuerySet[_M] = self.model._default_manager.select_related(*self.related)
        if self.model in IN_USE:
            queryset = queryset.annotate(in_use=IN_USE[self.model])
        return queryset

    # Any: Signatur von ListModelMixin.list/CreateModelMixin.create/
    # UpdateModelMixin.update in den DRF-Stubs; args/kwargs sind die
    # URL-Parameter, die DRF ungeprueft durchreicht.
    def list(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        queryset: QuerySet[_M] = self.get_queryset()
        queryset = self._filter_by_ids(request, queryset)
        queryset = self._filter_by_choices(request, queryset)
        search: str = request.query_params.get("search", "").strip()
        if search:
            queryset = queryset.filter(name__icontains=search)
        serializer: BaseSerializer[_M] = self.get_serializer(queryset, many=True)
        return Response({self.list_key: serializer.data})

    def _filter_by_ids(self, request: Request, queryset: QuerySet[_M]) -> QuerySet[_M]:
        """Filter aus self.filters; der Wert muss eine id (nur Ziffern) sein."""
        param: str
        field: str
        for param, field in self.filters.items():
            value: str | None = request.query_params.get(param)
            if value is not None:
                if not value.isdigit():
                    raise ValidationError({param: "Muss eine id sein."})
                queryset = queryset.filter(**{field: int(value)})
        return queryset

    def _filter_by_choices(self, request: Request, queryset: QuerySet[_M]) -> QuerySet[_M]:
        """Filter aus self.choice_filters; leere Werte werden ignoriert."""
        param: str
        field: str
        for param, field in self.choice_filters.items():
            value: str | None = request.query_params.get(param)
            if value:
                queryset = queryset.filter(**{field: value})
        return queryset

    # Any: DRF-Signatur, siehe list oben.
    def create(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        serializer: BaseSerializer[_M] = self.get_serializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        instance: _M = serializer.save()
        return Response(self._reloaded(instance), status=status.HTTP_201_CREATED)

    # Any: DRF-Signatur, siehe list oben.
    def update(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        serializer: BaseSerializer[_M] = self.get_serializer(
            self.get_object(), data=request.data, partial=kwargs.pop("partial", False)
        )
        serializer.is_valid(raise_exception=True)
        instance: _M = serializer.save()
        return Response(self._reloaded(instance))

    def perform_destroy(self, instance: _M) -> None:
        try:
            instance.delete()
        except ProtectedError as exc:
            raise Conflict(
                f"{instance} wird noch verwendet (in Armeen oder von anderen "
                "Katalogeintraegen) und kann nicht geloescht werden."
            ) from exc

    # Ein einzelnes Objekt eines ModelSerializers: data ist zur Laufzeit ein
    # ReturnDict (die Stubs geben fuer BaseSerializer.data nur Any an).
    def _reloaded(self, instance: _M) -> ReturnDict[str, object]:
        return self.get_serializer(self.get_queryset().get(pk=instance.pk)).data


class AdminSpeciesListView(_CatalogAdminMixin[Species], generics.ListCreateAPIView):
    model = Species
    list_key = "species"
    serializer_class = AdminSpeciesSerializer


class AdminSpeciesDetailView(_CatalogAdminMixin[Species], generics.RetrieveUpdateDestroyAPIView):
    model = Species
    serializer_class = AdminSpeciesSerializer


class _UnitTypeAdminMixin(_CatalogAdminMixin[UnitType]):
    model = UnitType
    serializer_class = AdminUnitTypeSerializer
    related = ("species",)

    def get_queryset(self) -> QuerySet[UnitType]:
        return super().get_queryset().prefetch_related(
            Prefetch(
                "profiles",
                queryset=EntityProfile.objects.select_related("unit_type__species").annotate(
                    in_use=IN_USE[EntityProfile]
                ),
            )
        )


class AdminUnitTypeListView(_UnitTypeAdminMixin, generics.ListCreateAPIView):
    list_key = "unit_types"
    filters = {"species": "species_id"}


class AdminUnitTypeDetailView(_UnitTypeAdminMixin, generics.RetrieveUpdateDestroyAPIView):
    pass


class AdminEntityProfileListView(_CatalogAdminMixin[EntityProfile], generics.ListCreateAPIView):
    model = EntityProfile
    list_key = "profiles"
    filters = {"unit_type": "unit_type_id", "species": "unit_type__species_id"}
    related = ("unit_type__species",)
    serializer_class = AdminEntityProfileSerializer


class AdminEntityProfileDetailView(_CatalogAdminMixin[EntityProfile], generics.RetrieveUpdateDestroyAPIView):
    model = EntityProfile
    related = ("unit_type__species",)
    serializer_class = AdminEntityProfileSerializer


class AdminWeaponListView(_CatalogAdminMixin[Weapon], generics.ListCreateAPIView):
    model = Weapon
    list_key = "weapons"
    filters = {"species": "species_id"}
    choice_filters = {"weapon_class": "weapon_class"}
    related = ("species",)
    serializer_class = AdminWeaponSerializer


class AdminWeaponDetailView(_CatalogAdminMixin[Weapon], generics.RetrieveUpdateDestroyAPIView):
    model = Weapon
    related = ("species",)
    serializer_class = AdminWeaponSerializer


class AdminEquipmentListView(_CatalogAdminMixin[Equipment], generics.ListCreateAPIView):
    model = Equipment
    list_key = "equipment"
    filters = {"species": "species_id"}
    related = ("species",)
    serializer_class = AdminEquipmentSerializer


class AdminEquipmentDetailView(_CatalogAdminMixin[Equipment], generics.RetrieveUpdateDestroyAPIView):
    model = Equipment
    related = ("species",)
    serializer_class = AdminEquipmentSerializer


# ---------------------------------------------------------------------------
# Gelaende
# ---------------------------------------------------------------------------


class AdminTerrainTypeListView(generics.ListAPIView):
    """
    Alle Tile-Typen mit ihren Werten, in der Reihenfolge des Spiels, unter
    {"terrain": [...]}. Angelegt werden sie nur aus dem Dump
    api/fixtures/seed.json (siehe management/commands/seed.py) -- es gibt
    genau die, die das Spiel kennt, deshalb kein POST.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]
    queryset = TerrainType.objects.all()
    serializer_class = AdminTerrainTypeSerializer

    # Any: Signatur von ListModelMixin.list in den DRF-Stubs (URL-Parameter,
    # ungeprueft durchgereicht).
    def list(self, request: Request, *args: Any, **kwargs: Any) -> Response:
        serializer: BaseSerializer[TerrainType] = self.get_serializer(self.get_queryset(), many=True)
        return Response({"terrain": serializer.data})


class AdminTerrainTypeDetailView(generics.RetrieveUpdateAPIView):
    """
    Ein Tile-Typ, angesprochen ueber seinen code (z.B. admin/terrain/forest/).
    Aendern ja, loeschen nein (siehe AdminTerrainTypeListView). Die Werte
    gelten ab dem naechsten Spielstart; laufende Spiele behalten ihre.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]
    queryset = TerrainType.objects.all()
    serializer_class = AdminTerrainTypeSerializer
    lookup_field = "code"
