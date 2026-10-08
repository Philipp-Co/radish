#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <stddef.h>

///
/// Die Mitspieler-Seite des Spiels: wer mitspielt, wer dran ist, wem was gehoert.
///
/// Sie fuehrt nichts selbst. Wer mitspielt und dran ist, steht im Zug (turn.h),
/// wem eine Figur gehoert, in ihr (RAD_Unit_t.owner) -- diese Datei legt die
/// beiden zusammen und fuegt die Fragen hinzu, die keines von beiden allein
/// beantworten kann, weil sie einander nicht kennen: ob der Benutzer ueberhaupt
/// mitspielt und ob die Figur nicht schon einem anderen gehoert.
///
/// Nach aussen ist sie damit die Fassade: ein Aufrufer nimmt einen Mitspieler
/// ueber RAD_GameAddPlayer auf und nicht ueber RAD_TurnAddUser, auch wenn dahinter
/// nur ein Aufruf steht. Was "mitspielen" heisst, soll sich aendern koennen, ohne
/// dass jede Aufrufstelle davon erfaehrt -- und die Ergebnisse des Zuges gehoeren
/// uebersetzt, bevor sie den Server erreichen.
///
/// **Sie meldet den Zugwechsel** (RAD_EventManagerPublishTurnChanged): der Zug
/// selbst kennt keine Abonnenten, und alles, was aendert, wer dran ist, laeuft
/// hier durch. Aus demselben Grund vergisst sie hier, was die Einheiten im Zug
/// getan haben (RAD_GameStartTurn).
///

static void RAD_GameNotifyIfTurnChanged(RAD_Game_t *game, RAD_UserId_t current_before);
static void RAD_GameStartTurn(RAD_Game_t *game);
static RAD_GameResult_t RAD_GameCheckUnitMayAct(const RAD_Unit_t *unit);
static bool RAD_GameUnitReaches(const RAD_Unit_t *unit, int32_t x, int32_t y);

static void RAD_GameAdoptUnit(RAD_Game_t *game, RAD_Unit_t *unit);
static void RAD_GameReleaseUnit(RAD_Game_t *game, const RAD_Unit_t *unit);

const char* RAD_GameResultText(RAD_GameResult_t result)
{
    switch(result)
    {
        case RAD_GAME_OK:                   return "in Ordnung";
        case RAD_GAME_ERROR_NO_USER:        return "kein gueltiger Benutzer";
        case RAD_GAME_ERROR_NO_UNIT:        return "keine gueltige Figur";
        case RAD_GAME_ERROR_FULL:           return "kein Platz mehr frei";
        case RAD_GAME_ERROR_NOT_PLAYING:    return "spielt nicht mit";
        case RAD_GAME_ERROR_NOT_OWNED:      return "Figur gehoert einem anderen";
        case RAD_GAME_ERROR_NOT_YOUR_TURN:  return "ein anderer ist dran";
        case RAD_GAME_ERROR_INVALID_UNIT:   return "Einheit passt nicht in ihre Felder";
        case RAD_GAME_ERROR_STARTED:        return "das Spiel laeuft schon";
        case RAD_GAME_ERROR_NOT_IN_RESERVE: return "Einheit steht nicht in der Reserve";
        case RAD_GAME_ERROR_OUT_OF_BOUNDS:  return "Feld liegt ausserhalb der Welt";
        case RAD_GAME_ERROR_NO_GROUND:      return "Feld hat kein Gelaende";
        case RAD_GAME_ERROR_OCCUPIED:       return "Feld ist besetzt";
        case RAD_GAME_ERROR_UNIT_NOT_DEPLOYED:
                                            return "Einheit steht nicht auf dem Feld";
        case RAD_GAME_ERROR_UNIT_JUST_DEPLOYED:
                                            return "Einheit ist in diesem Zug erst aufgestellt worden";
        case RAD_GAME_ERROR_UNIT_ALREADY_MOVED:
                                            return "Einheit ist in diesem Zug schon gezogen";
        case RAD_GAME_ERROR_UNIT_ALREADY_ATTACKED:
                                            return "Einheit hat in diesem Zug schon angegriffen";
        case RAD_GAME_ERROR_TARGET_OUT_OF_RANGE:
                                            return "Ziel liegt ausserhalb der Reichweite";
        default:                            return "unbekanntes Ergebnis";
    }
}

