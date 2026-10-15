#include <radish/io/net_event_handler.h>

#include <stdio.h>

///
/// Eine Einheit ist zerstoert -- gleich, wessen: ins Log und aus der Welt
/// (RAD_ClientWorldRemoveUnit). Das Feld, auf dem sie stand, wird frei.
///
void RAD_IoNetOnUnitDestroyed(void *user_argument, const RAD_NetUnitDestroyed_t *destroyed)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    printf("<- Einheit %d auf (%d, %d) zerstoert\n",
           (int)destroyed->entity, (int)destroyed->position.x, (int)destroyed->position.y);

    const RAD_ClientPosition_t position = { .x = destroyed->position.x, .y = destroyed->position.y };
    if(!RAD_ClientWorldRemoveUnit(context->world, destroyed->entity, position))
    {
        printf("   Einheit %d ist dem Client nicht bekannt\n", (int)destroyed->entity);
    }
}
