#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/control/commands.h>
#include <radish/game/control/unit_turn.h>
#include <radish/game/control/dice.h>
#include <radish/game/control/visibility.h>
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
///     1. Treffer       W10 >= Genauigkeit des Schuetzen
///                      -> ein zufaelliges Mitglied der Zieleinheit ist getroffen
///     2. Durchschlag   W10 + Durchschlag der Waffe >= Ruestung des Getroffenen
///     3. Wirkung       W10 + Staerke der Waffe     >= Widerstand des Getroffenen
///     4. Schaden       ein Lebenspunkt weniger
///
/// Faellt ein Mitglied auf 0, ist es nicht mehr da; hat die Einheit keins mehr,
/// ist sie zerstoert (RAD_GameDestroyUnit), und der Angriff ist zu Ende -- die
/// uebrigen Schuesse werden nicht gewuerfelt.
///
/// **Gezaehlt wird je Waffe** (RAD_AttackWeaponResult_t): jede Waffe, die
/// schiesst, bekommt einen Eintrag im Ergebnis, und erst ihre Summe ergibt die
/// Zaehler des ganzen Angriffs.
///
/// **Die Deckung des Gelaendes** (RAD_Terrain_t.cover) spielt im Kampf keine
/// Rolle: das Feld, auf dem das Ziel steht, erschwert den Treffer nicht.
///
/// **Die Reihenfolge ist fest**: Mitglieder, darin Waffen, darin Schuesse, je in
/// ihrer Reihenfolge. Zum selben Seed kommt also derselbe Kampf heraus.
///
/// Angegriffen werden darf jedes Feld in Reichweite und in Sicht, auch ein
/// leeres -- dann wird nicht geschossen -- und auch eine eigene Einheit, die dann
/// wie jede andere getroffen wird. Spaetere Regeln koennen das einschraenken.
///
/// **Die Sicht** versperren Gelaende (RAD_Terrain_t.blocks_sight) und Einheiten
/// auf den Feldern zwischen Schuetze und Ziel (RAD_GameHasLineOfSight).
///

static bool RAD_GameUnitReaches(const RAD_Unit_t *unit, int32_t x, int32_t y);
static bool RAD_GameWeaponReaches(const RAD_Weapon_t *weapon, int32_t distance);
static bool RAD_GameTileBlocksSight(const RAD_Game_t *game, int32_t x, int32_t y);
static void RAD_GameFireShot(RAD_Game_t *game, const RAD_UnitMember_t *shooter, const RAD_Weapon_t *weapon,
                             RAD_Unit_t *target, RAD_AttackWeaponResult_t *counts);