RAD_GameResult_t RAD_GameAddPlayer(RAD_Game_t *game, RAD_UserId_t user)
{
    // Zweimal derselbe aendert nichts -- auch keinen zweiten Spieler.
    if(RAD_GameFindPlayer(game, user) != NULL)
    {
        return RAD_GAME_OK;
    }

    const RAD_UserId_t current_before = RAD_TurnCurrentUser(&game->turn);

    // Mitspielen heisst in der Reihe stehen und einen Spieler haben. Was die Reihe
    // angeht -- hinten anhaengen, und wenn sie leer war, faengt sie mit ihm an --,
    // entscheidet der Zug (turn.h); hier wird nur uebersetzt.
    switch(RAD_TurnAddUser(&game->turn, user))
    {
        case RAD_TURN_OK:
            break;

        case RAD_TURN_ERROR_NO_USER:
            return RAD_GAME_ERROR_NO_USER;

        case RAD_TURN_ERROR_FULL:
        default:
            return RAD_GAME_ERROR_FULL;
    }

    // Die Reihe hat RAD_MAX_PLAYERS Plaetze wie "players"; hat sie ihn genommen,
    // ist auch hier einer frei. Fehlt der Speicher fuer den Spieler, geht er
    // wieder aus der Reihe: beide sollen dieselben Benutzer kennen. "Kein Platz
    // mehr frei" ist dann wortwoertlich, nur eben im Speicher.
    // Zurueckgenommen ist nichts geschehen: gemeldet wird erst unten.
    RAD_Player_t *player = RAD_CreatePlayer(user);
    if(player == NULL)
    {
        RAD_TurnRemoveUser(&game->turn, user);
        return RAD_GAME_ERROR_FULL;
    }

    game->players[game->number_of_players] = player;
    game->number_of_players++;

    // Wer wiederkommt, fuehrt seine Figuren weiter (game.h): sie stehen noch im
    // Pool und tragen ihn als Besitzer. Sein neuer Spieler nimmt sie auf.
    const int32_t number_of_units = RAD_UnitPoolNumberOfUnits(game->unit_pool);
    for(int32_t i = 0; i < number_of_units; ++i)
    {
        RAD_Unit_t *unit = RAD_UnitPoolUnitAt(game->unit_pool, i);
        if(unit->owner == user)
        {
            RAD_GameAdoptUnit(game, unit);
        }
    }

    // Der erste in einer leeren Reihe ist gleich dran; jeder weitere aendert nichts.
    RAD_GameNotifyIfTurnChanged(game, current_before);
    return RAD_GAME_OK;
}

void RAD_GameRemovePlayer(RAD_Game_t *game, RAD_UserId_t user)
{
    // Seine Figuren bleiben stehen und behalten ihn als Besitzer -- die
    // Begruendung steht in game.h. Ob der Zug weitergegeben werden muss, weil er
    // gerade dran war, entscheidet der Zug selbst.
    const RAD_UserId_t current_before = RAD_TurnCurrentUser(&game->turn);
    RAD_TurnRemoveUser(&game->turn, user);
    RAD_GameNotifyIfTurnChanged(game, current_before);

    // Der Spieler geht mit, samt Einheitenliste und Reserve. Die Einheiten selbst
    // bleiben, wo sie sind -- beide halten nur Zeiger.
    for(int32_t i = 0; i < game->number_of_players; ++i)
    {
        if(RAD_PlayerId(game->players[i]) != user)
        {
            continue;
        }

        RAD_DestroyPlayer(&game->players[i]);
        for(int32_t k = i + 1; k < game->number_of_players; ++k)
        {
            game->players[k - 1] = game->players[k];
        }
        game->number_of_players--;
        return;
    }
}

