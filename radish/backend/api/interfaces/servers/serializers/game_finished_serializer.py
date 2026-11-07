from typing import Any, TypedDict

from rest_framework import serializers


# Die validierten Daten von GameFinishedSerializer. Funktionale Schreibweise,
# weil "game-instance" kein gueltiger Python-Name ist.
GameFinishedData = TypedDict(
    "GameFinishedData",
    {"winner": str | None, "loser": str | None, "game-instance": str},
)


class GameFinishedSerializer(serializers.Serializer):
    """
    Eingabe fuer das Ende eines Spiels, wie die Spielinstanz es meldet (siehe
    server_views.GameFinishedView): wer gewonnen und wer verloren hat, je
    ueber die Spieler-Id (Player.identifier), und welche Instanz es meldet
    (GameServer.name). Bei einem Unentschieden sind winner und loser beide
    null -- nur einer von beiden null ist kein Ergebnis.

    "game-instance" ist kein gueltiger Python-Name und wird deshalb in
    __init__ eingehaengt statt als Klassenattribut. Ob die Ids zu den
    Spielern des Spiels gehoeren, prueft die View -- das Spiel kennt nur sie.
    """

    # Pflicht, aber null erlaubt: fehlen darf keiner, null heisst unentschieden.
    winner = serializers.CharField(max_length=8, allow_null=True)
    loser = serializers.CharField(max_length=8, allow_null=True)

    # Any: wird unveraendert an DRF Serializer.__init__ weitergereicht.
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.fields["game-instance"] = serializers.CharField(max_length=255)

    # Any: Signatur von DRF Serializer.validate (Stubs); enger waere eine
    # Liskov-Verletzung.
    def validate(self, attrs: dict[str, Any]) -> dict[str, Any]:
        winner: str | None
        loser: str | None
        winner, loser = attrs["winner"], attrs["loser"]
        if (winner is None) != (loser is None):
            raise serializers.ValidationError(
                "winner und loser sind entweder beide gesetzt oder bei einem Unentschieden beide null."
            )
        if winner is not None and winner == loser:
            raise serializers.ValidationError("winner und loser muessen verschiedene Spieler sein.")
        return attrs
