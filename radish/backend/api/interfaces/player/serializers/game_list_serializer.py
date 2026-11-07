from rest_framework import serializers

from ....models import Game


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
        fields = ["name", "player_count", "points_limit"]

    def get_player_count(self, obj: Game) -> int:
        return 2 if obj.second_player_id else 1
