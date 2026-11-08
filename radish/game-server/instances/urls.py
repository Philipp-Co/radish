from django.urls import path

from .interfaces.game_cancel_view import GameCancelView
from .interfaces.game_start_view import GameStartView

app_name = "instances"

urlpatterns = [
    path(
        "instances/<str:name>/game/start/",
        GameStartView.as_view(),
        name="game-start",
    ),
    path(
        "instances/<str:name>/game/cancel/",
        GameCancelView.as_view(),
        name="game-cancel",
    ),
]
