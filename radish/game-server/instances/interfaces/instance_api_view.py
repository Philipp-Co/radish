"""
Gemeinsame Basis der Instanz-Endpunkte (siehe die Klasse selbst).
"""

from rest_framework.permissions import AllowAny
from rest_framework.views import APIView


class InstanceAPIView(APIView):
    """
    Gemeinsame Basis aller Instanz-Endpunkte, damit die Auth spaeter an
    genau einer Stelle eingehaengt wird.

    Vorerst ohne Auth: keine Authentifizierung, jeder darf. Hier kommt
    spaeter z.B. eine Keycloak-JWT-Pruefung wie im Backend hin (siehe
    radish/backend/api/interfaces/auth/authentication.py). Ohne SessionAuthentication
    erzwingt DRF auch kein CSRF -- Aufrufe des Backends (Server zu Server,
    kein Browser) kommen deshalb ohne CSRF-Token durch. Nur fuer Dev.
    """

    authentication_classes = []
    permission_classes = [AllowAny]
