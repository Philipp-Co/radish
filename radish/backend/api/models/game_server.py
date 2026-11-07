from django.core.validators import MaxValueValidator, MinValueValidator, RegexValidator
from django.db import models


class GameServer(models.Model):
    """
    Ein verfuegbarer Spielserver, ueber den ein Client sich verbinden kann.

    "Server" allein waere hier mehrdeutig (siehe radish/game-server-core/ vs.
    zucchini_server) -- deshalb GameServer, auch wenn es hier erstmal nur um
    die Ablage von Adresse, Port und Name geht, ohne weitere Logik.
    """

    # unique, weil das Entfernen eines Servers ueber genau diesen Namen
    # laeuft (siehe server_views.ServerUnregisterView) -- ohne Eindeutigkeit
    # waere nicht klar, welcher Eintrag beim Entfernen gemeint ist.
    name = models.CharField(max_length=255, unique=True)
    # Bewusst CharField statt GenericIPAddressField: "address" muss keine
    # rohe IP sein, sondern alles, worueber sich diese Instanz erreichen
    # laesst -- z.B. auch ein Docker-Compose-Servicename wie "game-server"
    # (siehe docker-compose.yaml, RADISH_GAME_SERVER_ADDRESS), den ein
    # GenericIPAddressField als ungueltig abgelehnt haette. Die Aufloesung
    # (Hostname -> IP) uebernimmt ohnehin schon asyncios
    # create_datagram_endpoint() beim Verbindungsaufbau (siehe consumers.py,
    # EchoConsumer.connect()), hier wird nichts vorab validiert oder
    # aufgeloest.
    address = models.CharField(max_length=255)
    port = models.PositiveIntegerField(
        validators=[MinValueValidator(0), MaxValueValidator(65535)]
    )

    # True, sobald ein Game (siehe unten) diesen Server belegt. Steht separat
    # und nicht nur implizit ueber Game.server, damit sich freie Server auch
    # ohne einen Blick auf Game direkt abfragen lassen (GameServer.objects
    # .filter(is_occupied=False)).
    is_occupied = models.BooleanField(default=False)

    # HTTP-Basis-URL der Instanz-API (radish/game-server/instances/views.py),
    # ueber die das Backend ein Spiel auf dieser Instanz startet bzw.
    # abbricht (siehe instance_client.py) -- getrennt von address/port, die
    # auf den UDP-Port des Spielservers zeigen. Leer bei Instanzen, die sich
    # ohne sie gemeldet haben; auf denen laesst sich kein Spiel starten.
    control_url = models.CharField(max_length=255, blank=True, default="")
    # Name der Instanz in ihrem Container: Zucchini-Name und Pfadteil der
    # Instanz-API ({control_url}/api/instances/{instance_name}/...). Nur im
    # Container eindeutig, nicht global wie "name" -- zwei Container duerfen
    # beide eine "game-instance-1" haben (siehe docker/game-server/
    # game-instance-entrypoint.sh). Landet in einer URL, deshalb nur
    # Buchstaben, Ziffern, "_" und "-", wie die Instanz-API selbst es
    # verlangt. Leer wie control_url bei Instanzen, die sich ohne ihn
    # gemeldet haben; auf denen laesst sich kein Spiel starten.
    instance_name = models.CharField(
        max_length=255,
        blank=True,
        default="",
        validators=[RegexValidator(r"^[A-Za-z0-9_-]+$")],
    )

    def __str__(self):
        return f"{self.name} ({self.address}:{self.port})"
