#include <radish/io/net_event_handler.h>

#include <stdio.h>

///
/// Eine Einheit hat ein Feld angegriffen -- gleich, wessen: ins Log, mit dem,
/// was der Angriff bewirkt hat, und mit allem an die Beobachter der Welt
/// (RAD_ClientWorldReportAttack, als RAD_ClientAttackResult_t), damit sich der
/// Angriff zeigen laesst -- auch auf ein leeres Feld. In die Welt traegt das Ereignis selbst nichts ein: was von der getroffenen Einheit uebrig ist, bringt danach ihr neuer
/// Stand (unit.c), eine zerstoerte hat der Server vorher gemeldet
/// (unit_destroyed.c), und dass die angreifende in diesem Zug angegriffen hat,
/// bringt ebenso ihr neuer Stand. Die Antwort auf das Kommando sagt nur, ob es
/// angenommen wurde (attack_response.c).
///
void RAD_IoNetOnUnitAttacked(void *user_argument, const RAD_NetUnitAttacked_t *attacked)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    // Die Ids gehen unveraendert hinueber: RAD_NET_ENTITY_NONE und
    // RAD_CLIENT_UNIT_ID_NONE sind beide -1. Die Eintraege je Waffe hat der Codec
    // schon auf das begrenzt, was eine Einheit haben kann.
    RAD_ClientAttackResult_t result = {
        .attacker = (RAD_ClientUnitId_t)attacked->entity,
        .target = { .x = attacked->target.x, .y = attacked->target.y },
        .victim = (RAD_ClientUnitId_t)attacked->victim,
        .shots = attacked->shots,
        .hits = attacked->hits,
        .penetrations = attacked->penetrations,
        .wounds = attacked->wounds,
        .members_killed = attacked->members_killed,
        .victim_destroyed = attacked->victim_destroyed
    };
    for(uint32_t i = 0; (i < attacked->number_of_weapons) && (i < RAD_CLIENT_ATTACK_WEAPONS_MAX); ++i)
    {
        const RAD_NetAttackWeapon_t *weapon = &attacked->weapons[i];
        result.weapons[result.number_of_weapons++] = (RAD_ClientAttackWeapon_t){
            .member = weapon->member,
            .weapon = weapon->weapon,
            .shots = weapon->shots,
            .hits = weapon->hits,
            .penetrations = weapon->penetrations,
            .wounds = weapon->wounds,
            .members_killed = weapon->members_killed
        };
    }
    RAD_ClientWorldReportAttack(context->world, &result);

    if(attacked->victim == RAD_NET_ENTITY_NONE)
    {
        printf("<- Einheit %d greift (%d, %d) an -- dort steht niemand\n",
               (int)attacked->entity, (int)attacked->target.x, (int)attacked->target.y);
        return;
    }

    printf("<- Einheit %d greift Einheit %d auf (%d, %d) an: %u Schuesse, %u Treffer, %u durch, "
           "%u Lebenspunkte, %u Mitglieder gefallen%s\n",
           (int)attacked->entity, (int)attacked->victim,
           (int)attacked->target.x, (int)attacked->target.y,
           (unsigned)attacked->shots, (unsigned)attacked->hits, (unsigned)attacked->penetrations,
           (unsigned)attacked->wounds, (unsigned)attacked->members_killed,
           attacked->victim_destroyed ? ", Einheit zerstoert" : "");
    for(uint32_t i = 0; i < attacked->number_of_weapons; ++i)
    {
        const RAD_NetAttackWeapon_t *weapon = &attacked->weapons[i];
        printf("     Mitglied %u, Waffe %u: %u Schuesse, %u Treffer, %u durch, %u Lebenspunkte, %u gefallen\n",
               (unsigned)weapon->member, (unsigned)weapon->weapon,
               (unsigned)weapon->shots, (unsigned)weapon->hits, (unsigned)weapon->penetrations,
               (unsigned)weapon->wounds, (unsigned)weapon->members_killed);
    }
}
