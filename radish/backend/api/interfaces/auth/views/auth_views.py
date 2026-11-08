"""
Login/Logout ueber Keycloak (Authorization Code Flow, serverseitig).

api/login/           -- nicht eingeloggt: Redirect zu Keycloak.
                         eingeloggt: keine Weiterleitung, direkte Bestaetigung.
api/login/callback/  -- Rueckweg von Keycloak: tauscht den Code gegen Tokens,
                         legt/findet den Django-User und startet die Session.
api/logout/          -- beendet zuerst die Django-Session, dann (per Redirect)
                         die Keycloak-SSO-Session.

Der Django-User traegt selbst kein Passwort/keine Profildaten -- er ist per
Keycloak authentifiziert, sein username ist Keycloaks "sub" (siehe
keycloak.get_or_create_user_from_claims). Player (models/player.py) haengt per
OneToOneField an genau diesem User; welcher Player zu einer Anfrage gehoert,
ergibt sich also aus request.user (siehe client_views._get_or_create_player),
nicht aus einem hier zurueckgegebenen Identifier.
"""

from django.contrib.auth import get_user_model, login, logout
from django.contrib.auth import models as auth_models
from django.http import HttpRequest, HttpResponseBadRequest, HttpResponseRedirect, JsonResponse
from django.views import View

from api.interfaces.auth import keycloak

User = get_user_model()


class LoginView(View):
    def get(self, request: HttpRequest) -> JsonResponse | HttpResponseRedirect:
        if request.user.is_authenticated:
            # Bewusst ohne username/sub in der Antwort: das ist Keycloaks
            # interner User-Identifier und darf nicht an den Client
            # rausgehen (Player.identifier ist die dafuer vorgesehene,
            # oeffentliche Kennung -- siehe models.Player).
            return JsonResponse({"authenticated": True})

        state: str = keycloak.new_state()
        request.session["keycloak_login_state"] = state
        return HttpResponseRedirect(keycloak.build_authorization_redirect_url(state))


class LoginCallbackView(View):
    def get(self, request: HttpRequest) -> JsonResponse | HttpResponseBadRequest:
        error: str | None = request.GET.get("error")
        if error:
            # "error" kommt ungeprueft aus der URL -- jeder kann einen Link
            # auf login/callback/?error=<script>... bauen. Als text/plain
            # statt Djangos Default text/html, sonst fuehrte der Browser das
            # auf der Origin des Frontends aus (reflected XSS).
            # SecurityMiddleware setzt X-Content-Type-Options: nosniff, der
            # Browser raet den Typ also nicht doch wieder zu HTML um.
            return HttpResponseBadRequest(
                f"Keycloak meldet einen Fehler: {error}",
                content_type="text/plain; charset=utf-8",
            )

        # Die Session liefert Any; abgelegt wurde dort ein str (LoginView).
        expected_state: str | None = request.session.pop("keycloak_login_state", None)
        state: str | None = request.GET.get("state")
        if not expected_state or state != expected_state:
            return HttpResponseBadRequest("Ungueltiger oder fehlender state-Parameter.")

        code: str | None = request.GET.get("code")
        if not code:
            return HttpResponseBadRequest("Kein code-Parameter in der Antwort von Keycloak.")

        tokens: keycloak.TokenResponse = keycloak.exchange_code_for_tokens(code)
        userinfo: keycloak.KeycloakClaims = keycloak.fetch_userinfo(tokens["access_token"])

        # auth_models.User: das modulweite User = get_user_model() ist fuer
        # mypy eine Variable und als Typ nicht verwendbar.
        user: auth_models.User = keycloak.get_or_create_user_from_claims(userinfo)

        # Keine Passwort-basierte authenticate(): die Identitaet ist durch
        # den Code-Tausch mit Keycloak schon bewiesen. login() braucht dafuer
        # das Backend -- als Argument statt als backend-Attribut am User, das
        # die Stubs nicht kennen (login() liest beides gleich aus).
        login(request, user, backend="django.contrib.auth.backends.ModelBackend")

        # Fuer den End-Session-Redirect bei /api/logout/ noetig (id_token_hint).
        request.session["keycloak_id_token"] = tokens.get("id_token")

        # Bewusst ohne sub/preferred_username in der Antwort: sub ist
        # Keycloaks interner User-Identifier und darf nicht an den Client
        # rausgehen (siehe models.Player-Docstring). preferred_username
        # waere kein Sicherheitsproblem, aber fuer diesen Endpunkt reicht
        # die reine Bestaetigung des Logins.
        return JsonResponse({"authenticated": True})


class LogoutView(View):
    def get(self, request: HttpRequest) -> HttpResponseRedirect:
        # Wie oben: abgelegt wurde ein str oder None (LoginCallbackView).
        id_token: str | None = request.session.pop("keycloak_id_token", None)
        logout(request)
        return HttpResponseRedirect(keycloak.build_end_session_redirect_url(id_token))
