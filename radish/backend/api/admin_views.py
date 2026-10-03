"""
Endpunkte fuer den Adminbereich des Frontends (siehe radish/frontend/src/app/
pages/games/admin/) -- getrennt von client_views.py (Spieler-Endpunkte) und
server_views.py (Registrierung durch den technischen Server-Account): hier
geht es ausschliesslich um menschliche Admins mit der Keycloak-Realm-Rolle
"Radish-Admin" (siehe permissions.HasAdminRole) -- die Serverliste nur
lesend, der Armee-Katalog (Spezies, Einheitentypen mit ihren Profilen,
Waffen, Ausruestung) unter admin/catalog/ auch schreibend.
"""

from django.db.models import Exists, OuterRef, Prefetch
from django.db.models.deletion import ProtectedError
from rest_framework import generics, status
from rest_framework.exceptions import APIException, ValidationError
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from .models import (
    EntityProfile,
    Equipment,
    GameServer,
    Species,
    Unit,
    UnitEntityEquipment,
    UnitEntityWeapon,
    UnitType,
    Weapon,
)
from .permissions import HasAdminRole
from .serializers import (
    AdminEntityProfileSerializer,
    AdminEquipmentSerializer,
    AdminSpeciesSerializer,
    AdminUnitTypeSerializer,
    AdminWeaponSerializer,
    GameServerSerializer,
)


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

    def get(self, request):
        servers = GameServer.objects.all().order_by("name")
        serializer = GameServerSerializer(servers, many=True)
        return Response({"servers": serializer.data})


# ---------------------------------------------------------------------------
# Armee-Katalog
# ---------------------------------------------------------------------------

# Ob ein Katalogeintrag in mindestens einer Armee steckt -- als Annotation
# "in_use", die serializers._CatalogAdminSerializer fuer die Sperre der
# Strukturfelder liest und die das Frontend zum Ausgrauen nutzt. Ein Profil
# gilt schon als verwendet, wenn sein Einheitentyp es ist: seine Anzahl-
# grenzen betreffen jede Einheit des Typs, auch ohne Entitaet mit dem Profil.
IN_USE = {
    UnitType: Exists(Unit.objects.filter(unit_type=OuterRef("pk"))),
    EntityProfile: Exists(Unit.objects.filter(unit_type=OuterRef("unit_type"))),
    Weapon: Exists(UnitEntityWeapon.objects.filter(weapon=OuterRef("pk"))),
    Equipment: Exists(UnitEntityEquipment.objects.filter(equipment=OuterRef("pk"))),
}


class Conflict(APIException):
    status_code = status.HTTP_409_CONFLICT
    default_code = "conflict"


class _CatalogAdminMixin:
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
    model = None
    list_key = None
    # Query-Parameter -> Feld, z.B. {"species": "species_id"}.
    filters = {}
    # Query-Parameter -> Feld fuer Auswahlfelder, z.B. ?weapon_class=heavy.
    choice_filters = {}
    # Fuer species_name/unit_type_name in der Antwort (Kontext in der Suche).
    related = ()

    def get_queryset(self):
        queryset = self.model.objects.select_related(*self.related)
        if self.model in IN_USE:
            queryset = queryset.annotate(in_use=IN_USE[self.model])
        return queryset

    def list(self, request, *args, **kwargs):
        queryset = self.get_queryset()
        for param, field in self.filters.items():
            value = request.query_params.get(param)
            if value is not None:
                if not value.isdigit():
                    raise ValidationError({param: "Muss eine id sein."})
                queryset = queryset.filter(**{field: int(value)})
        for param, field in self.choice_filters.items():
            value = request.query_params.get(param)
            if value:
                queryset = queryset.filter(**{field: value})
        search = request.query_params.get("search", "").strip()
        if search:
            queryset = queryset.filter(name__icontains=search)
        serializer = self.get_serializer(queryset, many=True)
        return Response({self.list_key: serializer.data})

    def create(self, request, *args, **kwargs):
        serializer = self.get_serializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        instance = serializer.save()
        return Response(self._reloaded(instance), status=status.HTTP_201_CREATED)

    def update(self, request, *args, **kwargs):
        serializer = self.get_serializer(
            self.get_object(), data=request.data, partial=kwargs.pop("partial", False)
        )
        serializer.is_valid(raise_exception=True)
        instance = serializer.save()
        return Response(self._reloaded(instance))

    def perform_destroy(self, instance):
        try:
            instance.delete()
        except ProtectedError:
            raise Conflict(
                f"{instance} wird noch verwendet (in Armeen oder von anderen "
                "Katalogeintraegen) und kann nicht geloescht werden."
            )

    def _reloaded(self, instance):
        return self.get_serializer(self.get_queryset().get(pk=instance.pk)).data


class AdminSpeciesListView(_CatalogAdminMixin, generics.ListCreateAPIView):
    model = Species
    list_key = "species"
    serializer_class = AdminSpeciesSerializer


class AdminSpeciesDetailView(_CatalogAdminMixin, generics.RetrieveUpdateDestroyAPIView):
    model = Species
    serializer_class = AdminSpeciesSerializer


class _UnitTypeAdminMixin(_CatalogAdminMixin):
    model = UnitType
    serializer_class = AdminUnitTypeSerializer
    related = ("species",)

    def get_queryset(self):
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


class AdminEntityProfileListView(_CatalogAdminMixin, generics.ListCreateAPIView):
    model = EntityProfile
    list_key = "profiles"
    filters = {"unit_type": "unit_type_id", "species": "unit_type__species_id"}
    related = ("unit_type__species",)
    serializer_class = AdminEntityProfileSerializer


class AdminEntityProfileDetailView(_CatalogAdminMixin, generics.RetrieveUpdateDestroyAPIView):
    model = EntityProfile
    related = ("unit_type__species",)
    serializer_class = AdminEntityProfileSerializer


class AdminWeaponListView(_CatalogAdminMixin, generics.ListCreateAPIView):
    model = Weapon
    list_key = "weapons"
    filters = {"species": "species_id"}
    choice_filters = {"weapon_class": "weapon_class"}
    related = ("species",)
    serializer_class = AdminWeaponSerializer


class AdminWeaponDetailView(_CatalogAdminMixin, generics.RetrieveUpdateDestroyAPIView):
    model = Weapon
    related = ("species",)
    serializer_class = AdminWeaponSerializer


class AdminEquipmentListView(_CatalogAdminMixin, generics.ListCreateAPIView):
    model = Equipment
    list_key = "equipment"
    filters = {"species": "species_id"}
    related = ("species",)
    serializer_class = AdminEquipmentSerializer


class AdminEquipmentDetailView(_CatalogAdminMixin, generics.RetrieveUpdateDestroyAPIView):
    model = Equipment
    related = ("species",)
    serializer_class = AdminEquipmentSerializer
