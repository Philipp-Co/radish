#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <stddef.h>

///
/// Die Mitspieler-Seite des Spiels: wer mitspielt, wer dran ist, wem was gehoert.
///
/// Sie fuehrt nichts selbst. Wer mitspielt und dran ist, steht im Zug (turn.h),
/// wem eine Figur gehoert, in ihr (RAD_Entity_t.owner) -- diese Datei legt die
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
        case RAD_GAME_ERROR_NO_ENTITY:      return "keine gueltige Figur";
        case RAD_GAME_ERROR_FULL:           return "kein Platz mehr frei";
        case RAD_GAME_ERROR_NOT_PLAYING:    return "spielt nicht mit";
        case RAD_GAME_ERROR_NOT_OWNED:      return "Figur gehoert einem anderen";
        case RAD_GAME_ERROR_NOT_YOUR_TURN:  return "ein anderer ist dran";
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

RAD_UserId_t RAD_GameEntityOwner(const RAD_Game_t *game, RAD_EntityId_t entity)
{
    return RAD_WorldEntityOwner(&game->world, entity);
}

RAD_GameResult_t RAD_GameBindEntity(RAD_Game_t *game, RAD_UserId_t user, RAD_EntityId_t entity)
{
    if(user == RAD_USER_NONE)
    {
        return RAD_GAME_ERROR_NO_USER;
    }

    if(!RAD_GameIsPlaying(game, user))
    {
        return RAD_GAME_ERROR_NOT_PLAYING;
    }

    const RAD_UserId_t owner = RAD_WorldEntityOwner(&game->world, entity);
    if((owner != RAD_USER_NONE) && (owner != user))
    {
        return RAD_GAME_ERROR_NOT_OWNED;
    }

    // Zugleich die Probe, ob es die Figur ueberhaupt gibt: die Welt lehnt eine
    // Id ab, hinter der kein belegter Platz steht.
    if(!RAD_WorldSetEntityOwner(&game->world, entity, user))
    {
        return RAD_GAME_ERROR_NO_ENTITY;
    }

    return RAD_GAME_OK;
}

void RAD_GameUnbindEntity(RAD_Game_t *game, RAD_EntityId_t entity)
{
    RAD_WorldSetEntityOwner(&game->world, entity, RAD_USER_NONE);
}

bool RAD_GameMayControlEntity(const RAD_Game_t *game, RAD_UserId_t user, RAD_EntityId_t entity)
{
    if(user == RAD_USER_NONE)
    {
        return false;
    }

    const RAD_UserId_t owner = RAD_WorldEntityOwner(&game->world, entity);
    return (owner == RAD_USER_NONE) || (owner == user);
}

int32_t RAD_GameNumberOfUserEntities(const RAD_Game_t *game, RAD_UserId_t user)
{
    if(user == RAD_USER_NONE)
    {
        return 0;
    }

    int32_t count = 0;
    for(RAD_EntityId_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        const RAD_Entity_t *entity = &game->world.entities[i];
        if((entity->id != RAD_ENTITY_NONE) && (entity->owner == user))
        {
            count++;
        }
    }

    return count;
}

RAD_EntityId_t RAD_GameUserEntityAt(const RAD_Game_t *game, RAD_UserId_t user, int32_t index)
{
    if((user == RAD_USER_NONE) || (index < 0))
    {
        return RAD_ENTITY_NONE;
    }

    int32_t seen = 0;
    for(RAD_EntityId_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        const RAD_Entity_t *entity = &game->world.entities[i];
        if((entity->id == RAD_ENTITY_NONE) || (entity->owner != user))
        {
            continue;
        }

        if(seen == index)
        {
            return entity->id;
        }
        seen++;
    }

    return RAD_ENTITY_NONE;
}
