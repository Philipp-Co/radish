#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/game_definitions.h>
#include <stddef.h>

///
/// Die Siegbedingungen (game.h): die Wertung nach Punkten am Ende der letzten
/// Runde und die Niederlage dessen, der keine Einheit mehr hat. Beides endet im
/// selben Zustand -- "finished", "winner", "end_reason" (model/game.h) --, und nur
/// diese Datei setzt ihn.
///

static void RAD_GameFinish(RAD_Game_t *game, RAD_UserId_t winner, RAD_GameEndReason_t reason);
static bool RAD_GameUnitScores(const RAD_Game_t *game, RAD_UnitId_t id, RAD_UserId_t user);
static bool RAD_GamePlayerHasUnits(const RAD_Player_t *player);

int32_t RAD_GameRound(const RAD_Game_t *game)
{
    if(game == NULL)
    {
        return 0;
    }
    // Nach der letzten Runde zaehlt der Zug weiter, das Spiel nicht.
    const int32_t round = RAD_TurnRound(&game->turn);
    return (round > RAD_GAME_NUMBER_OF_ROUNDS) ? RAD_GAME_NUMBER_OF_ROUNDS : round;
}

int32_t RAD_GameNumberOfRounds(const RAD_Game_t *game)
{
    return (game == NULL) ? 0 : RAD_GAME_NUMBER_OF_ROUNDS;
}

bool RAD_GameIsFinished(const RAD_Game_t *game)
{
    return (game != NULL) && game->finished;
}

RAD_UserId_t RAD_GameWinner(const RAD_Game_t *game)
{
    return (game == NULL) ? RAD_USER_NONE : game->winner;
}

RAD_GameEndReason_t RAD_GameEndReason(const RAD_Game_t *game)
{
    return (game == NULL) ? RAD_GAME_END_NONE : game->end_reason;
}

int32_t RAD_GameScore(const RAD_Game_t *game, RAD_UserId_t user)
{
    if(game == NULL || user == RAD_USER_NONE)
    {
        return 0;
    }

    static const int32_t neighbours[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

    int32_t score = 0;
    for(int32_t y = 0; y < game->world.height; ++y)
    {
        for(int32_t x = 0; x < game->world.width; ++x)
        {
            const RAD_Tile_t *tile = &game->world.tiles[y][x];
            if(!tile->victory)
            {
                continue;
            }

            if(RAD_GameUnitScores(game, tile->unit, user))
            {
                score += RAD_VICTORY_TILE_POINTS;
            }

            // Jeder Nachbar fuer sich: wer das Feld nicht nehmen kann, kommt so
            // wenigstens heran.
            for(int32_t n = 0; n < 4; ++n)
            {
                const int32_t nx = x + neighbours[n][0];
                const int32_t ny = y + neighbours[n][1];
                if(RAD_WorldInBounds(&game->world, nx, ny)
                   && RAD_GameUnitScores(game, game->world.tiles[ny][nx].unit, user))
                {
                    score += RAD_VICTORY_NEIGHBOUR_POINTS;
                }
            }
        }
    }

    return score;
}

bool RAD_GameFinishAfterLastRound(RAD_Game_t *game)
{
    if(game->finished || RAD_TurnRound(&game->turn) <= RAD_GAME_NUMBER_OF_ROUNDS)
    {
        return false;
    }

    // Wer am meisten hat, gewinnt -- haben zwei gleich viel, niemand.
    RAD_UserId_t winner = RAD_USER_NONE;
    int32_t best = -1;
    bool tied = false;
    for(int32_t i = 0; i < RAD_GameNumberOfPlayers(game); ++i)
    {
        const RAD_UserId_t user = RAD_GamePlayerAt(game, i);
        const int32_t score = RAD_GameScore(game, user);
        if(score > best)
        {
            best = score;
            winner = user;
            tied = false;
        }
        else if(score == best)
        {
            tied = true;
        }
    }

    RAD_GameFinish(game, tied ? RAD_USER_NONE : winner, RAD_GAME_END_ROUNDS);
    return true;
}

void RAD_GameDestroyUnit(RAD_Game_t *game, RAD_UnitId_t unit)
{
    const RAD_Unit_t *destroyed = RAD_UnitPoolUnitById(game->unit_pool, unit);
    if(destroyed == NULL || destroyed->state == RAD_UNIT_STATE_DESTROYED)
    {
        return;
    }
    const RAD_UserId_t owner = destroyed->owner;

    RAD_WorldRemoveUnit(&game->world, unit);

    // Nur hier kann der Bestand eines Spielers auf null fallen. Spielt der
    // Besitzer nicht (mehr) mit, gibt es niemanden, der verliert.
    const RAD_Player_t *player = RAD_GameFindPlayer(game, owner);
    if(game->finished || player == NULL || RAD_GamePlayerHasUnits(player))
    {
        return;
    }

    // Gewonnen hat der andere; bleibt keiner, niemand.
    RAD_UserId_t winner = RAD_USER_NONE;
    for(int32_t i = 0; i < RAD_GameNumberOfPlayers(game); ++i)
    {
        const RAD_UserId_t user = RAD_GamePlayerAt(game, i);
        if(user != owner)
        {
            winner = user;
            break;
        }
    }
    RAD_GameFinish(game, winner, RAD_GAME_END_ELIMINATED);
}

///
/// Setzt den Endzustand und meldet ihn -- einmal.
///
static void RAD_GameFinish(RAD_Game_t *game, RAD_UserId_t winner, RAD_GameEndReason_t reason)
{
    game->finished = true;
    game->winner = winner;
    game->end_reason = reason;
    RAD_EventManagerPublishGameFinished(game->event_manager, winner, reason);
}

///
/// Punktet die Einheit "id" fuer "user"? Sie steht auf dem Feld, gehoert ihm und
/// kann punkten (RAD_Unit_t.can_capture). false fuer RAD_UNIT_NONE.
///
static bool RAD_GameUnitScores(const RAD_Game_t *game, RAD_UnitId_t id, RAD_UserId_t user)
{
    if(id == RAD_UNIT_NONE)
    {
        return false;
    }
    const RAD_Unit_t *unit = RAD_UnitPoolUnitById(game->unit_pool, id);
    return (unit != NULL)
        && (unit->state == RAD_UNIT_STATE_DEPLOYED)
        && (unit->owner == user)
        && unit->can_capture;
}

///
/// Hat der Spieler noch eine Einheit, in der Reserve oder auf dem Feld? Seine
/// Liste fuehrt auch die zerstoerten (player/player.h).
///
static bool RAD_GamePlayerHasUnits(const RAD_Player_t *player)
{
    for(int32_t i = 0; i < RAD_PlayerNumberOfUnits(player); ++i)
    {
        const RAD_Unit_t *unit = RAD_PlayerUnitAt(player, i);
        if(unit->state == RAD_UNIT_STATE_RESERVE || unit->state == RAD_UNIT_STATE_DEPLOYED)
        {
            return true;
        }
    }
    return false;
}
