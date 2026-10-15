#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/control/commands.h>
#include <radish/game/control/unit_turn.h>
#include <radish/game/control/dice.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

///
/// Der Angriff einer Einheit auf ein Feld (RAD_COMMAND_TYPE_ATTACK): die
/// Fabrik, die Regel samt Reichweite, das Ausfuehren und der Kampf -- alles, was
/// zu diesem Kommando gehoert, an einer Stelle. Was Ziehen und Angreifen
/// gemeinsam verlangen, steht in control/unit_turn.c.
///
/// **Der Kampf, Schuss fuer Schuss.** Jede Waffe jedes Mitglieds, die das Feld
/// erreicht, feuert alle ihre Schuesse; jeder Schuss geht durch vier Stufen, und
/// jede wird mit dem W10 des Spiels (RAD_Game_t.dice) entschieden:
///
///     1. Treffer       W10 >= Genauigkeit des Schuetzen + Deckung des Zielfeldes
///                      -> ein zufaelliges Mitglied der Zieleinheit ist getroffen
///     2. Durchschlag   W10 + Durchschlag der Waffe >= Ruestung des Getroffenen
///     3. Wirkung       W10 + Staerke der Waffe     >= Widerstand des Getroffenen
///     4. Schaden       ein Lebenspunkt weniger
///
/// Faellt ein Mitglied auf 0, ist es nicht mehr da; hat die Einheit keins mehr,
/// ist sie zerstoert (RAD_GameDestroyUnit), und der Angriff ist zu Ende -- die
/// uebrigen Schuesse werden nicht gewuerfelt.
///
/// **Die Deckung** ist die des Feldes, auf dem das Ziel steht
/// (RAD_Terrain_t.cover). Sie erschwert nur den Treffer, nicht die Stufen danach,
/// und eine Summe ueber 10 macht das Ziel mit dem W10 unerreichbar -- gewollt.
///
/// **Die Reihenfolge ist fest**: Mitglieder, darin Waffen, darin Schuesse, je in
/// ihrer Reihenfolge. Zum selben Seed kommt also derselbe Kampf heraus.
///
/// Angegriffen werden darf jedes Feld in Reichweite, auch ein leeres -- dann
/// wird nicht geschossen -- und auch eine eigene Einheit, die dann wie jede
/// andere getroffen wird. Spaetere Regeln koennen das einschraenken.
///

static bool RAD_GameUnitReaches(const RAD_Unit_t *unit, int32_t x, int32_t y);
static bool RAD_GameWeaponReaches(const RAD_Weapon_t *weapon, int32_t distance);
static int32_t RAD_GameDistance(const RAD_Unit_t *unit, int32_t x, int32_t y);
static void RAD_GameFireShot(RAD_Game_t *game, const RAD_UnitMember_t *shooter, const RAD_Weapon_t *weapon,
                             int32_t cover, RAD_Unit_t *target, RAD_AttackResult_t *result);
static void RAD_GameRemoveMember(RAD_Unit_t *unit, int32_t member);

bool RAD_GameAttack(RAD_Game_t *game, RAD_UnitId_t id, int16_t x, int16_t y, RAD_Command_t *output)
{
    // Wie beim Aufstellen: erst pruefen, dann schreiben.
    if((game == NULL) || (output == NULL) || (id < 0))
    {
        return false;
    }

    output->header.type = RAD_COMMAND_TYPE_ATTACK;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.attack.unit = id;
    output->command.attack.x = x;
    output->command.attack.y = y;
    return true;
}

RAD_GameResult_t RAD_GameCheckAttack(const RAD_Game_t *game, RAD_UnitId_t unit, int32_t x, int32_t y)
{
    // In der Reihenfolge der Fragen: ob ueberhaupt, wer, dann wohin.
    if(RAD_GameIsFinished(game))
    {
        return RAD_GAME_ERROR_FINISHED;
    }
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

bool RAD_GameUnitCanAttack(const RAD_Game_t *game, RAD_UnitId_t unit)
{
    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    return (RAD_GameCheckUnitMayAct(candidate) == RAD_GAME_OK) && !candidate->turn.attacked;
}

///
/// Eine Einheit greift ein Feld an. Die Regel steht in RAD_GameCheckAttack und
/// gilt hier noch einmal, wie beim Aufstellen. Abgelehnt wird still.
///
/// Angenommen zaehlt der Angriff, bevor geschossen wird (RAD_Unit_t.turn), und
/// gemeldet wird er danach, mit dem, was er bewirkt hat (RAD_AttackResult_t).
///
void RAD_GameHandleAttackCommand(RAD_Game_t *game, const RAD_CommandAttack_t *attack)
{
    const RAD_GameResult_t allowed = RAD_GameCheckAttack(game, attack->unit, attack->x, attack->y);
    if(allowed != RAD_GAME_OK)
    {
        printf("Unit with Id %i does not attack: %s\n", attack->unit, RAD_GameResultText(allowed));
        return;
    }

    RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, attack->unit);
    unit->turn.attacked = 1;

    RAD_Unit_t *target = RAD_WorldUnitAt(&game->world, attack->x, attack->y);
    RAD_AttackResult_t result = {
        .attacker = unit->id,
        .target = (target != NULL) ? target->id : RAD_UNIT_NONE,
        .x = attack->x,
        .y = attack->y
    };

    // Eine Einheit ohne Mitglieder kommt aus keiner Armee (RAD_WorldAddReserveUnit
    // verlangt eins), wohl aber aus RAD_WorldSpawnUnit. Auf sie wird nicht
    // geschossen -- es gaebe kein Mitglied, das ein Treffer waehlen koennte.
    if((target != NULL) && (target->number_of_members <= 0))
    {
        target = NULL;
    }

    const int32_t distance = RAD_GameDistance(unit, attack->x, attack->y);

    // Die Deckung des Zielfeldes, einmal fuer den ganzen Angriff: das Ziel steht
    // dort, bis es faellt, und dann wird nicht mehr geschossen.
    const int32_t cover = RAD_WorldTileAt(&game->world, attack->x, attack->y)->terrain.cover;
    for(int32_t m = 0; (target != NULL) && (m < unit->number_of_members) && (m < RAD_UNIT_MAX_MEMBERS); ++m)
    {
        const RAD_UnitMember_t *shooter = &unit->members[m];
        for(int32_t w = 0; (target != NULL) && (w < shooter->number_of_weapons) && (w < RAD_UNIT_MAX_WEAPONS); ++w)
        {
            const RAD_Weapon_t *weapon = &shooter->weapons[w];
            if(!RAD_GameWeaponReaches(weapon, distance))
            {
                continue;
            }

            for(int32_t shot = 0; (target != NULL) && (shot < weapon->shots); ++shot)
            {
                RAD_GameFireShot(game, shooter, weapon, cover, target, &result);

                // Ohne Mitglied keine Einheit: vom Feld, und der Angriff ist aus.
                if(target->number_of_members == 0)
                {
                    RAD_GameDestroyUnit(game, target->id);
                    result.target_destroyed = true;
                    target = NULL;
                }
            }
        }
    }

    // Gemeldet nach dem Kampf: ein Abonnent soll sie schon als eine sehen, die
    // angegriffen hat, und das Ziel mit dem, was davon uebrig ist.
    RAD_EventManagerPublishUnitAttacked(game->event_manager, &result);
}

