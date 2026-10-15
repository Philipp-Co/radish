#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/control/commands.h>
#include <stddef.h>
#include <stdio.h>

///
/// Das Aufstellen einer Einheit aus der Reserve (RAD_COMMAND_TYPE_DEPLOY_UNIT):
/// die Fabrik, die Regel und das Ausfuehren -- alles, was zu diesem Kommando
/// gehoert, an einer Stelle.
///
/// Die Fabriken aller Kommandos fuellen den Kopf gleich: Art, die naechste
/// Sequenznummer und den Absender aus game->local_user. Ein abgelehnter Aufruf
/// fasst "output" nicht an und verbraucht keine Sequenznummer -- sonst haette
/// die Reihe des Absenders eine Luecke, die die Gegenseite fuer ein verlorenes
/// Kommando halten muesste.
///

bool RAD_GameDeployUnit(RAD_Game_t *game, RAD_UnitId_t unit, int16_t x, int16_t y, RAD_Command_t *output)
{
    // Erst pruefen, dann schreiben: ein abgelehnter Aufruf fasst "output" nicht an
    // und verbraucht keine Sequenznummer.
    if((game == NULL) || (output == NULL) || (unit < 0))
    {
        return false;
    }

    output->header.type = RAD_COMMAND_TYPE_DEPLOY_UNIT;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.deploy_unit.unit = unit;
    output->command.deploy_unit.x = x;
    output->command.deploy_unit.y = y;

    return true;
}

RAD_GameResult_t RAD_GameCheckDeployUnit(const RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit, int32_t x, int32_t y)
{
    // In der Reihenfolge der Fragen: wer, was, wohin -- und vorher, ob ueberhaupt.
    if(RAD_GameIsFinished(game))
    {
        return RAD_GAME_ERROR_FINISHED;
    }
    if(user == RAD_USER_NONE)
    {
        return RAD_GAME_ERROR_NO_USER;
    }
    if(!RAD_GameIsPlaying(game, user))
    {
        return RAD_GAME_ERROR_NOT_PLAYING;
    }

    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    if(candidate == NULL)
    {
        return RAD_GAME_ERROR_NO_UNIT;
    }
    if(candidate->owner != user)
    {
        return RAD_GAME_ERROR_NOT_OWNED;
    }
    if(candidate->state != RAD_UNIT_STATE_RESERVE)
    {
        return RAD_GAME_ERROR_NOT_IN_RESERVE;
    }

    if(!RAD_WorldInBounds(&game->world, x, y))
    {
        return RAD_GAME_ERROR_OUT_OF_BOUNDS;
    }
    if(!RAD_GameInDeployZone(game, user, x, y))
    {
        return RAD_GAME_ERROR_OUTSIDE_DEPLOY_ZONE;
    }
    const RAD_Tile_t *tile = &game->world.tiles[y][x];
    if(tile->type == RAD_TILE_TYPE_VOID)
    {
        return RAD_GAME_ERROR_NO_GROUND;
    }
    // Wo keine Einheit hinziehen darf, wird auch keine hingestellt.
    if(tile->terrain.impassable)
    {
        return RAD_GAME_ERROR_IMPASSABLE;
    }
    if(tile->unit != RAD_UNIT_NONE)
    {
        return RAD_GAME_ERROR_OCCUPIED;
    }

    return RAD_GAME_OK;
}

///
/// Stellt eine Einheit aus der Reserve auf. Die Regel steht in
/// RAD_GameCheckDeployUnit, und sie gilt hier noch einmal, obwohl der Server sie
/// schon gefragt hat: das Spiel schreibt nur, was es selbst geprueft hat -- ein
/// Kommando kann auch an der Vorpruefung vorbei hier ankommen.
///
/// Abgelehnt wird still, wie ein Zug, der nicht geht: die Welt bleibt, wie sie war,
/// und es gibt kein Ereignis. Gelingt es, meldet die Welt die Einheit wie einen
/// Spawn (RAD_WorldDeployUnit).
///
void RAD_GameHandleDeployCommand(RAD_Game_t *game, RAD_UserId_t user, const RAD_CommandDeployUnit_t *deploy)
{
    const RAD_GameResult_t allowed = RAD_GameCheckDeployUnit(game, user, deploy->unit, deploy->x, deploy->y);
    if(allowed != RAD_GAME_OK)
    {
        printf("Unit with Id %i not deployed: %s\n", deploy->unit, RAD_GameResultText(allowed));
        return;
    }

    // Im Zug ihres Aufstellens darf sie weder ziehen noch angreifen. Gesetzt
    // vor dem Aufstellen: die Welt meldet sie dabei (RAD_WorldDeployUnit), und
    // ein Abonnent soll sie schon so sehen. Die Pruefung oben hat sichergestellt,
    // dass es sie gibt.
    RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, deploy->unit);
    unit->turn.deployed = 1;

    if(!RAD_WorldDeployUnit(&game->world, deploy->unit, deploy->x, deploy->y))
    {
        unit->turn.deployed = 0;
        return;
    }

    // Auf dem Feld steht sie in keiner Reserve mehr. Die Pruefung oben hat
    // sichergestellt, dass sie "user" gehoert und er mitspielt.
    RAD_Player_t *player = RAD_GameFindPlayer(game, user);
    RAD_ReserveRemoveUnit(RAD_PlayerReserve(player), unit);
}