RAD_Player_t* RAD_GameFindPlayer(const RAD_Game_t *game, RAD_UserId_t user)
{
    if(game == NULL || user == RAD_USER_NONE)
    {
        return NULL;
    }

    for(int32_t i = 0; i < game->number_of_players; ++i)
    {
        if(RAD_PlayerId(game->players[i]) == user)
        {
            return game->players[i];
        }
    }
    return NULL;
}

bool RAD_GameIsPlaying(const RAD_Game_t *game, RAD_UserId_t user)
{
    return RAD_TurnHasUser(&game->turn, user);
}

int32_t RAD_GameNumberOfPlayers(const RAD_Game_t *game)
{
    return RAD_TurnNumberOfUsers(&game->turn);
}

RAD_UserId_t RAD_GamePlayerAt(const RAD_Game_t *game, int32_t index)
{
    return RAD_TurnUserAt(&game->turn, index);
}

RAD_UserId_t RAD_GameCurrentUser(const RAD_Game_t *game)
{
    return RAD_TurnCurrentUser(&game->turn);
}

bool RAD_GameIsUsersTurn(const RAD_Game_t *game, RAD_UserId_t user)
{
    return RAD_TurnIsUsersTurn(&game->turn, user);
}

RAD_GameResult_t RAD_GameEndTurn(RAD_Game_t *game, RAD_UserId_t user)
{
    // Die Regel steht im Zug; hier wird nur uebersetzt. "Steht nicht in der
    // Reihe" heisst nach aussen "spielt nicht mit": wer mitspielt, steht in der
    // Reihe -- dafuer sorgen RAD_GameAddPlayer und RAD_GameRemovePlayer.
    switch(RAD_TurnEnd(&game->turn, user))
    {
        case RAD_TURN_OK:
            game->started = true;
            // Immer, auch wenn derselbe wieder dran ist: es ist ein neuer Zug.
            RAD_GameStartTurn(game);
            RAD_EventManagerPublishTurnChanged(game->event_manager, RAD_TurnCurrentUser(&game->turn));
            return RAD_GAME_OK;

        case RAD_TURN_ERROR_NO_USER:
            return RAD_GAME_ERROR_NO_USER;

        case RAD_TURN_ERROR_NOT_YOUR_TURN:
            return RAD_GAME_ERROR_NOT_YOUR_TURN;

        case RAD_TURN_ERROR_NOT_IN_ORDER:
        default:
            return RAD_GAME_ERROR_NOT_PLAYING;
    }
}

bool RAD_GameHasStarted(const RAD_Game_t *game)
{
    return (game != NULL) && game->started;
}

RAD_UserId_t RAD_GameUnitOwner(const RAD_Game_t *game, RAD_UnitId_t unit)
{
    return RAD_WorldUnitOwner(&game->world, unit);
}

RAD_GameResult_t RAD_GameBindUnit(RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit)
{
    if(user == RAD_USER_NONE)
    {
        return RAD_GAME_ERROR_NO_USER;
    }

    if(!RAD_GameIsPlaying(game, user))
    {
        return RAD_GAME_ERROR_NOT_PLAYING;
    }

    const RAD_UserId_t owner = RAD_WorldUnitOwner(&game->world, unit);
    if((owner != RAD_USER_NONE) && (owner != user))
    {
        return RAD_GAME_ERROR_NOT_OWNED;
    }

    // Zugleich die Probe, ob es die Figur ueberhaupt gibt: die Welt lehnt eine
    // Id ab, hinter der kein belegter Platz steht.
    if(!RAD_WorldSetUnitOwner(&game->world, unit, user))
    {
        return RAD_GAME_ERROR_NO_UNIT;
    }

    // Der Besitzer war niemand oder schon "user" (oben) -- es gibt also keinen
    // anderen Spieler, aus dessen Liste sie zu nehmen waere.
    RAD_GameAdoptUnit(game, RAD_WorldUnitById(&game->world, unit));
    return RAD_GAME_OK;
}

