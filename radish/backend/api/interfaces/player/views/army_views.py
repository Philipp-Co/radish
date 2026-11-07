"""
Endpunkte fuer die Armeen eines Spielers unter api/client/armies/ und den
Katalog, aus dem sie zusammengestellt werden, unter api/client/catalog/.

Wie bei client_views.py ergibt sich der Spieler zwingend aus request.user
(also aus dem validierten Access-Token) -- eine Armee eines anderen Spielers
ist fuer ihn schlicht nicht vorhanden (404 statt 403), damit die API nicht
verraet, welche ids es gibt.

Den Katalog pflegen spaeter eigene Admin-Schnittstellen (Rollen per OIDC,
siehe admin/roles/has_admin_role.HasAdminRole) -- hier wird er nur gelesen.
"""

from django.contrib.auth.models import AnonymousUser
from django.db.models import Count, Q, QuerySet
from django.db.models.deletion import ProtectedError
from django.shortcuts import get_object_or_404
from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.request import Request
from rest_framework.response import Response
from rest_framework.views import APIView

from .client_views import _get_or_create_player
from ....domain.army_queries import armies_with_costs
from ....models import Army, Game, Player, Species
from ..roles.has_player_role import HasPlayerRole
from ..serializers.army_detail_serializer import ArmyDetailSerializer
from ..serializers.army_list_serializer import ArmyListSerializer
from ..serializers.army_write_serializer import ArmyWriteSerializer
from ..serializers.species_catalog_serializer import SpeciesCatalogSerializer


def _army_detail_queryset() -> QuerySet[Army]:
    return armies_with_costs()


class CatalogView(APIView):
    """
    Liefert alle Spezies mit ihren Einheitentypen (samt Profilen), Waffen
    und Ausruestung -- die Auswahl fuer den Armee-Editor.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def get(self, request: Request) -> Response:
        species: QuerySet[Species] = Species.objects.prefetch_related(
            "unit_types__profiles", "weapons", "equipment"
        )
        return Response({"species": SpeciesCatalogSerializer(species, many=True).data})


class ArmyListView(APIView):
    """
    GET listet die eigenen Armeen, POST legt eine neue an (siehe
    army_write_serializer.ArmyWriteSerializer fuer die Regeln).

    GET legt dabei wie client_views.CurrentGameView KEIN Player-Objekt an --
    ohne Player gibt es ohnehin keine Armeen.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def get(self, request: Request) -> Response:
        # IsAuthenticated laesst keinen AnonymousUser durch.
        assert not isinstance(request.user, AnonymousUser)
        armies: QuerySet[Army] = _army_detail_queryset().filter(player__user=request.user).annotate(
            unit_count=Count("units")
        )
        return Response({"armies": ArmyListSerializer(armies, many=True).data})

    def post(self, request: Request) -> Response:
        player: Player = _get_or_create_player(request)
        serializer: ArmyWriteSerializer = ArmyWriteSerializer(data=request.data, context={"player": player})
        serializer.is_valid(raise_exception=True)
        army: Army = serializer.save()
        return Response(
            ArmyDetailSerializer(_army_detail_queryset().get(pk=army.pk)).data,
            status=status.HTTP_201_CREATED,
        )


def _game_using(army: Army) -> Game | None:
    """Das laufende Spiel, in dem die Armee antritt, oder None."""
    return Game.objects.filter(Q(host_army=army) | Q(second_player_army=army)).first()


def _in_game_response(game: Game) -> Response:
    return Response(
        {
            "detail": f"Die Armee wird im laufenden Spiel „{game.name}“ verwendet und "
            "kann erst danach geaendert oder geloescht werden."
        },
        status=status.HTTP_409_CONFLICT,
    )


class ArmyDetailView(APIView):
    """
    Eine einzelne eigene Armee: GET liefert sie vollstaendig, PUT ersetzt
    sie durch den mitgeschickten Stand, DELETE loescht sie samt Einheiten.

    Steckt die Armee in einem laufenden Spiel, sind PUT und DELETE gesperrt
    (409) -- sonst liesse sie sich nachtraeglich ueber das Punktelimit des
    Spiels bringen. Game.host_army/second_player_army haben dafuer PROTECT
    als zweite Sicherung.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    @staticmethod
    def _get_own_army(request: Request, army_id: int) -> Army:
        return get_object_or_404(
            _army_detail_queryset(), pk=army_id, player__user=request.user
        )

    def get(self, request: Request, army_id: int) -> Response:
        return Response(ArmyDetailSerializer(self._get_own_army(request, army_id)).data)

    def put(self, request: Request, army_id: int) -> Response:
        army: Army = self._get_own_army(request, army_id)
        game: Game | None = _game_using(army)
        if game is not None:
            return _in_game_response(game)
        serializer: ArmyWriteSerializer = ArmyWriteSerializer(
            army, data=request.data, context={"player": army.player}
        )
        serializer.is_valid(raise_exception=True)
        serializer.save()
        return Response(ArmyDetailSerializer(self._get_own_army(request, army_id)).data)

    def delete(self, request: Request, army_id: int) -> Response:
        army: Army = self._get_own_army(request, army_id)
        game: Game | None = _game_using(army)
        if game is not None:
            return _in_game_response(game)
        try:
            army.delete()
        except ProtectedError:
            # Zwischen Pruefung und Loeschen einem Spiel zugeordnet.
            game = _game_using(army)
            # ProtectedError heisst: ein Spiel verweist auf die Armee.
            assert game is not None
            return _in_game_response(game)
        return Response(status=status.HTTP_204_NO_CONTENT)