static void RAD_GameAddWeaponResult(RAD_AttackResult_t *result, const RAD_AttackWeaponResult_t *counts);
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
    if(!RAD_GameHasLineOfSight(game, candidate->x, candidate->y, x, y))
    {
        return RAD_GAME_ERROR_NO_LINE_OF_SIGHT;
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

    // Wer davon erfaehrt, entscheidet sich hier und nicht in der Welt -- und
    // vor dem Kampf: faellt das Ziel, saehe danach mancher durch sein Feld, der
    // es beim Schuss nicht tat. Zwei Listen:
    //
    //   ganz          der Besitzer der angreifenden Einheit und jeder, der ihr
    //                 Feld sieht -- sie wissen, wer geschossen hat
    //   ohne Schuetze der Besitzer der getroffenen Einheit und jeder, der das
    //                 Zielfeld sieht, aber nicht den Schuetzen -- sie erfahren,
    //                 was ankam, nicht woher (RAD_AttackResult_t.attacker leer)
    //
    // Wer nur den Schuetzen sieht, bekommt das ganze Ergebnis, auch das Ziel.
    // Eine bekannte Luecke: er erfaehrt so, was auf einem Feld stand, das er
    // nicht sieht.
    RAD_Recipients_t whole;
    RAD_GameRecipientsForTile(game, unit->owner, unit->x, unit->y, &whole);
    RAD_Recipients_t at_target;
    RAD_GameRecipientsForTile(game, (target != NULL) ? target->owner : RAD_USER_NONE,
                              attack->x, attack->y, &at_target);
    RAD_Recipients_t anonymous = { .number_of_users = 0, .variant = true };
    for(int32_t i = 0; i < at_target.number_of_users; ++i)
    {
        if(!RAD_RecipientsContains(&whole, at_target.users[i]))
        {
            RAD_RecipientsAdd(&anonymous, at_target.users[i]);
        }
    }

    // Eine Einheit ohne Mitglieder kommt aus keiner Armee (RAD_WorldAddReserveUnit
    // verlangt eins), wohl aber aus RAD_WorldSpawnUnit. Auf sie wird nicht
    // geschossen -- es gaebe kein Mitglied, das ein Treffer waehlen koennte.
    if((target != NULL) && (target->number_of_members <= 0))
    {
        target = NULL;
    }

    const int32_t distance = RAD_GameDistance(unit, attack->x, attack->y);

    for(int32_t m = 0; (target != NULL) && (m < unit->number_of_members) && (m < RAD_UNIT_MAX_MEMBERS); ++m)
    {
        const RAD_UnitMember_t *shooter = &unit->members[m];
        for(int32_t w = 0; (target != NULL) && (w < shooter->number_of_weapons) && (w < RAD_UNIT_MAX_WEAPONS); ++w)
        {
            const RAD_Weapon_t *weapon = &shooter->weapons[w];
            if(!RAD_GameWeaponReaches(weapon, distance) || (weapon->shots <= 0))
            {
                continue;
            }

            // Ein Eintrag je Waffe, die schiesst -- hoechstens einer je Waffe
            // jedes Mitglieds, das Feld reicht also (RAD_ATTACK_MAX_WEAPONS).
            RAD_AttackWeaponResult_t *counts = &result.weapons[result.number_of_weapons++];
            counts->member = m;
            counts->weapon = w;

            for(int32_t shot = 0; (target != NULL) && (shot < weapon->shots); ++shot)
            {
                RAD_GameFireShot(game, shooter, weapon, target, counts);

                // Ohne Mitglied keine Einheit: vom Feld, und der Angriff ist aus.
                if(target->number_of_members == 0)
                {
                    RAD_GameDestroyUnit(game, target->id);
                    result.target_destroyed = true;
                    target = NULL;
                }
            }
            RAD_GameAddWeaponResult(&result, counts);
        }
    }

    // Gemeldet nach dem Kampf: ein Abonnent soll sie schon als eine sehen, die
    // angegriffen hat, und das Ziel mit dem, was davon uebrig ist -- an die
    // beiden Listen von oben, ohne Schuetzen an die zweite. Der Abonnent ohne
    // Empfaenger bekommt es genau einmal, ganz; die zweite ist eine weitere
    // Fassung und geht nur an ihre Liste (RAD_Recipients_t.variant).
    RAD_WorldNotifyUnitAttacked(&game->world, &result, &whole);
    if(anonymous.number_of_users > 0)
    {
        RAD_AttackResult_t without_attacker = result;
        without_attacker.attacker = RAD_UNIT_NONE;
        without_attacker.number_of_weapons = 0;
        RAD_WorldNotifyUnitAttacked(&game->world, &without_attacker, &anonymous);
    }
}

///
/// Ein Schuss von "shooter" mit "weapon" auf "target", durch die vier Stufen
/// oben. Jede Stufe, die er schafft, zaehlt
/// in "counts", dem Eintrag seiner Waffe; an der ersten, die er nicht schafft,
/// ist er zu Ende. "target" hat wenigstens ein Mitglied.
///
static void RAD_GameFireShot(RAD_Game_t *game, const RAD_UnitMember_t *shooter, const RAD_Weapon_t *weapon,
                             RAD_Unit_t *target, RAD_AttackWeaponResult_t *counts)
{
    counts->shots++;

    // 1. Trifft er das Feld? Getroffen ist dann eines der Mitglieder, die noch
    // da sind.
    if(RAD_DiceD10(&game->dice) < shooter->accuracy)
    {
        return;
    }
    counts->hits++;
    const int32_t victim_index = RAD_DiceRoll(&game->dice, target->number_of_members) - 1;
    RAD_UnitMember_t *victim = &target->members[victim_index];

    // 2. Kommt er durch die Ruestung?
    if(RAD_DiceD10(&game->dice) + weapon->penetration < victim->armor)
    {
        return;
    }
    counts->penetrations++;

    // 3. Wirkt er gegen den Widerstand?
    if(RAD_DiceD10(&game->dice) + weapon->strength < victim->toughness)
    {
        return;
    }
    counts->wounds++;

    // 4. Ein Lebenspunkt weniger -- und bei keinem mehr ist das Mitglied weg.
    victim->health--;
    if(victim->health <= 0)
    {
        RAD_GameRemoveMember(target, victim_index);
        counts->members_killed++;
    }
}