///
/// Ein Schuss von "shooter" mit "weapon" auf "target", das mit der Deckung
/// "cover" steht, durch die vier Stufen oben. Jede Stufe, die er schafft, zaehlt
/// in "result"; an der ersten, die er nicht schafft, ist er zu Ende. "target" hat
/// wenigstens ein Mitglied.
///
static void RAD_GameFireShot(RAD_Game_t *game, const RAD_UnitMember_t *shooter, const RAD_Weapon_t *weapon,
                             int32_t cover, RAD_Unit_t *target, RAD_AttackResult_t *result)
{
    result->shots++;

    // 1. Trifft er das Feld, gegen die Deckung darauf? Getroffen ist dann eines der
    // Mitglieder, die noch da sind.
    if(RAD_DiceD10(&game->dice) < shooter->accuracy + cover)
    {
        return;
    }
    result->hits++;
    const int32_t victim_index = RAD_DiceRoll(&game->dice, target->number_of_members) - 1;
    RAD_UnitMember_t *victim = &target->members[victim_index];

    // 2. Kommt er durch die Ruestung?
    if(RAD_DiceD10(&game->dice) + weapon->penetration < victim->armor)
    {
        return;
    }
    result->penetrations++;

    // 3. Wirkt er gegen den Widerstand?
    if(RAD_DiceD10(&game->dice) + weapon->strength < victim->toughness)
    {
        return;
    }
    result->wounds++;

    // 4. Ein Lebenspunkt weniger -- und bei keinem mehr ist das Mitglied weg.
    victim->health--;
    if(victim->health <= 0)
    {
        RAD_GameRemoveMember(target, victim_index);
        result->members_killed++;
    }
}

///
/// Nimmt das Mitglied "member" aus der Einheit; die hinter ihm ruecken auf, damit
/// die Reihe ohne Luecke und in ihrer Reihenfolge bleibt.
///
static void RAD_GameRemoveMember(RAD_Unit_t *unit, int32_t member)
{
    const int32_t behind = unit->number_of_members - member - 1;
    if(behind > 0)
    {
        memmove(&unit->members[member], &unit->members[member + 1], (size_t)behind * sizeof(unit->members[0]));
    }
    unit->number_of_members--;
    memset(&unit->members[unit->number_of_members], 0, sizeof(unit->members[0]));
}

///
/// Reicht wenigstens eine Waffe eines Mitglieds von "unit" bis (x, y)? Die
/// Entfernung in Feldern waagerecht plus senkrecht, zwischen min_range und
/// max_range der Waffe (RAD_GameCheckAttack). Das eigene Feld nie.
///
static bool RAD_GameUnitReaches(const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    const int32_t distance = RAD_GameDistance(unit, x, y);
    if(distance == 0)
    {
        return false;
    }

    for(int32_t m = 0; (m < unit->number_of_members) && (m < RAD_UNIT_MAX_MEMBERS); ++m)
    {
        const RAD_UnitMember_t *member = &unit->members[m];
        for(int32_t w = 0; (w < member->number_of_weapons) && (w < RAD_UNIT_MAX_WEAPONS); ++w)
        {
            if(RAD_GameWeaponReaches(&member->weapons[w], distance))
            {
                return true;
            }
        }
    }

    return false;
}

static bool RAD_GameWeaponReaches(const RAD_Weapon_t *weapon, int32_t distance)
{
    return (distance >= weapon->min_range) && (distance <= weapon->max_range);
}

/// Felder waagerecht plus senkrecht von "unit" bis (x, y).
static int32_t RAD_GameDistance(const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    const int32_t dx = x - unit->x;
    const int32_t dy = y - unit->y;
    return ((dx < 0) ? -dx : dx) + ((dy < 0) ? -dy : dy);
}
