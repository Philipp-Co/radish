from django.conf import settings
from django.core.validators import MaxValueValidator, MinValueValidator
from django.db import models
from django.utils.crypto import get_random_string


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

    def __str__(self):
        return f"{self.name} ({self.address}:{self.port})"


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


class GameStatus(models.TextChoices):
    """
    Wo ein Spiel steht:
    - LOBBY: frisch erstellt, die Spieler warten; der Host startet es von
      Hand, sobald ein zweiter Spieler da ist (siehe client_views
      .GameStartView).
    - RUNNING: die Instanz hat das Spiel aufgesetzt, die Spieler verlassen
      die Lobby. Das Aufsetzen geschieht synchron im Start-Aufruf an die
      Instanz -- einen Zwischenzustand dafuer gibt es deshalb nicht.
    """

    LOBBY = "lobby", "Lobby"
    RUNNING = "running", "Laeuft"


class Game(models.Model):
    """
    Ein Spiel. Belegt beim Erstellen genau einen GameServer und hat bis zu
    zwei Spieler: den Ersteller (host) und einen weiteren Mitspieler
    (second_player). Beginnt in der Lobby, siehe GameStatus.
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

    # Mit wie vielen Punkten gespielt wird: keine der beiden Armeen darf mehr
    # kosten (siehe client_views._army_for_game).
    points_limit = models.PositiveIntegerField()
    # Die Armeen, mit denen Host und zweiter Spieler antreten. PROTECT: eine
    # Armee in einem laufenden Spiel laesst sich nicht loeschen (und auch
    # nicht aendern, siehe army_views.ArmyDetailView). host_army ist nur in
    # der Datenbank nullable -- fuer Spiele, die es schon vor den Armeen gab;
    # neue Spiele verlangen sie immer (siehe GameCreateView).
    host_army = models.ForeignKey(
        "Army", on_delete=models.PROTECT, related_name="+", null=True, blank=True
    )
    second_player_army = models.ForeignKey(
        "Army", on_delete=models.PROTECT, related_name="+", null=True, blank=True
    )

    status = models.CharField(
        max_length=20, choices=GameStatus.choices, default=GameStatus.LOBBY
    )

    # Die Zucchini-Codes der beiden Spieler, je 16 Hexziffern (siehe
    # access.py). Geheim: vergeben beim Spielstart (GameStartView), an die
    # Instanz fuer ihre Whitelist uebergeben und vom WebSocket-Consumer vor
    # jede Nachricht seines Spielers gesetzt -- nie an einen Client
    # ausgeliefert. Leer, solange das Spiel nicht gestartet ist.
    host_zucchini_code = models.CharField(max_length=16, blank=True, default="")
    second_player_zucchini_code = models.CharField(max_length=16, blank=True, default="")

    def zucchini_code_for(self, player):
        """Der Code des Spielers in diesem Spiel, "" fuer jemand anderen."""
        if player.pk == self.host_id:
            return self.host_zucchini_code
        if player.pk == self.second_player_id:
            return self.second_player_zucchini_code
        return ""

    def __str__(self):
        return self.name


# ---------------------------------------------------------------------------
# Armeen
#
# Zwei Ebenen: der Katalog (Species, UnitType, EntityProfile, Weapon,
# Equipment) legt fest, was es gibt und welche Werte es hat -- gepflegt
# kuenftig ueber eigene Admin-Schnittstellen (Rollen kommen per OIDC aus
# Keycloak, deshalb bewusst NICHT ueber Djangos admin.py). Die Armee eines
# Spielers (Army, Unit, UnitEntity) waehlt nur aus diesem Katalog aus und
# traegt selbst keine Werte.
#
# Katalogeintraege sind per PROTECT gegen das Loeschen geschuetzt, solange
# eine Armee sie noch verwendet -- sonst verschwaenden beim Aufraeumen des
# Katalogs still Einheiten aus den Armeen der Spieler.
# ---------------------------------------------------------------------------

# Kosten: Entitaet (ueber ihr Profil), Waffe und Ausruestung haben je einen
# festen Preis im Katalog. Was eine Armee kostet, wird daraus berechnet
# (Army/Unit/UnitEntity.total_cost) und nicht gespeichert -- sonst liefe
# der Wert beim Aendern eines Preises im Katalog auseinander. Ohne Default:
# ein vergessener Preis soll auffallen, nicht still 0 kosten.

# Obergrenze fuer die Entitaeten einer Einheit, unabhaengig vom Einheitentyp.
MAX_UNIT_ENTITIES = 10


class Species(models.Model):
    """
    Eine Spezies. Jede Armee gehoert genau einer an, und alles, was in ihr
    steckt (Einheitentypen, Waffen, Ausruestung), muss von derselben sein.
    """

    name = models.CharField(max_length=255, unique=True)

    class Meta:
        ordering = ["name"]

    def __str__(self):
        return self.name


class UnitType(models.Model):
    """
    Ein Einheitentyp im Katalog einer Spezies. Die Werte der einzelnen
    Entitaeten stehen nicht hier, sondern in den EntityProfile-Eintraegen
    darunter -- eine Einheit darf Entitaeten mit abweichenden Profilen
    enthalten (etwa einen Anfuehrer neben seinen Soldaten).
    """

    species = models.ForeignKey(
        Species, on_delete=models.PROTECT, related_name="unit_types"
    )
    name = models.CharField(max_length=255)
    min_entities = models.PositiveSmallIntegerField(
        default=1, validators=[MinValueValidator(1), MaxValueValidator(MAX_UNIT_ENTITIES)]
    )
    max_entities = models.PositiveSmallIntegerField(
        default=MAX_UNIT_ENTITIES,
        validators=[MinValueValidator(1), MaxValueValidator(MAX_UNIT_ENTITIES)],
    )
    # Wie viele schwere bzw. super-schwere Waffen die Einheit hoechstens
    # gleichzeitig fuehren darf, ueber alle Entitaeten zusammen -- zusaetzlich
    # zu den Slots der Profile. None = keine Grenze auf Einheitenebene.
    max_heavy_weapons = models.PositiveSmallIntegerField(null=True, blank=True)
    max_super_heavy_weapons = models.PositiveSmallIntegerField(null=True, blank=True)
    # Wie viele andere Einheiten eine Einheit dieses Typs transportieren
    # kann (0 = kein Transporter).
    transport_capacity = models.PositiveSmallIntegerField(default=0)
    # Ob Einheiten dieses Typs Ziele einnehmen koennen.
    can_capture_objectives = models.BooleanField(default=False)
    # Bewegungsradius: wie viele Felder weit sich die Einheit bewegen kann.
    # Wie die Werte der Profile ohne Default -- der Admin traegt ihn ein.
    movement = models.PositiveSmallIntegerField()

    class Meta:
        ordering = ["species", "name"]
        constraints = [
            models.UniqueConstraint(
                fields=["species", "name"], name="unique_unit_type_per_species"
            ),
        ]

    def __str__(self):
        return f"{self.name} ({self.species})"


class EntityProfile(models.Model):
    """
    Die Werte einer Entitaet innerhalb eines Einheitentyps. min_count/
    max_count begrenzen, wie viele Entitaeten mit diesem Profil eine Einheit
    haben darf -- "genau ein Anfuehrer" ist dann min_count = max_count = 1.
    """

    unit_type = models.ForeignKey(
        UnitType, on_delete=models.CASCADE, related_name="profiles"
    )
    name = models.CharField(max_length=255)
    health = models.PositiveSmallIntegerField()
    armor = models.PositiveSmallIntegerField()
    strength = models.PositiveSmallIntegerField()
    accuracy = models.PositiveSmallIntegerField()
    equipment_slots = models.PositiveSmallIntegerField()
    # Waffenslots je Klasse (siehe WeaponClass): weapon_slots fuer
    # Standardwaffen, dazu schwere und super-schwere Waffen -- alle traegt
    # eine einzelne Entitaet, etwa der Soldat mit der schweren Waffe oder
    # ein Panzer mit Hauptkanone und Seitenwaffen.
    weapon_slots = models.PositiveSmallIntegerField()
    heavy_weapon_slots = models.PositiveSmallIntegerField(default=0)
    super_heavy_weapon_slots = models.PositiveSmallIntegerField(default=0)
    cost = models.PositiveIntegerField()
    min_count = models.PositiveSmallIntegerField(
        default=0, validators=[MaxValueValidator(MAX_UNIT_ENTITIES)]
    )
    max_count = models.PositiveSmallIntegerField(
        default=MAX_UNIT_ENTITIES, validators=[MaxValueValidator(MAX_UNIT_ENTITIES)]
    )

    class Meta:
        ordering = ["unit_type", "id"]
        constraints = [
            models.UniqueConstraint(
                fields=["unit_type", "name"], name="unique_profile_per_unit_type"
            ),
        ]

    def __str__(self):
        return f"{self.name} ({self.unit_type.name})"


class WeaponClass(models.TextChoices):
    """
    Welche Slots eine Waffe belegt: jede Klasse hat am Entitaetsprofil ihre
    eigene Slotzahl (EntityProfile.weapon_slots, heavy_weapon_slots,
    super_heavy_weapon_slots) -- getragen werden alle Waffen von einzelnen
    Entitaeten, nicht von der Einheit.
    """

    STANDARD = "standard", "Waffe"
    HEAVY = "heavy", "Schwere Waffe"
    SUPER_HEAVY = "super_heavy", "Super-schwere Waffe"


class Weapon(models.Model):
    """
    Eine Waffe im Katalog einer Spezies. Alle Klassen (siehe WeaponClass)
    haben dieselben Werte und unterscheiden sich nur darin, wer sie traegt
    -- deshalb ein Modell mit weapon_class statt eines je Klasse.
    """

    species = models.ForeignKey(
        Species, on_delete=models.PROTECT, related_name="weapons"
    )
    name = models.CharField(max_length=255)
    weapon_class = models.CharField(
        max_length=20, choices=WeaponClass.choices, default=WeaponClass.STANDARD
    )
    shots = models.PositiveSmallIntegerField()
    strength = models.PositiveSmallIntegerField()
    # Reichweite als Spanne: unterhalb von min_range kann die Waffe nicht
    # feuern (0 = keine Mindestreichweite).
    min_range = models.PositiveSmallIntegerField(default=0)
    max_range = models.PositiveSmallIntegerField()
    armor_penetration = models.PositiveSmallIntegerField()
    cost = models.PositiveIntegerField()

    class Meta:
        ordering = ["species", "name"]
        constraints = [
            models.UniqueConstraint(
                fields=["species", "name"], name="unique_weapon_per_species"
            ),
        ]

    def __str__(self):
        return f"{self.name} ({self.species})"


class Equipment(models.Model):
    """
    Ein Ausruestungsgegenstand im Katalog einer Spezies. Soll spaeter Werte
    der Einheit veraendern -- die Modifikatoren dafuer kommen erst mit
    diesem Schritt dazu, bis dahin traegt er nur seinen Namen.
    """

    species = models.ForeignKey(
        Species, on_delete=models.PROTECT, related_name="equipment"
    )
    name = models.CharField(max_length=255)
    cost = models.PositiveIntegerField()

    class Meta:
        ordering = ["species", "name"]
        constraints = [
            models.UniqueConstraint(
                fields=["species", "name"], name="unique_equipment_per_species"
            ),
        ]

    def __str__(self):
        return f"{self.name} ({self.species})"


class Army(models.Model):
    """
    Eine Armee eines Spielers. Der Name ist nur innerhalb der Armeen
    desselben Spielers eindeutig.
    """

    player = models.ForeignKey(Player, on_delete=models.CASCADE, related_name="armies")
    name = models.CharField(max_length=255)
    species = models.ForeignKey(Species, on_delete=models.PROTECT, related_name="armies")

    class Meta:
        ordering = ["name"]
        constraints = [
            models.UniqueConstraint(
                fields=["player", "name"], name="unique_army_name_per_player"
            ),
        ]

    def __str__(self):
        return f"{self.name} ({self.player})"

    def total_cost(self):
        return sum(unit.total_cost() for unit in self.units.all())


def armies_with_costs():
    """
    Armeen samt allem, was fuer Army.total_cost und die Ausgabe noetig ist
    -- inklusive der Katalogeintraege mit ihren Preisen, damit keine Abfrage
    je Einheit, Entitaet oder Slot entsteht.
    """
    return Army.objects.select_related("species").prefetch_related(
        # Der Einheitentyp gleich mit -- fuer die Spieldaten beim Start
        # (instance_client.game_start_document) und die Ausgabe der Armee.
        models.Prefetch("units", queryset=Unit.objects.select_related("unit_type")),
        models.Prefetch(
            "units__entities",
            queryset=UnitEntity.objects.select_related("profile").prefetch_related(
                models.Prefetch(
                    "weapons", queryset=UnitEntityWeapon.objects.select_related("weapon")
                ),
                models.Prefetch(
                    "equipment",
                    queryset=UnitEntityEquipment.objects.select_related("equipment"),
                ),
            ),
        ),
    )


class Unit(models.Model):
    """
    Eine Einheit in einer Armee: ein Einheitentyp und 1 bis MAX_UNIT_ENTITIES
    Entitaeten. Alle Waffen, auch schwere und super-schwere, tragen die
    Entitaeten (siehe UnitEntityWeapon).
    """

    army = models.ForeignKey(Army, on_delete=models.CASCADE, related_name="units")
    position = models.PositiveSmallIntegerField()
    unit_type = models.ForeignKey(UnitType, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["army", "position"]

    def total_cost(self):
        return sum(entity.total_cost() for entity in self.entities.all())


class UnitEntity(models.Model):
    """
    Eine einzelne Entitaet einer Einheit. Ihre Werte kommen aus dem Profil;
    Waffen und Ausruestung liegen in nummerierten Slots (UnitEntityWeapon,
    UnitEntityEquipment), damit dieselbe Waffe auch zweimal getragen werden
    kann -- ein ManyToManyField ohne Slot liesse das nicht zu.
    """

    unit = models.ForeignKey(Unit, on_delete=models.CASCADE, related_name="entities")
    position = models.PositiveSmallIntegerField()
    profile = models.ForeignKey(EntityProfile, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["unit", "position"]

    def total_cost(self):
        return (
            self.profile.cost
            + sum(slot.weapon.cost for slot in self.weapons.all())
            + sum(slot.equipment.cost for slot in self.equipment.all())
        )


class UnitEntityWeapon(models.Model):
    """
    Eine Waffe einer Entitaet in einem nummerierten Slot ihrer Klasse.
    weapon_class ist die Klasse des Slots, nicht der Waffe: Slot 0 der
    Standard- und Slot 0 der schweren Waffen sind zwei verschiedene Slots.
    """

    entity = models.ForeignKey(UnitEntity, on_delete=models.CASCADE, related_name="weapons")
    weapon_class = models.CharField(
        max_length=20, choices=WeaponClass.choices, default=WeaponClass.STANDARD
    )
    slot = models.PositiveSmallIntegerField()
    weapon = models.ForeignKey(Weapon, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["entity", "weapon_class", "slot"]
        constraints = [
            models.UniqueConstraint(
                fields=["entity", "weapon_class", "slot"], name="unique_weapon_slot_per_entity"
            ),
        ]


class UnitEntityEquipment(models.Model):
    entity = models.ForeignKey(UnitEntity, on_delete=models.CASCADE, related_name="equipment")
    slot = models.PositiveSmallIntegerField()
    equipment = models.ForeignKey(Equipment, on_delete=models.PROTECT, related_name="+")

    class Meta:
        ordering = ["entity", "slot"]
        constraints = [
            models.UniqueConstraint(
                fields=["entity", "slot"], name="unique_equipment_slot_per_entity"
            ),
        ]
