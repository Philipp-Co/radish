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
        default:                            return "unbekanntes Ergebnis";
    }
}

RAD_GameResult_t RAD_GameAddPlayer(RAD_Game_t *game, RAD_UserId_t user)
{
    // Mitspielen heisst in der Reihe stehen, mehr ist nicht zu tun. Was das genau
    // bedeutet -- hinten anhaengen, und wenn die Reihe leer war, faengt sie mit
    // ihm an --, entscheidet der Zug (turn.h); hier wird nur uebersetzt.
    switch(RAD_TurnAddUser(&game->turn, user))
    {
        case RAD_TURN_OK:
            return RAD_GAME_OK;

        case RAD_TURN_ERROR_NO_USER:
            return RAD_GAME_ERROR_NO_USER;

        case RAD_TURN_ERROR_FULL:
        default:
            return RAD_GAME_ERROR_FULL;
    }
}

void RAD_GameRemovePlayer(RAD_Game_t *game, RAD_UserId_t user)
{
    // Seine Figuren bleiben stehen und behalten ihn als Besitzer -- die
    // Begruendung steht in game.h. Ob der Zug weitergegeben werden muss, weil er
    // gerade dran war, entscheidet der Zug selbst.
    RAD_TurnRemoveUser(&game->turn, user);
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

    return RAD_GAME_OK;
}

void RAD_GameUnbindUnit(RAD_Game_t *game, RAD_UnitId_t unit)
{
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
    for(RAD_UnitId_t i=0;i < RAD_MAX_UNITS; ++i)
    {
        const RAD_Unit_t *unit = &game->world.units[i];
        if((unit->id != RAD_UNIT_NONE) && (unit->owner == user))
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
    for(RAD_UnitId_t i=0;i < RAD_MAX_UNITS; ++i)
    {
        const RAD_Unit_t *unit = &game->world.units[i];
        if((unit->id == RAD_UNIT_NONE) || (unit->owner != user))
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
        return (game->world.number_of_units >= RAD_MAX_UNITS) ? RAD_GAME_ERROR_FULL : RAD_GAME_ERROR_INVALID_UNIT;
    }

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

    if((unit < 0) || (unit >= RAD_MAX_UNITS) || (game->world.units[unit].id == RAD_UNIT_NONE))
    {
        return RAD_GAME_ERROR_NO_UNIT;
    }
    const RAD_Unit_t *candidate = &game->world.units[unit];
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
