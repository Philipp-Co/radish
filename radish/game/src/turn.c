#include <radish/game/turn.h>
#include <stddef.h>

///
/// Alle Zugriffe auf die Reihe laufen ueber RAD_TurnIndexOfUser und
/// RAD_TurnCheckSpend. Dadurch steht die eine Zusage, auf der alles beruht --
/// jeder Benutzer hoechstens einmal in der Reihe --, nur an einer Stelle, und die
/// Frage "kann er das bezahlen" nur in einer Fassung, statt in der Pruefung und
/// in der Buchung getrennt zu altern.
///

static int32_t RAD_TurnIndexOfUser(const RAD_Turn_t *turn, RAD_UserId_t user);
static RAD_TurnResult_t RAD_TurnCheckSpend(const RAD_Turn_t *turn, RAD_UserId_t user, int32_t points);


const char* RAD_TurnResultText(RAD_TurnResult_t result)
{
    switch(result)
    {
        case RAD_TURN_OK:                              return "in Ordnung";
        case RAD_TURN_ERROR_NO_USER:                   return "kein gueltiger Benutzer";
        case RAD_TURN_ERROR_NOT_IN_ORDER:              return "steht nicht in der Reihe";
        case RAD_TURN_ERROR_NOT_YOUR_TURN:             return "ein anderer ist dran";
        case RAD_TURN_ERROR_FULL:                      return "kein Platz mehr in der Reihe";
        case RAD_TURN_ERROR_DUPLICATE:                 return "steht zweimal in der Reihe";
        case RAD_TURN_ERROR_NOT_ENOUGH_ACTION_POINTS:  return "nicht genug Aktionspunkte";
        case RAD_TURN_ERROR_INVALID_COST:              return "kein gueltiger Preis";
        default:                                       return "unbekanntes Ergebnis";
    }
}

RAD_Turn_t RAD_CreateTurn(void)
{
    RAD_Turn_t turn;

    // Auch hinter der Reihe wird geschrieben: gelesen wird zwar nur bis
    // number_of_users, aber ein genullter Platz faellt beim Nachsehen auf, statt
    // als Benutzer durchzugehen.
    for(int32_t i=0;i < RAD_MAX_PLAYERS; ++i)
    {
        turn.order[i] = RAD_USER_NONE;
    }

    turn.number_of_users = 0;
    turn.number = 0;
    turn.action_points = 0;

    return turn;
}

RAD_TurnResult_t RAD_TurnAddUser(RAD_Turn_t *turn, RAD_UserId_t user)
{
    if(user == RAD_USER_NONE)
    {
        return RAD_TURN_ERROR_NO_USER;
    }

    if(RAD_TurnIndexOfUser(turn, user) >= 0)
    {
        return RAD_TURN_OK;
    }

    if(turn->number_of_users >= RAD_MAX_PLAYERS)
    {
        return RAD_TURN_ERROR_FULL;
    }

    const bool was_empty = (turn->number_of_users == 0);

    turn->order[turn->number_of_users] = user;
    turn->number_of_users++;

    // Der erste eroeffnet den Zug. Bei jedem weiteren bleibt alles, wie es war --
    // er haengt sich hinten an, und wer dran ist, merkt davon nichts.
    if(was_empty)
    {
        turn->number = 0;
        turn->action_points = RAD_ACTION_POINTS_PER_TURN;
    }

    return RAD_TURN_OK;
}

void RAD_TurnRemoveUser(RAD_Turn_t *turn, RAD_UserId_t user)
{
    const int32_t index = RAD_TurnIndexOfUser(turn, user);
    if(index < 0)
    {
        return;
    }

    const bool was_current = (index == turn->number);

    // Die Reihe bleibt dicht: alles hinter ihm rueckt eine Stelle vor. Anders als
    // im Entitaetenpool darf hier eine Luecke nicht stehen bleiben -- der Index
    // laeuft ueber die Reihe.
    for(int32_t i=index;i < turn->number_of_users - 1; ++i)
    {
        turn->order[i] = turn->order[i + 1];
    }

    turn->number_of_users--;
    turn->order[turn->number_of_users] = RAD_USER_NONE;

    if(turn->number_of_users == 0)
    {
        turn->number = 0;
        turn->action_points = 0;
        return;
    }

    if(index < turn->number)
    {
        // Er stand vor dem, der dran ist: der ist mit vorgerueckt, und die Stelle
        // muss mit. Derselbe bleibt dran, sein Vorrat auch.
        turn->number--;
    }
    else if(was_current)
    {
        // Sein Nachfolger ist an seine Stelle gerueckt und damit schon dran --
        // ausser er war der letzte, dann faengt die Reihe wieder vorne an. Neuer
        // Zug, voller Vorrat.
        if(turn->number >= turn->number_of_users)
        {
            turn->number = 0;
        }
        turn->action_points = RAD_ACTION_POINTS_PER_TURN;
    }
}

