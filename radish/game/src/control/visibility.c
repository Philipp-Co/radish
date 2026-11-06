#include <radish/game/control/visibility.h>
#include <radish/game/control/unit_turn.h>
#include <radish/game/model/game.h>
#include <stddef.h>
#include <string.h>

bool RAD_GameUnitSeesTile(const RAD_Game_t *game, RAD_UnitId_t unit, int32_t x, int32_t y)
{
    if((game == NULL) || !RAD_WorldInBounds(&game->world, x, y))
    {
        return false;
    }

    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    if((candidate == NULL) || (candidate->state != RAD_UNIT_STATE_DEPLOYED))
    {
        return false;
    }

    // Erst die Entfernung, dann die Linie: die ist der teure Teil.
    if(RAD_GameDistance(candidate, x, y) > candidate->sight_range)
    {
        return false;
    }
    return RAD_GameHasLineOfSight(game, candidate->x, candidate->y, x, y);
}

bool RAD_GameIsTileVisible(const RAD_Game_t *game, RAD_UserId_t user, int32_t x, int32_t y)
{
    if((game == NULL) || (user == RAD_USER_NONE))
    {
        return false;
    }

    // Ueber den Pool und nicht ueber eine Liste je Benutzer: der Besitzer steht in
    // der Einheit (unit.h), und bei hoechstens RAD_MAX_UNITS Plaetzen ist die
    // Suche billiger als jede Buchfuehrung daneben.
    const int32_t number_of_units = RAD_UnitPoolNumberOfUnits(game->unit_pool);
    for(int32_t i = 0; i < number_of_units; ++i)
    {
        const RAD_Unit_t *unit = RAD_UnitPoolUnitAt(game->unit_pool, i);
        if((unit != NULL) && (unit->owner == user) && RAD_GameUnitSeesTile(game, unit->id, x, y))
        {
            return true;
        }
    }
    return false;
}

bool RAD_GameIsUnitVisible(const RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit)
{
    if((game == NULL) || (user == RAD_USER_NONE))
    {
        return false;
    }

    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    if(candidate == NULL)
    {
        return false;
    }
    if(candidate->owner == user)
    {
        return true;
    }
    return (candidate->state == RAD_UNIT_STATE_DEPLOYED)
        && RAD_GameIsTileVisible(game, user, candidate->x, candidate->y);
}

bool RAD_GameVisibility(const RAD_Game_t *game, RAD_UserId_t user, RAD_Visibility_t *output)
{
    if((game == NULL) || (output == NULL))
    {
        return false;
    }

    // Leer anfangen: was hinter der Welt liegt, bleibt false.
    memset(output, 0, sizeof(*output));
    output->width = game->world.width;
    output->height = game->world.height;

    // Je Feld dieselbe Frage wie von aussen -- so kann das Raster nie etwas
    // anderes sagen als RAD_GameIsTileVisible.
    for(int32_t y = 0; y < output->height; ++y)
    {
        for(int32_t x = 0; x < output->width; ++x)
        {
            output->tiles[y][x] = RAD_GameIsTileVisible(game, user, x, y);
        }
    }
    return true;
}

bool RAD_GameRecipientsForTile(const RAD_Game_t *game, RAD_UserId_t owner, int32_t x, int32_t y,
                               RAD_Recipients_t *output)
{
    if((game == NULL) || (output == NULL))
    {
        return false;
    }

    *output = (RAD_Recipients_t){ .number_of_users = 0 };
    RAD_RecipientsAdd(output, owner);

    for(int32_t i = 0; i < RAD_GameNumberOfPlayers(game); ++i)
    {
        const RAD_UserId_t user = RAD_GamePlayerAt(game, i);
        if(RAD_GameIsTileVisible(game, user, x, y))
        {
            RAD_RecipientsAdd(output, user);
        }
    }
    return true;
}

int32_t RAD_GameVisiblePartOfPath(const RAD_Game_t *game, RAD_UserId_t user, const RAD_Path_t *path,
                                  RAD_Path_t *output)
{
    if(output == NULL)
    {
        return 0;
    }
    memset(output, 0, sizeof(*output));
    if((game == NULL) || (path == NULL))
    {
        return 0;
    }

    for(int32_t i = 0; (i < path->number_of_steps) && (i < RAD_PATH_MAX_STEPS); ++i)
    {
        if(RAD_GameIsTileVisible(game, user, path->steps_to[i].x, path->steps_to[i].y))
        {
            output->steps_to[output->number_of_steps++] = path->steps_to[i];
        }
    }
    return output->number_of_steps;
}

