from django.conf import settings
from django.core.validators import MaxValueValidator, MinValueValidator
from django.db import models
from django.utils.crypto import get_random_string


class GameServer(models.Model):
    """
    Ein verfuegbarer Spielserver, ueber den ein Client sich verbinden kann.

    "Server" allein waere hier mehrdeutig (siehe radish/game-server-core/ vs.
    zucchini_server) -- deshalb GameServer, auch wenn es hier erstmal nur um
    die Ablage von IP, Port und Name geht, ohne weitere Logik.
    """

    # unique, weil das Entfernen eines Servers ueber genau diesen Namen
    # laeuft (siehe server_views.ServerUnregisterView) -- ohne Eindeutigkeit
    # waere nicht klar, welcher Eintrag beim Entfernen gemeint ist.
    name = models.CharField(max_length=255, unique=True)
    ip_address = models.GenericIPAddressField()
    port = models.PositiveIntegerField(
        validators=[MinValueValidator(0), MaxValueValidator(65535)]
    )

    # True, sobald ein Game (siehe unten) diesen Server belegt. Steht separat
    # und nicht nur implizit ueber Game.server, damit sich freie Server auch
    # ohne einen Blick auf Game direkt abfragen lassen (GameServer.objects
    # .filter(is_occupied=False)).
    is_occupied = models.BooleanField(default=False)

    def __str__(self):
        return f"{self.name} ({self.ip_address}:{self.port})"


def _generate_player_identifier():
    """
    8 Zeichen aus Buchstaben/Ziffern -- in ASCII also 8 Byte, wie gefordert.
    """
    return get_random_string(length=8)


class Player(models.Model):
    """
    Ein Spieler, 1:1 an genau einen (durch Keycloak authentifizierten)
    Django-User gebunden -- siehe KeycloakJWTAuthentication in
    api/authentication.py. Der Django-User traegt selbst kein Passwort und
    keine Profildaten, sein username ist einfach Keycloaks "sub" (siehe
    keycloak.get_or_create_user_from_claims); Login/Logout laufen
    vollstaendig ueber Keycloak.

    "identifier" bleibt eine beim Erstellen zufaellig zugewiesene, 8 Byte
    lange Kennung -- sie dient weiterhin als der nach aussen sichtbare
    Spieler-Handle in API-Antworten (siehe serializers.GameDetailSerializer),
    kann aber anders als frueher nicht mehr vom Client vorgegeben werden:
    welcher Player gemeint ist, ergibt sich zwingend aus dem validierten
    Access-Token (siehe client_views._get_or_create_player).
    """

    user = models.OneToOneField(
        settings.AUTH_USER_MODEL, on_delete=models.CASCADE, related_name="player"
    )
    identifier = models.CharField(max_length=8, unique=True, editable=False)

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


class Game(models.Model):
    """
    Ein laufendes Spiel. Belegt beim Erstellen genau einen GameServer und
    hat bis zu zwei Spieler: den Ersteller (host) und einen weiteren
    Mitspieler (second_player).
    """

    # unique, weil GameJoinView ein Spiel ueber genau diesen Namen findet
    # (siehe client_views.GameJoinView) -- ohne Eindeutigkeit waere nicht
    # klar, welchem Spiel beigetreten werden soll.
    name = models.CharField(max_length=255, unique=True)
    password = models.CharField(max_length=255)

    # Ein Game belegt genau einen Server, und ein Server ist immer nur von
    # hoechstens einem Game belegt -- deshalb OneToOne und nicht ForeignKey.
    # CASCADE: mit dem Server verschwindet auch das Game, das ihn belegte.
    server = models.OneToOneField(
        GameServer, on_delete=models.CASCADE, related_name="game"
    )

    # CASCADE: ohne den Spieler dahinter ergibt das Spiel keinen Sinn mehr.
    host = models.ForeignKey(Player, on_delete=models.CASCADE, related_name="hosted_games")
    second_player = models.ForeignKey(
        Player, on_delete=models.CASCADE, related_name="joined_games", null=True, blank=True
    )

    def __str__(self):
        return self.name
