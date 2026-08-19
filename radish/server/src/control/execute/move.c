#include "move.h"

#include <radish/server/control/execute.h>
#include <radish/game/model/world/world.h>

#include <stddef.h>

///
/// Gezogen wird ueber RAD_WorldMoveEntity und nicht ueber die Tiles selbst: nur
/// die Welt haelt RAD_Entity_t.x/y und RAD_Tile_t.entity synchron und
/// veroeffentlicht das Ereignis. Ein abgelehnter Zug laesst sie garantiert
/// unveraendert (siehe world.h) -- deshalb steht hier keine eigene
/// Rueckabwicklung.
///
/// **from_x/from_y werden nicht geprueft.** Sie stehen im Kommando, aber das Ziel
/// ist absolut, und genau daran haengt die Zusicherung aus command.h: ein
/// wiederholt zugestelltes Kommando fuehrt zum selben Ergebnis wie ein einmal
/// zugestelltes. Wuerde hier verlangt, dass die Figur noch auf dem Startfeld
/// steht, waere die Wiederholung ein Fehler -- und die Strecke darf doppelt
/// zustellen.
///

uint32_t RAD_ControlExecuteMoveCommand(const RAD_Command_t *command, RAD_Game_t *game, RAD_UserId_t user)
{
    (void)user;

    const RAD_EntityId_t entity = command->command.move_entity.entity;
    const int32_t x = command->command.move_entity.to_x;
    const int32_t y = command->command.move_entity.to_y;

    if(RAD_WorldEntityById(&game->world, entity) == NULL)
    {
        return (uint32_t)RAD_CONTROL_ERROR_NO_SUCH_ENTITY;
    }

    if(!RAD_WorldInBounds(&game->world, x, y))
    {
        return (uint32_t)RAD_CONTROL_ERROR_OUT_OF_BOUNDS;
    }

    // Ab hier kann RAD_WorldMoveEntity nur noch aus einem Grund ablehnen: auf dem
    // Zielfeld steht schon jemand. Die Figur gibt es, das Feld liegt in der Welt,
    // und derselbe Platz noch einmal ist dort ausdruecklich in Ordnung.
    if(!RAD_WorldMoveEntity(&game->world, entity, x, y))
    {
        return (uint32_t)RAD_CONTROL_ERROR_TARGET_OCCUPIED;
    }

    return (uint32_t)RAD_CONTROL_OK;
}