RAD_TurnResult_t RAD_TurnSetOrder(RAD_Turn_t *turn, const RAD_UserId_t *users, int32_t number_of_users)
{
    if(number_of_users < 0 || number_of_users > RAD_MAX_PLAYERS)
    {
        return RAD_TURN_ERROR_FULL;
    }

    if(number_of_users > 0 && users == NULL)
    {
        return RAD_TURN_ERROR_NO_USER;
    }

    // Erst die ganze Reihe pruefen, dann schreiben -- so laesst eine abgelehnte
    // Reihenfolge den Zug unveraendert, statt ihn halb umgestellt zu hinterlassen.
    for(int32_t i=0;i < number_of_users; ++i)
    {
        if(users[i] == RAD_USER_NONE)
        {
            return RAD_TURN_ERROR_NO_USER;
        }

        for(int32_t j=0;j < i; ++j)
        {
            if(users[j] == users[i])
            {
                return RAD_TURN_ERROR_DUPLICATE;
            }
        }
    }

    for(int32_t i=0;i < number_of_users; ++i)
    {
        turn->order[i] = users[i];
    }
    for(int32_t i=number_of_users;i < RAD_MAX_PLAYERS; ++i)
    {
        turn->order[i] = RAD_USER_NONE;
    }

    turn->number_of_users = number_of_users;

    // Die Runde faengt von vorne an: der erste der neuen Reihe ist dran. Alles
    // andere waere zu erraten -- wer vorher zog, steht vielleicht gar nicht mehr
    // in der Reihe, und an welcher Stelle sie fortsetzen sollte, sagt niemand.
    turn->number = 0;
    turn->action_points = (number_of_users > 0) ? RAD_ACTION_POINTS_PER_TURN : 0;

    return RAD_TURN_OK;
}

int32_t RAD_TurnNumberOfUsers(const RAD_Turn_t *turn)
{
    return turn->number_of_users;
}

RAD_UserId_t RAD_TurnUserAt(const RAD_Turn_t *turn, int32_t index)
{
    if(index < 0 || index >= turn->number_of_users)
    {
        return RAD_USER_NONE;
    }

    return turn->order[index];
}

bool RAD_TurnHasUser(const RAD_Turn_t *turn, RAD_UserId_t user)
{
    return RAD_TurnIndexOfUser(turn, user) >= 0;
}

RAD_UserId_t RAD_TurnCurrentUser(const RAD_Turn_t *turn)
{
    return RAD_TurnUserAt(turn, turn->number);
}

bool RAD_TurnIsUsersTurn(const RAD_Turn_t *turn, RAD_UserId_t user)
{
    // Ohne diese Zeile waere niemand dran, sobald die Reihe leer ist.
    if(user == RAD_USER_NONE)
    {
        return false;
    }

    return RAD_TurnCurrentUser(turn) == user;
}

int32_t RAD_TurnActionPoints(const RAD_Turn_t *turn)
{
    return turn->action_points;
}

bool RAD_TurnCanSpendActionPoints(const RAD_Turn_t *turn, RAD_UserId_t user, int32_t points)
{
    return RAD_TurnCheckSpend(turn, user, points) == RAD_TURN_OK;
}

RAD_TurnResult_t RAD_TurnSpendActionPoints(RAD_Turn_t *turn, RAD_UserId_t user, int32_t points)
{
    const RAD_TurnResult_t result = RAD_TurnCheckSpend(turn, user, points);
    if(result != RAD_TURN_OK)
    {
        return result;
    }

    turn->action_points -= points;

    return RAD_TURN_OK;
}

RAD_TurnResult_t RAD_TurnEnd(RAD_Turn_t *turn, RAD_UserId_t user)
{
    if(user == RAD_USER_NONE)
    {
        return RAD_TURN_ERROR_NO_USER;
    }

    if(RAD_TurnIndexOfUser(turn, user) < 0)
    {
        return RAD_TURN_ERROR_NOT_IN_ORDER;
    }

    if(!RAD_TurnIsUsersTurn(turn, user))
    {
        return RAD_TURN_ERROR_NOT_YOUR_TURN;
    }

    // Zyklisch: hinter dem letzten kommt wieder der erste. Steht nur einer in der
    // Reihe, kommt dieselbe Stelle wieder heraus -- er ist noch einmal dran, und
    // sein Vorrat faengt trotzdem von vorne an.
    turn->number = (turn->number + 1) % turn->number_of_users;
    turn->action_points = RAD_ACTION_POINTS_PER_TURN;

    return RAD_TURN_OK;
}

///
/// Stelle des Benutzers in der Reihe, -1 wenn er nicht darin steht.
///
/// Ohne die erste Zeile faende RAD_USER_NONE die Plaetze hinter der Reihe -- die
/// tragen genau diesen Wert.
///
static int32_t RAD_TurnIndexOfUser(const RAD_Turn_t *turn, RAD_UserId_t user)
{
    if(user == RAD_USER_NONE)
    {
        return -1;
    }

    for(int32_t i=0;i < turn->number_of_users; ++i)
    {
        if(turn->order[i] == user)
        {
            return i;
        }
    }

    return -1;
}

///
/// Alles, was gegen ein Abbuchen spricht -- einmal geschrieben fuer die Frage
/// (RAD_TurnCanSpendActionPoints) und die Aenderung (RAD_TurnSpendActionPoints).
/// Getrennt gefuehrt waere genau das der Ort, an dem beide auseinanderlaufen.
///
static RAD_TurnResult_t RAD_TurnCheckSpend(const RAD_Turn_t *turn, RAD_UserId_t user, int32_t points)
{
    if(user == RAD_USER_NONE)
    {
        return RAD_TURN_ERROR_NO_USER;
    }

    if(RAD_TurnIndexOfUser(turn, user) < 0)
    {
        return RAD_TURN_ERROR_NOT_IN_ORDER;
    }

    if(!RAD_TurnIsUsersTurn(turn, user))
    {
        return RAD_TURN_ERROR_NOT_YOUR_TURN;
    }

    if(points < 0)
    {
        return RAD_TURN_ERROR_INVALID_COST;
    }

    if(points > turn->action_points)
    {
        return RAD_TURN_ERROR_NOT_ENOUGH_ACTION_POINTS;
    }

    return RAD_TURN_OK;
}
