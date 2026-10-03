from django.urls import path

from . import admin_views, army_views, auth_views, client_views, server_views

app_name = "api"

urlpatterns = [
    path("servers/", server_views.ServerView.as_view(), name="server-register"),
    path(
        "servers/<str:name>/",
        server_views.ServerView.as_view(),
        name="server-unregister",
    ),
    path("client/game/", client_views.GameCreateView.as_view(), name="client-game-create"),
    path("client/game/start/", client_views.GameStartView.as_view(), name="client-game-start"),
    path("client/game/join/", client_views.GameJoinView.as_view(), name="client-game-join"),
    path("client/game/current/", client_views.CurrentGameView.as_view(), name="client-game-current"),
    path("client/game/leave/", client_views.LeaveGameView.as_view(), name="client-game-leave"),
    path("client/games/", client_views.GameListView.as_view(), name="client-game-list"),
    path("client/state/", client_views.PlayerStateView.as_view(), name="client-player-state"),
    path("client/catalog/", army_views.CatalogView.as_view(), name="client-catalog"),
    path("client/armies/", army_views.ArmyListView.as_view(), name="client-army-list"),
    path(
        "client/armies/<int:army_id>/",
        army_views.ArmyDetailView.as_view(),
        name="client-army-detail",
    ),
    path("admin/servers/", admin_views.AdminServerListView.as_view(), name="admin-server-list"),
    path(
        "admin/catalog/species/",
        admin_views.AdminSpeciesListView.as_view(),
        name="admin-catalog-species-list",
    ),
    path(
        "admin/catalog/species/<int:pk>/",
        admin_views.AdminSpeciesDetailView.as_view(),
        name="admin-catalog-species-detail",
    ),
    path(
        "admin/catalog/unit-types/",
        admin_views.AdminUnitTypeListView.as_view(),
        name="admin-catalog-unit-type-list",
    ),
    path(
        "admin/catalog/unit-types/<int:pk>/",
        admin_views.AdminUnitTypeDetailView.as_view(),
        name="admin-catalog-unit-type-detail",
    ),
    path(
        "admin/catalog/profiles/",
        admin_views.AdminEntityProfileListView.as_view(),
        name="admin-catalog-profile-list",
    ),
    path(
        "admin/catalog/profiles/<int:pk>/",
        admin_views.AdminEntityProfileDetailView.as_view(),
        name="admin-catalog-profile-detail",
    ),
    path(
        "admin/catalog/weapons/",
        admin_views.AdminWeaponListView.as_view(),
        name="admin-catalog-weapon-list",
    ),
    path(
        "admin/catalog/weapons/<int:pk>/",
        admin_views.AdminWeaponDetailView.as_view(),
        name="admin-catalog-weapon-detail",
    ),
    path(
        "admin/catalog/equipment/",
        admin_views.AdminEquipmentListView.as_view(),
        name="admin-catalog-equipment-list",
    ),
    path(
        "admin/catalog/equipment/<int:pk>/",
        admin_views.AdminEquipmentDetailView.as_view(),
        name="admin-catalog-equipment-detail",
    ),
    path("login/", auth_views.LoginView.as_view(), name="login"),
    path("login/callback/", auth_views.LoginCallbackView.as_view(), name="login-callback"),
    path("logout/", auth_views.LogoutView.as_view(), name="logout"),
]
