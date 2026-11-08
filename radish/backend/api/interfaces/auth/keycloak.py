"""
Kleine Helfer fuer die OpenID-Connect-Anbindung an Keycloak.

Der Authorization-Code-Flow laeuft komplett serverseitig: "radish-backend"
ist ein confidential Client mit Secret (siehe
docker/keycloak/realm-radish.json), der Browser sieht nie ein Token, nur
Redirects und am Ende eine Django-Session (siehe auth_views.py).

Bewusst ohne eigene JWT-Signaturpruefung: der Access-Token wird direkt gegen
Keycloaks userinfo-Endpunkt getauscht (fetch_userinfo). Keycloak selbst
prueft dabei die Signatur -- wir muessen kein JWKS mitfuehren oder cachen.
"""

import secrets
from typing import NotRequired, TypedDict
from urllib.parse import urlencode

from django.conf import settings
from django.contrib.auth import get_user_model

# Nur fuer Type Hints: das Projekt nutzt Djangos Standard-User-Model (kein
# AUTH_USER_MODEL), get_user_model() liefert also genau diese Klasse.
# pylint: disable-next=imported-auth-user
from django.contrib.auth.models import User
import requests


class TokenResponse(TypedDict):
    """
    Antwort des Token-Endpunkts beim Code-Tausch (OpenID Connect, Abschnitt
    3.1.3.3). Keycloak schickt noch mehr ("not-before-policy", ...), hier
    steht nur, was der Standard zusagt bzw. was wir lesen.
    """

    access_token: str
    token_type: str
    expires_in: NotRequired[int]
    refresh_token: NotRequired[str]
    refresh_expires_in: NotRequired[int]
    # Nur bei scope "openid" -- den fordern wir immer an, die Antwort ist
    # aber nicht unsere Garantie (deshalb tokens.get("id_token")).
    id_token: NotRequired[str]
    scope: NotRequired[str]
    session_state: NotRequired[str]


class RealmAccess(TypedDict):
    """Keycloaks Klaim "realm_access"; roles wird defensiv mit .get gelesen."""

    roles: NotRequired[list[str]]


class KeycloakClaims(TypedDict):
    """
    Claims eines Keycloak-Nutzers: die Antwort des userinfo-Endpunkts oder
    das payload eines validierten Access-Tokens. Sicher ist nur "sub", alles
    andere haengt an Scopes und Mappern im Realm.
    """

    sub: str
    email: NotRequired[str]
    email_verified: NotRequired[bool]
    preferred_username: NotRequired[str]
    name: NotRequired[str]
    given_name: NotRequired[str]
    family_name: NotRequired[str]
    realm_access: NotRequired[RealmAccess]


def _public_realm_url(path: str) -> str:
    """
    Fuer alles, was als Redirect beim Browser landet (Login, Logout): muss
    die von aussen erreichbare Keycloak-URL sein, sonst kann der Browser sie
    nicht aufloesen.
    """
    return f"{settings.KEYCLOAK_BASE_URL}/realms/{settings.KEYCLOAK_REALM}{path}"


def _internal_realm_url(path: str) -> str:
    """
    Fuer reine Server-zu-Server-Aufrufe (Code-Tausch, userinfo, JWKS): laeuft
    das Backend selbst im Docker-Netzwerk, ist KEYCLOAK_INTERNAL_BASE_URL der
    Service-Name "keycloak" statt der nach aussen veroeffentlichten URL --
    siehe Kommentar bei KEYCLOAK_INTERNAL_BASE_URL in settings.py.
    """
    return f"{settings.KEYCLOAK_INTERNAL_BASE_URL}/realms/{settings.KEYCLOAK_REALM}{path}"


def authorization_endpoint() -> str:
    return _public_realm_url("/protocol/openid-connect/auth")


def token_endpoint() -> str:
    return _internal_realm_url("/protocol/openid-connect/token")