void RAD_GameUnbindUnit(RAD_Game_t *game, RAD_UnitId_t unit)
{
    const RAD_Unit_t *bound = RAD_WorldUnitById(&game->world, unit);
    if(bound != NULL)
    {
        RAD_GameReleaseUnit(game, bound);
    }
    RAD_WorldSetUnitOwner(&game->world, unit, RAD_USER_NONE);
}

bool RAD_GameMayControlUnit(const RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit)
{
    if(user == RAD_USER_NONE)
    {
        return false;
    }

    const RAD_UserId_t owner = RAD_WorldUnitOwner(&game->world, unit);
    return (owner == RAD_USER_NONE) || (owner == user);
}

int32_t RAD_GameNumberOfUserUnits(const RAD_Game_t *game, RAD_UserId_t user)
{
    if(user == RAD_USER_NONE)
    {
        return 0;
    }

    int32_t count = 0;
    const int32_t number_of_units = RAD_UnitPoolNumberOfUnits(game->unit_pool);
    for(int32_t i=0;i < number_of_units; ++i)
    {
        if(RAD_UnitPoolUnitAt(game->unit_pool, i)->owner == user)
        {
            count++;
        }
    }

    return count;
}

RAD_UnitId_t RAD_GameUserUnitAt(const RAD_Game_t *game, RAD_UserId_t user, int32_t index)
{
    if((user == RAD_USER_NONE) || (index < 0))
    {
        return RAD_UNIT_NONE;
    }

    int32_t seen = 0;
    const int32_t number_of_units = RAD_UnitPoolNumberOfUnits(game->unit_pool);
    for(int32_t i=0;i < number_of_units; ++i)
    {
        const RAD_Unit_t *unit = RAD_UnitPoolUnitAt(game->unit_pool, i);
        if(unit->owner != user)
        {
            continue;
        }

        if(seen == index)
        {
            return unit->id;
        }
        seen++;
    }

    return RAD_UNIT_NONE;
}

RAD_GameResult_t RAD_GameAddUnit(RAD_Game_t *game, RAD_UserId_t owner, const RAD_Unit_t *values, RAD_UnitId_t *id)
{
    if(id != NULL)
    {
        *id = RAD_UNIT_NONE;
    }

    // Die Reihenfolge der Pruefungen ist die der Fragen: wer, wann, was. Erst
    // danach entscheidet die Welt, ob noch Platz ist -- sie kennt nur den Pool,
    // nicht die Regeln.
    if(owner == RAD_USER_NONE)
    {
        return RAD_GAME_ERROR_NO_USER;
    }
    if(!RAD_GameIsPlaying(game, owner))
    {
        return RAD_GAME_ERROR_NOT_PLAYING;
    }
    if(game->started)
    {
        return RAD_GAME_ERROR_STARTED;
    }
    if(values == NULL)
    {
        return RAD_GAME_ERROR_INVALID_UNIT;
    }

    // Die Welt lehnt aus zwei Gruenden ab, und nur einer davon ist eine Frage an
    // den Pool. Welcher es war, sagt der Pool selbst: ist kein Slot mehr frei,
    // ist er voll, sonst lag es an den Werten.
    const RAD_UnitId_t added = RAD_WorldAddReserveUnit(&game->world, values, owner);
    if(added == RAD_UNIT_NONE)
    {
        return (RAD_UnitPoolNumberOfUnits(game->unit_pool) >= RAD_MAX_UNITS) ? RAD_GAME_ERROR_FULL : RAD_GAME_ERROR_INVALID_UNIT;
    }

    // In die Einheitenliste und die Reserve des Besitzers -- er spielt mit (oben).
    RAD_GameAdoptUnit(game, RAD_WorldUnitById(&game->world, added));

    if(id != NULL)
    {
        *id = added;
    }
    return RAD_GAME_OK;
}

