#include "common.h"

#include <stdio.h>

static bool RAD_IoNetIsInReserve(const RAD_ClientWorld_t *world, RAD_NetUserId_t owner, RAD_NetEntityId_t id);

///
/// Eine Einheit steht jetzt auf dem Feld -- gleich, wer sie aufgestellt hat: ins
/// Log und in die Welt. Das ist der einzige Weg, auf dem ein Deployment in die
/// Welt kommt; die Antwort auf das Kommando sagt nur noch, ob es ging
/// (deploy_response.c).
///
/// Kennt der Client die Einheit noch nicht -- er hat die Reserve nicht abgefragt,
/// oder es ist die eines anderen --, kommt sie erst aus dem Ereignis in die Reserve
/// ihres Besitzers und wird dann aufgestellt. Kennt er sie schon, wird sie mit
/// dem ersetzt, was das Ereignis bringt -- es traegt die ganze Einheit
/// (RAD_IoNetUnitToClient).
///
void RAD_IoNetOnUnitDeployed(void *user_argument, const RAD_NetUnitDeployed_t *deployed)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    const RAD_NetUnit_t *unit = &deployed->unit;

    printf("<- Einheit %d \"%s\" (%u Mitglieder) von 0x%llx aufgestellt auf (%d, %d)\n",
           (int)unit->unit,
           unit->name,
           (unsigned)unit->number_of_members,
           (unsigned long long)unit->owner,
           (int)deployed->position.x,
           (int)deployed->position.y);

    RAD_ClientUnit_t client_unit;
    RAD_IoNetUnitToClient(unit, &client_unit);

    // In der Reserve nur ersetzen (RAD_ClientWorldAddUnit), sonst erst in die
    // Reserve ihres Besitzers -- das ersetzt sie dabei ebenso.
    if(RAD_IoNetIsInReserve(context->world, unit->owner, unit->unit))
    {
        RAD_ClientWorldAddUnit(context->world, &client_unit);
    }
    else if(!RAD_ClientWorldAddReserveUnit(context->world, &client_unit))
    {
        printf("   Einheit %d passt in keine Reserve -- nicht aufgestellt\n", (int)unit->unit);
        return;
    }

    const RAD_ClientPosition_t position = { .x = deployed->position.x, .y = deployed->position.y };
    if(!RAD_ClientWorldDeployUnit(context->world, unit->unit, position))
    {
        printf("   Einheit %d nicht aufgestellt: Feld unbekannt oder belegt, oder kein Platz mehr\n",
               (int)unit->unit);
    }
}

static bool RAD_IoNetIsInReserve(const RAD_ClientWorld_t *world, RAD_NetUserId_t owner, RAD_NetEntityId_t id)
{
    const RAD_ClientReserve_t *reserve = RAD_ClientWorldReserve(world, owner);
    return (reserve != NULL) && RAD_ClientReserveContains(reserve, id);
}
