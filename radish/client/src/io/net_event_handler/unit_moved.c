#include <radish/io/net_event_handler.h>

#include <stdio.h>

///
/// Eine Einheit ist gezogen -- gleich, wessen: ins Log und in die Welt. Das ist
/// der einzige Weg, auf dem ein Zug in die Welt kommt; die Antwort auf das
/// Kommando sagt nur noch, ob es angenommen wurde (command_response.c).
///
/// Versetzt wird den Weg entlang, den die Einheit wirklich gegangen ist
/// (RAD_ClientWorldMoveEntity) -- nicht auf das Ziel, das angefragt war: war der
/// Weg unterwegs versperrt, endet er davor. Die Welt reicht den ganzen Weg an
/// ihre Beobachter weiter ("unit_moved"), damit sich der Zug nachzeichnen
/// laesst.
///
void RAD_IoNetOnUnitMoved(void *user_argument, const RAD_NetUnitMoved_t *moved)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;
    const RAD_NetPath_t *path = &moved->path;

    // Der Codec laesst nur [2, RAD_NET_PATH_MAX_STEPS] durch (net_codec.h); ohne
    // diese Pruefung laese ein anderer Wert ausserhalb von steps_to.
    if((path->number_of_steps < 2) || (path->number_of_steps > RAD_NET_PATH_MAX_STEPS))
    {
        printf("<- Einheit %d gezogen, aber Weg mit %d Feldern verworfen\n",
               (int)moved->entity, (int)path->number_of_steps);
        return;
    }

    const size_t count = (size_t)path->number_of_steps;
    RAD_ClientPosition_t steps[RAD_NET_PATH_MAX_STEPS];
    for(size_t i = 0; i < count; ++i)
    {
        steps[i] = (RAD_ClientPosition_t){ .x = path->steps_to[i].x, .y = path->steps_to[i].y };
    }

    printf("<- Einheit %d gezogen von (%d, %d) nach (%d, %d), %d Schritte\n",
           (int)moved->entity, (int)steps[0].x, (int)steps[0].y,
           (int)steps[count - 1].x, (int)steps[count - 1].y, (int)count - 1);

    RAD_ClientWorldMoveEntity(context->world, moved->entity, steps, count);
}
