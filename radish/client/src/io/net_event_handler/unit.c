#include "common.h"

#include <stdio.h>

///
/// Eine Einheit, die einem Spieler gehoert, als Antwort auf eine
/// Einheiten-Anfrage -- und mit ihrem neuen Stand nach einem Zug oder Angriff
/// und nach jedem Zugwechsel: ins Log und ins Repository der Welt
/// (RAD_ClientWorldAddUnit) -- in keine Reserve und auf kein Feld, denn wo sie
/// steht, sagt die Nachricht nicht. Vollstaendig, mit Mitgliedern, Waffen und
/// dem, was sie im Zug schon getan hat (RAD_IoNetUnitToClient). Eine bekannte
/// Einheit wird ersetzt, und ihre Beobachter erfahren es.
///
void RAD_IoNetOnUnit(void *user_argument, const RAD_NetUnit_t *unit)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;
    const RAD_NetUserId_t own_player_id = context->session->own_player_id;

    const char *whose = (own_player_id == RAD_NET_USER_NONE) ? "Besitzer unbekannt"
                      : (unit->owner == own_player_id)      ? "eigene"
                                                            : "fremde";

    printf("<- Einheit %d \"%s\" (%u Mitglieder) von 0x%llx (%s)%s%s%s\n",
           (int)unit->unit,
           unit->name,
           (unsigned)unit->number_of_members,
           (unsigned long long)unit->owner,
           whose,
           unit->deployed ? ", aufgestellt" : "",
           unit->moved ? ", gezogen" : "",
           unit->attacked ? ", hat angegriffen" : "");

    RAD_ClientUnit_t client_unit;
    RAD_IoNetUnitToClient(unit, &client_unit);

    if(!RAD_ClientWorldAddUnit(context->world, &client_unit))
    {
        printf("   Einheit %d nicht aufgenommen -- ohne Besitzer oder kein Platz mehr\n", (int)unit->unit);
    }
}