def userinfo_endpoint() -> str:
    return _internal_realm_url("/protocol/openid-connect/userinfo")


def end_session_endpoint() -> str:
    return _public_realm_url("/protocol/openid-connect/logout")


def new_state() -> str:
    """
    Zufaelliger state-Wert gegen CSRF: wird vor dem Redirect in der Session
    abgelegt und beim Rueckweg (login/callback/) mit dem von Keycloak
    zurueckgegebenen state verglichen.
    """
    return secrets.token_urlsafe(32)


def build_authorization_redirect_url(state: str) -> str:
    params: dict[str, str] = {
        "client_id": settings.KEYCLOAK_CLIENT_ID,
        "response_type": "code",
        "scope": "openid",
        "redirect_uri": settings.KEYCLOAK_REDIRECT_URI,
        "state": state,
    }
    return f"{authorization_endpoint()}?{urlencode(params)}"


def exchange_code_for_tokens(code: str) -> TokenResponse:
    """
    Tauscht den Authorization Code gegen Access-/ID-Token. Laeuft als
    Server-zu-Server-Aufruf mit Client-Secret (Back-Channel) -- der Code
    selbst kommt ueber den Browser-Redirect zu login/callback/, verlaesst
    den Server danach aber nicht mehr.
    """
    response: requests.Response = requests.post(
        token_endpoint(),
        data={
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": settings.KEYCLOAK_REDIRECT_URI,
            "client_id": settings.KEYCLOAK_CLIENT_ID,
            "client_secret": settings.KEYCLOAK_CLIENT_SECRET,
        },
        timeout=10,
    )
    response.raise_for_status()
    # json() ist laut Stubs Any; die Form legt der OIDC-Standard fest
    # (siehe TokenResponse), geprueft wird sie hier nicht.
    return response.json()


def fetch_userinfo(access_token: str) -> KeycloakClaims:
    response: requests.Response = requests.get(
        userinfo_endpoint(),
        headers={"Authorization": f"Bearer {access_token}"},
        timeout=10,
    )
    response.raise_for_status()
    # Wie oben: Form nach OIDC (userinfo traegt immer "sub"), ungeprueft.
    return response.json()


def build_end_session_redirect_url(id_token: str | None = None) -> str:
    """
    id_token als id_token_hint mitzugeben ist der von Keycloak empfohlene
    Weg (kein extra Client-Secret beim Logout noetig, Keycloak kann den
    Aufrufer allein daraus erkennen). Ohne ID-Token (z.B. wenn die Session
    schon weg war) faellt das auf client_id zurueck.
    """
    params: dict[str, str] = {"post_logout_redirect_uri": settings.KEYCLOAK_POST_LOGOUT_REDIRECT_URI}
    if id_token:
        params["id_token_hint"] = id_token
    else:
        params["client_id"] = settings.KEYCLOAK_CLIENT_ID
    return f"{end_session_endpoint()}?{urlencode(params)}"


def get_or_create_user_from_claims(claims: KeycloakClaims) -> User:
    """
    Legt bei Bedarf einen Django-User zu den Claims eines Keycloak-Tokens an
    (username = sub) oder findet den bestehenden. Genutzt sowohl vom
    Login-Callback (Redirect-Flow, siehe auth_views.LoginCallbackView) als
    auch von KeycloakJWTAuthentication (Bearer-Token-Flow, siehe
    authentication.py) -- Keycloak ist in beiden Faellen die Quelle der
    Wahrheit fuer die Identitaet, nicht die lokale User-Tabelle. claims kann
    sowohl das dict vom userinfo-Endpunkt als auch das payload-dict eines
    validierten Access-Tokens sein, beide tragen mindestens "sub".
    """
    user_model: type[User] = get_user_model()
    sub: str = claims["sub"]
    user: User
    user, _ = user_model.objects.get_or_create(
        username=sub,
        defaults={"email": claims.get("email", "")},
    )
    return user