RAD_GameResult_t RAD_GameCheckDeployUnit(const RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit, int32_t x, int32_t y)
{
    // In der Reihenfolge der Fragen: wer, was, wohin.
    if(user == RAD_USER_NONE)
    {
        return RAD_GAME_ERROR_NO_USER;
    }
    if(!RAD_GameIsPlaying(game, user))
    {
        return RAD_GAME_ERROR_NOT_PLAYING;
    }

    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    if(candidate == NULL)
    {
        return RAD_GAME_ERROR_NO_UNIT;
    }
    if(candidate->owner != user)
    {
        return RAD_GAME_ERROR_NOT_OWNED;
    }
    if(candidate->state != RAD_UNIT_STATE_RESERVE)
    {
        return RAD_GAME_ERROR_NOT_IN_RESERVE;
    }

    if(!RAD_WorldInBounds(&game->world, x, y))
    {
        return RAD_GAME_ERROR_OUT_OF_BOUNDS;
    }
    const RAD_Tile_t *tile = &game->world.tiles[y][x];
    if(tile->type == RAD_TILE_TYPE_VOID)
    {
        return RAD_GAME_ERROR_NO_GROUND;
    }
    if(tile->unit != RAD_UNIT_NONE)
    {
        return RAD_GAME_ERROR_OCCUPIED;
    }

    return RAD_GAME_OK;
}

RAD_GameResult_t RAD_GameCheckMoveUnit(const RAD_Game_t *game, RAD_UnitId_t unit)
{
    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    const RAD_GameResult_t may_act = RAD_GameCheckUnitMayAct(candidate);
    if(may_act != RAD_GAME_OK)
    {
        return may_act;
    }
    if(candidate->turn.moved)
    {
        return RAD_GAME_ERROR_UNIT_ALREADY_MOVED;
    }

    return RAD_GAME_OK;
}

RAD_GameResult_t RAD_GameCheckAttack(const RAD_Game_t *game, RAD_UnitId_t unit, int32_t x, int32_t y)
{
    // In der Reihenfolge der Fragen: wer, dann wohin.
    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    const RAD_GameResult_t may_act = RAD_GameCheckUnitMayAct(candidate);
    if(may_act != RAD_GAME_OK)
    {
        return may_act;
    }
    if(candidate->turn.attacked)
    {
        return RAD_GAME_ERROR_UNIT_ALREADY_ATTACKED;
    }

    if(!RAD_WorldInBounds(&game->world, x, y))
    {
        return RAD_GAME_ERROR_OUT_OF_BOUNDS;
    }
    if(!RAD_GameUnitReaches(candidate, x, y))
    {
        return RAD_GAME_ERROR_TARGET_OUT_OF_RANGE;
    }

    return RAD_GAME_OK;
}

bool RAD_GameUnitCanMove(const RAD_Game_t *game, RAD_UnitId_t unit)
{
    return RAD_GameCheckMoveUnit(game, unit) == RAD_GAME_OK;
}

bool RAD_GameUnitCanAttack(const RAD_Game_t *game, RAD_UnitId_t unit)
{
    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    return (RAD_GameCheckUnitMayAct(candidate) == RAD_GAME_OK) && !candidate->turn.attacked;
}

///
/// Was Ziehen und Angreifen gemeinsam verlangen: die Einheit gibt es, sie steht
/// auf dem Feld und ist nicht in diesem Zug aufgestellt worden.
///
static RAD_GameResult_t RAD_GameCheckUnitMayAct(const RAD_Unit_t *unit)
{
    if(unit == NULL)
    {
        return RAD_GAME_ERROR_NO_UNIT;
    }
    if(unit->state != RAD_UNIT_STATE_DEPLOYED)
    {
        return RAD_GAME_ERROR_UNIT_NOT_DEPLOYED;
    }
    if(unit->turn.deployed)
    {
        return RAD_GAME_ERROR_UNIT_JUST_DEPLOYED;
    }

    return RAD_GAME_OK;
}

