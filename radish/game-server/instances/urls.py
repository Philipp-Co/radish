from django.urls import path

from . import views

app_name = "instances"

urlpatterns = [
    path(
        "instances/<str:name>/game/start/",
        views.GameStartView.as_view(),
        name="game-start",
    ),
    path(
        "instances/<str:name>/game/cancel/",
        views.GameCancelView.as_view(),
        name="game-cancel",
    ),
]
