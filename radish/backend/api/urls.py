from django.urls import path

from . import admin_views, auth_views, client_views, server_views

app_name = "api"

urlpatterns = [
    path("servers/", server_views.ServerView.as_view(), name="server-register"),
    path(
        "servers/<str:name>/",
        server_views.ServerView.as_view(),
        name="server-unregister",
    ),
    path("client/game/", client_views.GameCreateView.as_view(), name="client-game-create"),
    path("client/game/join/", client_views.GameJoinView.as_view(), name="client-game-join"),
    path("client/game/current/", client_views.CurrentGameView.as_view(), name="client-game-current"),
    path("client/game/leave/", client_views.LeaveGameView.as_view(), name="client-game-leave"),
    path("client/games/", client_views.GameListView.as_view(), name="client-game-list"),
    path("client/command/", client_views.CommandView.as_view(), name="client-command"),
    path("client/stream/", client_views.StreamView.as_view(), name="client-stream"),
    path("client/state/", client_views.PlayerStateView.as_view(), name="client-player-state"),
    path("admin/servers/", admin_views.AdminServerListView.as_view(), name="admin-server-list"),
    path("login/", auth_views.LoginView.as_view(), name="login"),
    path("login/callback/", auth_views.LoginCallbackView.as_view(), name="login-callback"),
    path("logout/", auth_views.LogoutView.as_view(), name="logout"),
]