///
/// Reicht wenigstens eine Waffe eines Mitglieds von "unit" bis (x, y)? Die
/// Entfernung in Feldern waagerecht plus senkrecht, zwischen min_range und
/// max_range der Waffe (RAD_GameCheckAttack). Das eigene Feld nie.
///
static bool RAD_GameUnitReaches(const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    const int32_t dx = x - unit->x;
    const int32_t dy = y - unit->y;
    const int32_t distance = ((dx < 0) ? -dx : dx) + ((dy < 0) ? -dy : dy);
    if(distance == 0)
    {
        return false;
    }

    for(int32_t m = 0; (m < unit->number_of_members) && (m < RAD_UNIT_MAX_MEMBERS); ++m)
    {
        const RAD_UnitMember_t *member = &unit->members[m];
        for(int32_t w = 0; (w < member->number_of_weapons) && (w < RAD_UNIT_MAX_WEAPONS); ++w)
        {
            const RAD_Weapon_t *weapon = &member->weapons[w];
            if((distance >= weapon->min_range) && (distance <= weapon->max_range))
            {
                return true;
            }
        }
    }

    return false;
}

///
/// Ein neuer Zug beginnt: alle Einheiten vergessen, was sie im letzten getan
/// haben (RAD_Unit_t.turn). Alle und nicht nur die dessen, der jetzt dran ist --
/// handeln duerfen ohnehin nur seine, und so muss niemand wissen, wem welche
/// gehoert. Vor der Meldung zu rufen, damit ein Abonnent schon den neuen Stand
/// liest.
///
static void RAD_GameStartTurn(RAD_Game_t *game)
{
    const int32_t number_of_units = RAD_UnitPoolNumberOfUnits(game->unit_pool);
    for(int32_t i = 0; i < number_of_units; ++i)
    {
        RAD_Unit_t *unit = RAD_UnitPoolUnitAt(game->unit_pool, i);
        unit->turn.deployed = 0;
        unit->turn.moved = 0;
        unit->turn.attacked = 0;
    }
}

///
/// Haelt Einheitenliste und Reserve des Besitzers mit der Einheit zusammen
/// (player/player.h): sie steht in seiner Liste, und in seiner Reserve, solange
/// sie im Zustand RAD_UNIT_STATE_RESERVE ist. Spielt der Besitzer nicht mit oder
/// gibt es keinen, gibt es auch keine Liste -- dann bleibt nur RAD_Unit_t.owner,
/// und ein spaeterer Beitritt holt sie nach (RAD_GameAddPlayer). Zweimal ist
/// harmlos: beide Listen nehmen dieselbe Einheit nur einmal auf.
///
static void RAD_GameAdoptUnit(RAD_Game_t *game, RAD_Unit_t *unit)
{
    RAD_Player_t *player = RAD_GameFindPlayer(game, unit->owner);
    if(player == NULL)
    {
        return;
    }

    RAD_PlayerAddUnit(player, unit);
    if(unit->state == RAD_UNIT_STATE_RESERVE)
    {
        RAD_ReserveAddUnit(RAD_PlayerReserve(player), unit);
    }
}

///
/// Das Gegenstueck: nimmt die Einheit aus Liste und Reserve ihres Besitzers.
///
static void RAD_GameReleaseUnit(RAD_Game_t *game, const RAD_Unit_t *unit)
{
    RAD_PlayerRemoveUnit(RAD_GameFindPlayer(game, unit->owner), unit);
}

///
/// Meldet den Zugwechsel, wenn jetzt ein anderer dran ist als "current_before"
/// -- fuer Beitritt und Austritt, die das nur manchmal aendern.
///
static void RAD_GameNotifyIfTurnChanged(RAD_Game_t *game, RAD_UserId_t current_before)
{
    const RAD_UserId_t current = RAD_TurnCurrentUser(&game->turn);
    if(current != current_before)
    {
        RAD_GameStartTurn(game);
        RAD_EventManagerPublishTurnChanged(game->event_manager, current);
    }
}