int32_t RAD_GameObservedPartOfPath(const RAD_Game_t *game, RAD_UserId_t user, const RAD_Path_t *path,
                                   RAD_Path_t *output)
{
    if(output == NULL)
    {
        return 0;
    }
    memset(output, 0, sizeof(*output));
    if((game == NULL) || (path == NULL))
    {
        return 0;
    }

    const int32_t number_of_steps = (path->number_of_steps < RAD_PATH_MAX_STEPS)
        ? path->number_of_steps : RAD_PATH_MAX_STEPS;

    // Erst und letztes sichtbares Feld; dazwischen bleiben Luecken, wie sie sind.
    int32_t first = -1;
    int32_t last = -1;
    bool seen[RAD_PATH_MAX_STEPS] = { false };
    for(int32_t i = 0; i < number_of_steps; ++i)
    {
        seen[i] = RAD_GameIsTileVisible(game, user, path->steps_to[i].x, path->steps_to[i].y);
        if(seen[i])
        {
            first = (first < 0) ? i : first;
            last = i;
        }
    }
    if(first < 0)
    {
        return 0;
    }

    // Je ein Randfeld davor und danach, soweit der Weg sie hat.
    const int32_t from = (first > 0) ? first - 1 : first;
    const int32_t to = (last < number_of_steps - 1) ? last + 1 : last;
    for(int32_t i = from; i <= to; ++i)
    {
        if(seen[i] || (i == from) || (i == to))
        {
            output->steps_to[output->number_of_steps++] = path->steps_to[i];
        }
    }
    return output->number_of_steps;
}

void RAD_GameTakeSightSnapshot(const RAD_Game_t *game, RAD_SightSnapshot_t *output)
{
    memset(output, 0, sizeof(*output));
    if(game == NULL)
    {
        return;
    }

    output->number_of_players = RAD_GameNumberOfPlayers(game);
    for(int32_t p = 0; p < output->number_of_players; ++p)
    {
        output->players[p] = RAD_GamePlayerAt(game, p);
    }

    // Je Platz im Pool: der Pool gibt nie einen frei (unit_pool.h), Platz i ist
    // nach dem Kommando also dieselbe Einheit wie davor.
    output->number_of_units = RAD_UnitPoolNumberOfUnits(game->unit_pool);
    for(int32_t i = 0; i < output->number_of_units; ++i)
    {
        const RAD_Unit_t *unit = RAD_UnitPoolUnitAt(game->unit_pool, i);
        output->state[i] = unit->state;
        output->position[i] = (RAD_Position_t){ .x = unit->x, .y = unit->y };
        for(int32_t p = 0; p < output->number_of_players; ++p)
        {
            output->seen[p][i] = RAD_GameIsUnitVisible(game, output->players[p], unit->id);
        }
    }
}

///
/// Wo "observer" die Einheit zuletzt sah, die er jetzt nicht mehr sieht -- die
/// Regel steht in visibility.h bei RAD_GameNotifySightChanges.
///
static RAD_Position_t RAD_GameLastSeenAt(const RAD_Game_t *game, RAD_UserId_t observer, const RAD_Unit_t *unit,
                                         RAD_Position_t before, RAD_UnitId_t moved, const RAD_Path_t *path)
{
    if((unit->id != moved) || (path == NULL))
    {
        return before;
    }

    // Gegangen ist sie den Anfang des Weges bis zu ihrem jetzigen Feld.
    RAD_Path_t walked = { .number_of_steps = 0 };
    for(int32_t i = 0; (i < path->number_of_steps) && (i < RAD_PATH_MAX_STEPS); ++i)
    {
        walked.steps_to[walked.number_of_steps++] = path->steps_to[i];
        if((path->steps_to[i].x == unit->x) && (path->steps_to[i].y == unit->y))
        {
            break;
        }
    }

    RAD_Path_t seen;
    if(RAD_GameVisiblePartOfPath(game, observer, &walked, &seen) == 0)
    {
        return before;
    }
    return seen.steps_to[seen.number_of_steps - 1];
}

void RAD_GameNotifySightChanges(RAD_Game_t *game, const RAD_SightSnapshot_t *before,
                                RAD_UnitId_t moved, const RAD_Path_t *path)
{
    if((game == NULL) || (before == NULL))
    {
        return;
    }

    for(int32_t i = 0; i < before->number_of_units; ++i)
    {
        const RAD_Unit_t *unit = RAD_UnitPoolUnitAt(game->unit_pool, i);
        if((unit == NULL)
           || (before->state[i] != RAD_UNIT_STATE_DEPLOYED)
           || (unit->state != RAD_UNIT_STATE_DEPLOYED))
        {
            continue;
        }

        RAD_Recipients_t sighted = { .number_of_users = 0 };
        for(int32_t p = 0; p < before->number_of_players; ++p)
        {
            const RAD_UserId_t observer = before->players[p];
            const bool now = RAD_GameIsUnitVisible(game, observer, unit->id);
            if(now && !before->seen[p][i])
            {
                RAD_RecipientsAdd(&sighted, observer);
            }
            else if(!now && before->seen[p][i])
            {
                const RAD_Position_t at = RAD_GameLastSeenAt(game, observer, unit, before->position[i], moved, path);
                RAD_Recipients_t lost = { .number_of_users = 0 };
                RAD_RecipientsAdd(&lost, observer);
                RAD_WorldNotifyUnitLost(&game->world, unit->id, at.x, at.y, &lost);
            }
        }

        if(sighted.number_of_users > 0)
        {
            RAD_WorldNotifyUnitSighted(&game->world, unit->id, &sighted);
        }
    }
}
