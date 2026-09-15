from django.contrib import admin

from .models import Game, GameServer, Player


@admin.register(GameServer)
class GameServerAdmin(admin.ModelAdmin):
    list_display = ("name", "ip_address", "port", "is_occupied")


@admin.register(Player)
class PlayerAdmin(admin.ModelAdmin):
    list_display = ("identifier",)


@admin.register(Game)
class GameAdmin(admin.ModelAdmin):
    list_display = ("name", "server", "host", "second_player")
