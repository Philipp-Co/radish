#include <radish/game/control/unit_turn.h>
#include <radish/game/model/game.h>
#include <stddef.h>

///
/// Was Ziehen und Angreifen gemeinsam verlangen: die Einheit gibt es, sie steht
/// auf dem Feld und ist nicht in diesem Zug aufgestellt worden.
///
RAD_GameResult_t RAD_GameCheckUnitMayAct(const RAD_Unit_t *unit)
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
/// Ein neuer Zug beginnt: alle Einheiten vergessen, was sie im letzten getan
/// haben (RAD_Unit_t.turn). Alle und nicht nur die dessen, der jetzt dran ist --
/// handeln duerfen ohnehin nur seine, und so muss niemand wissen, wem welche
/// gehoert. Vor der Meldung zu rufen, damit ein Abonnent schon den neuen Stand
/// liest.
///
void RAD_GameStartTurn(RAD_Game_t *game)
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
