"""
JWT-Authentifizierung fuer die Endpunkte unter api/ -- ausser login/logout,
die weiterhin ueber die Django-Session laufen (siehe auth_views.py).

Validiert direkt die von Keycloak ausgestellten Access-Tokens: RS256 gegen
Keycloaks JWKS-Endpunkt (siehe SIMPLE_JWT["JWK_URL"] in settings.py). Django
stellt selbst keine eigenen Tokens aus, sondern ist reiner Resource-Server
fuer Keycloak-Tokens.

Ein gueltiges Keycloak-Token reicht: der zugehoerige Django-User muss vorher
nicht ueber api/login/ angelegt worden sein -- Keycloak ist die Quelle der
Wahrheit fuer die Identitaet, get_user() legt den User bei Bedarf selbst an
(derselbe Weg wie beim Login-Callback, siehe keycloak.py).
"""

from rest_framework_simplejwt.authentication import JWTAuthentication

from .keycloak import get_or_create_user_from_claims


class KeycloakJWTAuthentication(JWTAuthentication):
    def get_user(self, validated_token):
        return get_or_create_user_from_claims(validated_token.payload)
