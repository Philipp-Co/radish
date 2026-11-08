from rest_framework import serializers

from api.models import GameServer


class GameServerSerializer(serializers.ModelSerializer):
    class Meta:
        model = GameServer
        fields = ["id", "name", "address", "port", "is_occupied", "control_url", "instance_name"]
