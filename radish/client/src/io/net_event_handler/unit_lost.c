#include <radish/io/net_event_handler.h>
#include <stdio.h>

///
/// Eine fremde Einheit ist aus der Sicht verschwunden: ins Log und vom Feld
/// (RAD_ClientWorldHideUnit). Zerstoert ist sie nicht -- die Welt behaelt sie,
/// damit sie bekannt ist, wenn sie wieder in Sicht kommt. "position" ist, wo wir
/// sie zuletzt sahen.
///
void RAD_IoNetOnUnitLost(void *user_argument, const RAD_NetUnitLost_t *lost)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;
    printf("<- Einheit %d aus der Sicht, zuletzt auf (%d, %d)\n",
           (int)lost->entity, (int)lost->position.x, (int)lost->position.y);
    const RAD_ClientPosition_t position = { .x = lost->position.x, .y = lost->position.y };
    if(!RAD_ClientWorldHideUnit(context->world, lost->entity, position))
    {
        printf("   Einheit %d stand auf keinem Feld\n", (int)lost->entity);
    }
}
