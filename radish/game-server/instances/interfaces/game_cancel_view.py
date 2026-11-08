"""
Endpunkt, ueber den das Backend ein Spiel auf einer Instanz abbricht.
"""

from rest_framework import status
from rest_framework.request import Request
from rest_framework.response import Response

from instances.domain.game_files import cancel_game
from instances.interfaces.instance_api_view import InstanceAPIView


class GameCancelView(InstanceAPIView):
    """
    Bricht das Spiel auf einer Instanz ab.
    """

    def post(self, request: Request, name: str) -> Response:
        """
        Bricht das Spiel der Instanz "name" ab -- aufgerufen, wenn ein
        Spieler ein gestartetes Spiel verlaesst (siehe cancel_game). Immer
        204: das Backend hat das Spiel bereits beendet und wertet die
        Antwort nicht aus.
        """
        cancel_game(name)
        return Response(status=status.HTTP_204_NO_CONTENT)
