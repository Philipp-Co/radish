#include <radish/io/net_event_handler.h>

#include <stdio.h>

///
/// Eine Einheit hat ein Feld angegriffen -- gleich, wessen: ins Log, mit dem,
/// was der Angriff bewirkt hat. In die Welt traegt das Ereignis selbst nichts
/// ein: was von der getroffenen Einheit uebrig ist, bringt danach ihr neuer
/// Stand (unit.c), eine zerstoerte hat der Server vorher gemeldet
/// (unit_destroyed.c), und dass die angreifende in diesem Zug angegriffen hat,
/// bringt ebenso ihr neuer Stand. Die Antwort auf das Kommando sagt nur, ob es
/// angenommen wurde (attack_response.c).
///
void RAD_IoNetOnUnitAttacked(void *user_argument, const RAD_NetUnitAttacked_t *attacked)
{
    (void)user_argument;

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
}
