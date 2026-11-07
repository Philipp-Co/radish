from django.db import models


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
