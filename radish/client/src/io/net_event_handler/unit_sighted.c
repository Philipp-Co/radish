#include "common.h"
#include <stdio.h>

///
/// Eine fremde Einheit ist in Sicht gekommen -- der Nebel des Krieges: der
/// Server schickt jedem nur, was er sieht. Ins Log und in die Welt
/// (RAD_ClientWorldShowUnit): die ganze Einheit, denn der Client kennt sie
/// womoeglich noch nicht, und das Feld, auf dem sie steht. Kam sie durch ihren
/// eigenen Zug, ist der Teil des Weges, den wir sahen, schon vorher gekommen
/// (unit_moved.c).
///
void RAD_IoNetOnUnitSighted(void *user_argument, const RAD_NetUnitSighted_t *sighted)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;
    const RAD_NetUnit_t *unit = &sighted->unit;
    printf("<- Einheit %d \"%s\" von 0x%llx in Sicht auf (%d, %d)\n",
           (int)unit->unit,
           unit->name,
           (unsigned long long)unit->owner,
           (int)sighted->position.x,
           (int)sighted->position.y);

    RAD_ClientUnit_t client_unit;
    RAD_IoNetUnitToClient(unit, &client_unit);
    const RAD_ClientPosition_t position = { .x = sighted->position.x, .y = sighted->position.y };
    if(!RAD_ClientWorldShowUnit(context->world, &client_unit, position))
    {
        printf("   Einheit %d nicht aufs Feld gestellt: Feld unbekannt oder belegt, oder kein Platz mehr\n",
               (int)unit->unit);
    }
}
