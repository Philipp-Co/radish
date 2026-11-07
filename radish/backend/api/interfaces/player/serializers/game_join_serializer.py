from rest_framework import serializers


class GameJoinSerializer(serializers.Serializer):
    """
    Eingabe fuer das Beitreten: welches Spiel (Name) und dessen Passwort.
    Wer beitritt, kommt wie bei GameCreateSerializer nicht mehr vom Client,
    sondern aus request.user (siehe client_views.GameJoinView). army_id: die
    Armee, mit der er antritt -- sie muss zum Punktelimit des Spiels passen.
    """

    name = serializers.CharField()
    password = serializers.CharField()
    army_id = serializers.IntegerField()