///
/// Zaehlt den Eintrag einer Waffe in die Zaehler des ganzen Angriffs.
///
static void RAD_GameAddWeaponResult(RAD_AttackResult_t *result, const RAD_AttackWeaponResult_t *counts)
{
    result->shots += counts->shots;
    result->hits += counts->hits;
    result->penetrations += counts->penetrations;
    result->wounds += counts->wounds;
    result->members_killed += counts->members_killed;
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

///
/// Die Regel steht in game.h. Gelaufen wird die Linie Feld fuer Feld, wie sie
/// die Kanten des Rasters kreuzt -- jedes Feld, durch das sie geht, und keins
/// daneben.
///
/// Gerechnet wird in ganzen Zahlen: die Linie geht von Mitte zu Mitte, nach
/// ix Schritten waagerecht und iy senkrecht steht sie vor der Kante bei
/// (2*ix + 1) / (2*nx) bzw. (2*iy + 1) / (2*ny) ihrer Laenge. Welche sie
/// zuerst kreuzt, sagt der Vergleich ueber Kreuz; sind beide gleich, geht sie
/// durch die Ecke. Zum selben Paar Felder kommt also immer dieselbe Antwort, und
/// in umgekehrter Richtung dieselben Felder.
///
bool RAD_GameHasLineOfSight(const RAD_Game_t *game, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y)
{
    if((game == NULL)
       || !RAD_WorldInBounds(&game->world, from_x, from_y)
       || !RAD_WorldInBounds(&game->world, to_x, to_y))
    {
        return false;
    }

    const int32_t nx = (to_x > from_x) ? (to_x - from_x) : (from_x - to_x);
    const int32_t ny = (to_y > from_y) ? (to_y - from_y) : (from_y - to_y);
    const int32_t sx = (to_x > from_x) ? 1 : -1;
    const int32_t sy = (to_y > from_y) ? 1 : -1;

    int32_t x = from_x;
    int32_t y = from_y;
    for(int32_t ix = 0, iy = 0; (ix < nx) || (iy < ny);)
    {
        const int32_t decision = (1 + 2 * ix) * ny - (1 + 2 * iy) * nx;
        if(decision == 0)
        {
            // Durch die Ecke: versperrt nur, wenn beide Felder daneben es sind.
            // Keins davon ist ein Ende -- das naechste Feld liegt schraeg.
            if(RAD_GameTileBlocksSight(game, x + sx, y) && RAD_GameTileBlocksSight(game, x, y + sy))
            {
                return false;
            }
            x += sx;
            y += sy;
            ++ix;
            ++iy;
        }
        else if(decision < 0)
        {
            x += sx;
            ++ix;
        }
        else
        {
            y += sy;
            ++iy;
        }

        // Das Ziel selbst versperrt nichts.
        if(((x != to_x) || (y != to_y)) && RAD_GameTileBlocksSight(game, x, y))
        {
            return false;
        }
    }

    return true;
}

///
/// Versperrt das Feld (x, y) die Sicht -- sein Gelaende oder eine Einheit darauf?
/// (x, y) liegt in der Welt: es liegt auf der Linie zwischen zwei Feldern darin,
/// und die Welt ist ein Rechteck.
///
static bool RAD_GameTileBlocksSight(const RAD_Game_t *game, int32_t x, int32_t y)
{
    const RAD_Tile_t *tile = &game->world.tiles[y][x];
    return tile->terrain.blocks_sight || (tile->unit != RAD_UNIT_NONE);
}
