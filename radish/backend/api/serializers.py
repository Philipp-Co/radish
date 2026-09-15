from rest_framework import serializers

from .models import Game, GameServer


class GameServerSerializer(serializers.ModelSerializer):
    class Meta:
        model = GameServer
        fields = ["id", "name", "ip_address", "port", "is_occupied"]


class GameListSerializer(serializers.ModelSerializer):
    """
    Eintrag in der Liste offener Spiele: Name und Spielerzahl.

    host ist Pflicht (zaehlt also immer als ein Spieler), second_player
    ist erst gesetzt, sobald jemand beigetreten ist -- die Spielerzahl ist
    deshalb 1 oder 2.
    """

    player_count = serializers.SerializerMethodField()

    class Meta:
        model = Game
        fields = ["name", "player_count"]

    def get_player_count(self, obj):
        return 2 if obj.second_player_id else 1


class GameCreateSerializer(serializers.Serializer):
    """
    Eingabe fuer das Erstellen eines Spiels: Name und Passwort. Wer der
    Ersteller ist, kommt nicht mehr vom Client (kein player_identifier
    mehr) -- die View ermittelt den Player ueber request.user, also ueber
    das validierte Access-Token. server und host werden ebenfalls von der
    View zugewiesen/aufgeloest, nicht direkt vom Client gesetzt.
    """

    name = serializers.CharField()
    password = serializers.CharField()


class GameJoinSerializer(serializers.Serializer):
    """
    Eingabe fuer das Beitreten: welches Spiel (Name) und dessen Passwort.
    Wer beitritt, kommt wie bei GameCreateSerializer nicht mehr vom Client,
    sondern aus request.user (siehe client_views.GameJoinView).
    """

    name = serializers.CharField()
    password = serializers.CharField()


class GameDetailSerializer(serializers.ModelSerializer):
    """
    Antwort auf ein erstelltes/beigetretenes Spiel. Ohne password -- die
    muss der Client ohnehin schon kennen, es besteht kein Grund, sie in
    der Antwort zu wiederholen. Bewusst auch ohne den belegten Server:
    IP und Port eines GameServer sollen ueber diese Endpunkte nicht nach
    aussen dringen (die Verbindung zum Server laeuft ja ueber Django,
    siehe client_views.CommandView -- der Client braucht sie dafuer nicht).

    host/second_player sind Player-Fremdschluessel -- nach aussen aber
    weiterhin einfach deren Kennung (identifier), keine verschachtelten
    Player-Objekte, da Player ohnehin nichts weiter als die Kennung traegt.
    """

    host_identifier = serializers.SlugRelatedField(
        source="host", slug_field="identifier", read_only=True
    )
    second_player_identifier = serializers.SlugRelatedField(
        source="second_player", slug_field="identifier", read_only=True
    )

    class Meta:
        model = Game
        fields = ["id", "name", "host_identifier", "second_player_identifier"]


class CommandSerializer(serializers.Serializer):
    """
    Eingabe fuer ein Kommando: nur noch die Nachricht selbst (Rohformat
    noch offen -- aktuell ein einfacher Text, der 1:1 als UDP-Payload
    rausgeht). Der sendende Spieler kommt nicht mehr vom Client (kein
    player_identifier mehr), sondern aus request.user (siehe
    client_views.CommandView).
    """

    message = serializers.CharField()
