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
from urllib.parse import urlencode

import requests
from django.conf import settings
from django.contrib.auth import get_user_model


def _public_realm_url(path):
    """
    Fuer alles, was als Redirect beim Browser landet (Login, Logout): muss
    die von aussen erreichbare Keycloak-URL sein, sonst kann der Browser sie
    nicht aufloesen.
    """
    return f"{settings.KEYCLOAK_BASE_URL}/realms/{settings.KEYCLOAK_REALM}{path}"


def _internal_realm_url(path):
    """
    Fuer reine Server-zu-Server-Aufrufe (Code-Tausch, userinfo, JWKS): laeuft
    das Backend selbst im Docker-Netzwerk, ist KEYCLOAK_INTERNAL_BASE_URL der
    Service-Name "keycloak" statt der nach aussen veroeffentlichten URL --
    siehe Kommentar bei KEYCLOAK_INTERNAL_BASE_URL in settings.py.
    """
    return f"{settings.KEYCLOAK_INTERNAL_BASE_URL}/realms/{settings.KEYCLOAK_REALM}{path}"


def authorization_endpoint():
    return _public_realm_url("/protocol/openid-connect/auth")


def token_endpoint():
    return _internal_realm_url("/protocol/openid-connect/token")


def userinfo_endpoint():
    return _internal_realm_url("/protocol/openid-connect/userinfo")


def end_session_endpoint():
    return _public_realm_url("/protocol/openid-connect/logout")


def new_state():
    """
    Zufaelliger state-Wert gegen CSRF: wird vor dem Redirect in der Session
    abgelegt und beim Rueckweg (login/callback/) mit dem von Keycloak
    zurueckgegebenen state verglichen.
    """
    return secrets.token_urlsafe(32)


def build_authorization_redirect_url(state):
    params = {
        "client_id": settings.KEYCLOAK_CLIENT_ID,
        "response_type": "code",
        "scope": "openid",
        "redirect_uri": settings.KEYCLOAK_REDIRECT_URI,
        "state": state,
    }
    return f"{authorization_endpoint()}?{urlencode(params)}"


def exchange_code_for_tokens(code):
    """
    Tauscht den Authorization Code gegen Access-/ID-Token. Laeuft als
    Server-zu-Server-Aufruf mit Client-Secret (Back-Channel) -- der Code
    selbst kommt ueber den Browser-Redirect zu login/callback/, verlaesst
    den Server danach aber nicht mehr.
    """
    response = requests.post(
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
    return response.json()


def fetch_userinfo(access_token):
    response = requests.get(
        userinfo_endpoint(),
        headers={"Authorization": f"Bearer {access_token}"},
        timeout=10,
    )
    response.raise_for_status()
    return response.json()


def build_end_session_redirect_url(id_token=None):
    """
    id_token als id_token_hint mitzugeben ist der von Keycloak empfohlene
    Weg (kein extra Client-Secret beim Logout noetig, Keycloak kann den
    Aufrufer allein daraus erkennen). Ohne ID-Token (z.B. wenn die Session
    schon weg war) faellt das auf client_id zurueck.
    """
    params = {"post_logout_redirect_uri": settings.KEYCLOAK_POST_LOGOUT_REDIRECT_URI}
    if id_token:
        params["id_token_hint"] = id_token
    else:
        params["client_id"] = settings.KEYCLOAK_CLIENT_ID
    return f"{end_session_endpoint()}?{urlencode(params)}"


def get_or_create_user_from_claims(claims):
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
    User = get_user_model()
    sub = claims["sub"]
    user, _ = User.objects.get_or_create(
        username=sub,
        defaults={"email": claims.get("email", "")},
    )
    return user
