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

from typing import cast

# Nur fuer Type Hints: das Projekt nutzt Djangos Standard-User-Model (kein
# AUTH_USER_MODEL), get_user_model() liefert also genau diese Klasse.
# pylint: disable-next=imported-auth-user
from django.contrib.auth.models import User
from rest_framework_simplejwt.authentication import JWTAuthentication
from rest_framework_simplejwt.tokens import Token

from .keycloak import KeycloakClaims, get_or_create_user_from_claims


class KeycloakJWTAuthentication(JWTAuthentication):
    # simplejwt annotiert die Rueckgabe mit der TypeVar AuthUser, die in der
    # Klasse nicht gebunden ist -- jede konkrete Rueckgabe gilt fuer mypy als
    # inkompatibel, obwohl User eine der erlaubten Varianten ist.
    def get_user(self, validated_token: Token) -> User:  # type: ignore[override]
        # payload ist in simplejwt dict[str, Any]. Das Token ist an dieser
        # Stelle gegen Keycloaks JWKS validiert, also ein Keycloak-Access-Token
        # -- und das traegt immer "sub" (siehe KeycloakClaims).
        claims: KeycloakClaims = cast(KeycloakClaims, validated_token.payload)
        return get_or_create_user_from_claims(claims)
