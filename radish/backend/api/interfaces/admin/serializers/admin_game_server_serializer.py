# Die Admin-Serverliste gibt dieselben Felder aus wie die Schnittstelle der
# Spielinstanzen (servers/serializers/game_server_serializer.py), darf sich aber
# unabhaengig von ihr aendern -- wie die anderen Admin- und Spieler-Serializer.
# pylint: disable=duplicate-code

from rest_framework import serializers

from api.models import GameServer


class AdminGameServerSerializer(serializers.ModelSerializer):
    class Meta:
        model = GameServer
        fields = ["id", "name", "address", "port", "is_occupied", "control_url", "instance_name"]
