# Gleiche Felder wie die Admin-Serverliste (admin/serializers/
# admin_game_server_serializer.py), bewusst getrennt.
# pylint: disable=duplicate-code

from rest_framework import serializers

from api.models import GameServer


class GameServerSerializer(serializers.ModelSerializer):
    class Meta:
        model = GameServer
        fields = ["id", "name", "address", "port", "is_occupied", "control_url", "instance_name"]
