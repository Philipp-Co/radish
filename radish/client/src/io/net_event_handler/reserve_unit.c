#include <radish/io/net_event_handler.h>

#include <stdio.h>

///
/// Eine Einheit aus der Reserve, als Antwort auf eine Reserve-Anfrage: ins Log
/// und in die Reserve ihres Besitzers in der Welt. Ob sie die eigene ist, sagt
/// die eigene Spieler-Id (own_player_id); solange die nicht da ist, bleibt es
/// offen.
///
/// Ihre Mitglieder schickt der Server noch nicht mit, nur deren Anzahl -- die
/// Einheit kommt deshalb ohne Mitglieder in die Welt.
///
void RAD_IoNetOnReserveUnit(void *user_argument, const RAD_NetReserveUnit_t *unit)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;
    const RAD_NetUserId_t own_player_id = context->session->own_player_id;

    const char *whose = (own_player_id == RAD_NET_USER_NONE) ? "Besitzer unbekannt"
                      : (unit->owner == own_player_id)      ? "eigene"
                                                            : "fremde";

    printf("<- Reserve: Einheit %d \"%s\" (%u Mitglieder) von 0x%llx (%s)\n",
           (int)unit->unit,
           unit->name,
           (unsigned)unit->number_of_members,
           (unsigned long long)unit->owner,
           whose);

    RAD_ClientUnit_t client_unit;
    RAD_ClientUnitInit(&client_unit, unit->unit, unit->owner, unit->name);

    if(!RAD_ClientWorldAddReserveUnit(context->world, &client_unit))
    {
        printf("   Einheit %d passt in keine Reserve\n", (int)unit->unit);
    }
}
