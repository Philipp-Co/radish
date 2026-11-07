from django.conf import settings
from django.db import models
from django.utils.crypto import get_random_string


def _generate_player_identifier():
    """
    8 Zeichen aus Buchstaben/Ziffern -- in ASCII also 8 Byte, wie gefordert.
    """
    return get_random_string(length=8)


class Player(models.Model):
    """
    Ein Spieler, 1:1 an genau einen (durch Keycloak authentifizierten)
    Django-User gebunden -- siehe KeycloakJWTAuthentication in
    api/interfaces/auth/authentication.py. Der Django-User traegt selbst kein Passwort und
    keine Profildaten, sein username ist einfach Keycloaks "sub" (siehe
    keycloak.get_or_create_user_from_claims); Login/Logout laufen
    vollstaendig ueber Keycloak.

    "identifier" bleibt eine beim Erstellen zufaellig zugewiesene, 8 Byte
    lange Kennung -- sie dient weiterhin als der nach aussen sichtbare
    Spieler-Handle in API-Antworten (siehe game_detail_serializer.GameDetailSerializer),
    kann aber anders als frueher nicht mehr vom Client vorgegeben werden:
    welcher Player gemeint ist, ergibt sich zwingend aus dem validierten
    Access-Token (siehe client_views._get_or_create_player).
    """

    user = models.OneToOneField(
        settings.AUTH_USER_MODEL, on_delete=models.CASCADE, related_name="player"
    )
    identifier = models.CharField(max_length=8, unique=True, editable=False)
    # Keycloak-Benutzername ("preferred_username" im Access-Token). Wird bei
    # jedem Aufruf nachgezogen, der den Player aufloest (siehe
    # client_views._sync_player_name) -- der Name gehoert Keycloak, hier
    # steht nur eine Kopie, etwa fuer die Spieldaten beim Start (siehe
    # instance_client.game_start_document). Leer, solange er noch nie
    # mitkam.
    name = models.CharField(max_length=255, blank=True, default="")

    def save(self, *args, **kwargs):
        # Nur beim allerersten Speichern zuweisen, nicht bei jedem Update.
        if not self.identifier:
            for _ in range(10):
                candidate = _generate_player_identifier()
                if not Player.objects.filter(identifier=candidate).exists():
                    self.identifier = candidate
                    break
            else:
                # Bei 8 Zeichen aus 62 moeglichen praktisch ausgeschlossen,
                # aber ein stiller Endlosversuch waere schlimmer als ein
                # klarer Fehler.
                raise RuntimeError(
                    "Konnte keine eindeutige Spieler-Kennung erzeugen."
                )
        super().save(*args, **kwargs)

    def __str__(self):
        return self.identifier
