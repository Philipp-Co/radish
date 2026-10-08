#include <radish/game/model/player/player.h>
#include <radish/game/game_definitions.h>
#include <stdlib.h>
#include <stddef.h>

///
/// Die Struktur steht hier und nicht im Header, wie bei Pool und Reserve.
/// "units" ist vorne mit "number_of_units" Zeigern belegt.
///
struct RAD_Player
{
    RAD_UserId_t id;

    RAD_Unit_t *units[RAD_MAX_UNITS];
    int32_t number_of_units;

    /// Gehoert dem Spieler: angelegt mit ihm, zerstoert mit ihm.
    RAD_Reserve_t *reserve;
};

static int32_t RAD_PlayerIndexOf(const RAD_Player_t *player, const RAD_Unit_t *unit);

RAD_Player_t* RAD_CreatePlayer(RAD_UserId_t id)
{
    if(id == RAD_USER_NONE)
    {
        return NULL;
    }

    RAD_Player_t *player = malloc(sizeof(struct RAD_Player));
    if(player == NULL)
    {
        return NULL;
    }

    player->reserve = RAD_CreateReserve();
    if(player->reserve == NULL)
    {
        free(player);
        return NULL;
    }

    player->id = id;
    player->number_of_units = 0;
    return player;
}

void RAD_DestroyPlayer(RAD_Player_t **player)
{
    if(player == NULL || *player == NULL)
    {
        return;
    }
    RAD_DestroyReserve(&(*player)->reserve);
    free(*player);
    *player = NULL;
}

RAD_UserId_t RAD_PlayerId(const RAD_Player_t *player)
{
    return player == NULL ? RAD_USER_NONE : player->id;
}

RAD_Reserve_t* RAD_PlayerReserve(const RAD_Player_t *player)
{
    return player == NULL ? NULL : player->reserve;
}

bool RAD_PlayerAddUnit(RAD_Player_t *player, RAD_Unit_t *unit)
{
    if(player == NULL || unit == NULL)
    {
        return false;
    }
    if(RAD_PlayerIndexOf(player, unit) >= 0)
    {
        return true;
    }
    if(player->number_of_units >= RAD_MAX_UNITS)
    {
        return false;
    }

    player->units[player->number_of_units] = unit;
    player->number_of_units++;
    return true;
}

bool RAD_PlayerRemoveUnit(RAD_Player_t *player, const RAD_Unit_t *unit)
{
    if(player == NULL || unit == NULL)
    {
        return false;
    }

    const int32_t index = RAD_PlayerIndexOf(player, unit);
    if(index < 0)
    {
        return false;
    }

    for(int32_t i = index + 1; i < player->number_of_units; ++i)
    {
        player->units[i - 1] = player->units[i];
    }
    player->number_of_units--;

    // Was nicht mehr dem Spieler gehoert, steht auch nicht mehr in seiner
    // Reserve. Stand es dort nicht, ist das kein Fehler.
    (void)RAD_ReserveRemoveUnit(player->reserve, unit);
    return true;
}

bool RAD_PlayerOwnsUnit(const RAD_Player_t *player, const RAD_Unit_t *unit)
{
    if(player == NULL || unit == NULL)
    {
        return false;
    }
    return RAD_PlayerIndexOf(player, unit) >= 0;
}

int32_t RAD_PlayerNumberOfUnits(const RAD_Player_t *player)
{
    return player == NULL ? 0 : player->number_of_units;
}

RAD_Unit_t* RAD_PlayerUnitAt(const RAD_Player_t *player, int32_t index)
{
    if(player == NULL || index < 0 || index >= player->number_of_units)
    {
        return NULL;
    }
    return player->units[index];
}

///
/// Die Stelle von "unit" in der Einheitenliste, oder -1.
///
static int32_t RAD_PlayerIndexOf(const RAD_Player_t *player, const RAD_Unit_t *unit)
{
    for(int32_t i = 0; i < player->number_of_units; ++i)
    {
        if(player->units[i] == unit)
        {
            return i;
        }
    }
    return -1;
}
