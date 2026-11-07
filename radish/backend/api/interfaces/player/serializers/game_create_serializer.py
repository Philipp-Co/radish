from rest_framework import serializers


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
    # Mit wie vielen Punkten gespielt wird, und die Armee des Erstellers --
    # die muss dazu passen (siehe client_views._army_for_game).
    points_limit = serializers.IntegerField(min_value=1)
    army_id = serializers.IntegerField()
