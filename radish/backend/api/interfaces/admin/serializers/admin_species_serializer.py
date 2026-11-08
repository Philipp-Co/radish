from rest_framework import serializers

from api.models import Species


class AdminSpeciesSerializer(serializers.ModelSerializer):
    class Meta:
        model = Species
        fields = ["id", "name"]
