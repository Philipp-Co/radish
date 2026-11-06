#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/control/commands.h>
#include <radish/game/control/unit_turn.h>
#include <radish/game/control/visibility.h>
#include <stddef.h>
#include <stdio.h>

///
/// Der Zug einer Einheit ueber einen Weg (RAD_COMMAND_TYPE_MOVE_UNIT): die
/// Fabrik, die Regel und das Ausfuehren -- alles, was zu diesem Kommando
/// gehoert, an einer Stelle. Was Ziehen und Angreifen gemeinsam verlangen,
/// steht in control/unit_turn.c.
///

bool RAD_GameMoveUnit(RAD_Game_t *game, RAD_UnitId_t id, const RAD_Path_t *path, RAD_Command_t *output)
{
    if((game == NULL) || (path == NULL) || (output == NULL))
    {
        return false;
    }

    // Erst pruefen, dann schreiben -- und die Sequenznummer laeuft erst danach
    // weiter. Sonst waere ein abgelehnter Zug eine verbrauchte Nummer, die nie
    // ueber die Strecke geht.
    // Zwei Felder mindestens: eines ist der Standort und kein Weg (path.h).
    if((path->number_of_steps < 2) || (path->number_of_steps > RAD_PATH_MAX_STEPS))
    {
        return false;
    }

    output->header.type = RAD_COMMAND_TYPE_MOVE_UNIT;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.move_unit.unit = id;
    output->command.move_unit.path.number_of_steps = path->number_of_steps;

    for(int32_t i = 0; i < RAD_PATH_MAX_STEPS; ++i)
    {
        // Kopiert wird der Pfad und nicht der Zeiger: ein Kommando ist Daten und
        // soll nichts festhalten, was dem Aufrufer gehoert. Hinter dem Zaehler
        // wird genullt, damit im Kommando kein Rest dessen steht, was beim
        // Aufrufer hinter seinen Schritten lag -- der Codec schreibt diese Plaetze
        // ohnehin als (0,0) heraus (move_unit.h).
        const bool used = (i < path->number_of_steps);

        output->command.move_unit.path.steps_to[i].x = used ? path->steps_to[i].x : 0;
        output->command.move_unit.path.steps_to[i].y = used ? path->steps_to[i].y : 0;
    }

    return true;
}

RAD_GameResult_t RAD_GameCheckMoveUnit(const RAD_Game_t *game, RAD_UnitId_t unit)
{
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
    if(candidate->turn.moved)
    {
        return RAD_GAME_ERROR_UNIT_ALREADY_MOVED;
    }

    return RAD_GAME_OK;
}

RAD_GameResult_t RAD_GameCheckMovePath(const RAD_Game_t *game, RAD_UnitId_t unit, const RAD_Path_t *path)
{
    const RAD_GameResult_t may_move = RAD_GameCheckMoveUnit(game, unit);
    if(may_move != RAD_GAME_OK)
    {
        return may_move;
    }
    if((path == NULL) || (path->number_of_steps < 2) || (path->number_of_steps > RAD_PATH_MAX_STEPS))
    {
        return RAD_GAME_ERROR_INVALID_PATH;
    }

    // RAD_GameCheckMoveUnit hat sie gefunden, und sie steht auf dem Feld.
    const RAD_Unit_t *candidate = RAD_UnitPoolUnitById(game->unit_pool, unit);
    int32_t x = candidate->x;
    int32_t y = candidate->y;
    int32_t cost = 0;

    // Ab eins: steps_to[0] ist der Standort und kein Schritt (path.h).
    for(int32_t i = 1; i < path->number_of_steps; ++i)
    {
        const int32_t next_x = path->steps_to[i].x;
        const int32_t next_y = path->steps_to[i].y;
        const int32_t dx = next_x - x;
        const int32_t dy = next_y - y;
        if(((dx < 0) ? -dx : dx) + ((dy < 0) ? -dy : dy) != 1)
        {
            return RAD_GAME_ERROR_INVALID_PATH;
        }
        if(!RAD_WorldInBounds(&game->world, next_x, next_y))
        {
            return RAD_GAME_ERROR_OUT_OF_BOUNDS;
        }

        const RAD_Tile_t *tile = &game->world.tiles[next_y][next_x];
        if(tile->type == RAD_TILE_TYPE_VOID)
        {
            return RAD_GAME_ERROR_NO_GROUND;
        }
        if(tile->terrain.impassable)
        {
            return RAD_GAME_ERROR_IMPASSABLE;
        }

        cost += RAD_TileMovementCost(tile);
        x = next_x;
        y = next_y;
    }

    if(cost > candidate->movement)
    {
        return RAD_GAME_ERROR_OUT_OF_MOVEMENT;
    }
    return RAD_GAME_OK;
}

int32_t RAD_TileMovementCost(const RAD_Tile_t *tile)
{
    // Ein Feld, das die Reichweite erhoeht, ist nicht umsonst (tile.h).
    const int32_t cost = 1 - (int32_t)tile->terrain.movement_modifier;
    return (cost > 1) ? cost : 1;
}

bool RAD_GameUnitCanMove(const RAD_Game_t *game, RAD_UnitId_t unit)
{
    return RAD_GameCheckMoveUnit(game, unit) == RAD_GAME_OK;
}

///
/// Ein gelungener Zug fuer die, die nicht ihn besitzen: jeder andere Mitspieler
/// bekommt den Teil des Weges, den er sieht, mit je einem Randfeld davor und
/// danach (RAD_GameObservedPartOfPath), als eigene Fassung -- so faehrt eine
/// Einheit sichtbar in den Nebel hinein oder aus ihm heraus. Bleibt weniger als
/// zwei Felder, sieht er gar nichts davon und bekommt nichts. Ob die Einheit
/// dabei in seine Sicht kam oder aus ihr verschwand, ist eine andere Meldung.
///
static void RAD_GameNotifyObservers(RAD_Game_t *game, const RAD_Unit_t *unit, const RAD_Path_t *path)
{
    for(int32_t i = 0; i < RAD_GameNumberOfPlayers(game); ++i)
    {
        const RAD_UserId_t observer = RAD_GamePlayerAt(game, i);
        if(observer == unit->owner)
        {
            continue;
        }

        RAD_Path_t seen;
        if(RAD_GameObservedPartOfPath(game, observer, path, &seen) < 2)
        {
            continue;
        }

        RAD_Recipients_t only_observer = { .number_of_users = 0, .variant = true };
        RAD_RecipientsAdd(&only_observer, observer);
        RAD_WorldNotifyUnitMoved(&game->world, unit->id, &seen, 0, &only_observer);
    }
}

void RAD_GameHandleMoveCommand(RAD_Game_t *game, RAD_CommandMoveUnit_t *move)
{
    int32_t result = -1;
    //
    // Der valide Pfad ist zum Beginn leer. Er fuellt sich mit jedem
    // Schleifendurchlauf um ein weiteres Feld -- und faengt mit dem an, auf dem die
    // Figur schon steht, damit er dieselbe Gestalt hat wie der hereingekommene
    // (path.h): erstes Feld der Standort, letztes das Ziel.
    //
    RAD_Path_t valid_path = {
        .number_of_steps = 0
    };
    RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, move->unit);
    //
    // Wer davon erfaehrt, entscheidet sich hier und nicht in der Welt. Den ganzen
    // Weg -- und einen abgelehnten Zug -- bekommt nur der Besitzer; wer ihm sonst
    // zusieht, bekommt nach dem Zug, was er davon sah (RAD_GameNotifyObservers).
    // Ohne Einheit bleibt die Liste leer.
    //
    RAD_Recipients_t recipients = {0};
    if(NULL == unit)
    {
        printf("Entity with Id %i does not exist!\n", move->unit);
        goto end;
    }
    RAD_RecipientsAdd(&recipients, unit->owner);
    //
    // Nur was auf dem Feld steht, kann ziehen -- eine Einheit in der Reserve oder
    // eine zerstoerte hat keine Position --, und nur einmal je Zug, nicht im Zug
    // ihres Aufstellens, ueber einen Weg von Nachbar zu Nachbar, der begehbar ist
    // und in ihre Bewegung passt (RAD_GameCheckMovePath). Der Server hat das
    // schon gefragt; geschrieben wird trotzdem nur, was hier geprueft ist.
    //
    const RAD_GameResult_t allowed = RAD_GameCheckMovePath(game, move->unit, &move->path);
    if(RAD_GAME_OK != allowed)
    {
        printf("Unit with Id %i does not move: %s\n", move->unit, RAD_GameResultText(allowed));
        goto end;
    }
    //
    // Erstmal pruefen.
    //
    int32_t x = unit->x;
    int32_t y = unit->y;

    //
    // Der Standort ist das erste Feld des gelaufenen Weges. Er wird nicht geprueft:
    // dort steht die Figur, also ist das Feld besetzt -- von ihr selbst.
    //
    valid_path.steps_to[0].x = (int16_t)x;
    valid_path.steps_to[0].y = (int16_t)y;
    valid_path.number_of_steps = 1;

    //
    // Ab eins und nicht ab null: steps_to[0] ist der Standort und kein Schritt
    // (path.h). Bei null zu beginnen hiesse, das Feld unter der Figur auf Belegung
    // zu pruefen -- es ist belegt, und jede Bewegung endete sofort.
    //
    // Was in steps_to[0] steht, wird dabei nicht mit dem Standort verglichen. Ein
    // zweimal zugestelltes Kommando muss zum selben Ergebnis fuehren (command.h);
    // beim zweiten Mal steht die Figur schon am Ziel, und ein Vergleich waere dann
    // ein Fehlschlag, wo keiner sein soll.
    //
    uint32_t i = 1;
    for(;i<((uint32_t)move->path.number_of_steps); ++i)
    {
        //
        // Check if the next Tile is empty.
        //
        const int16_t tmp_x = move->path.steps_to[i].x;
        const int16_t tmp_y = move->path.steps_to[i].y;
        const RAD_Tile_t *tile = RAD_WorldTileAt(&game->world, tmp_x, tmp_y);
        if(NULL == tile)
        {
            valid_path.number_of_steps = 0;
            goto end;
        }
        else if(RAD_UNIT_NONE != tile->unit)
        {
            //
            // Sobald die naechste Tile besetzt ist, hoert die Bewegung auf.
            //
            result = 0;
            break;
        }
        x = tmp_x;
        y = tmp_y;
        valid_path.steps_to[valid_path.number_of_steps].x = x;
        valid_path.steps_to[valid_path.number_of_steps].y = y;
        valid_path.number_of_steps++;
    }
    result = 0;

    //
    // Nur der Standort im Pfad heisst: kein Schritt ging durch. Dann bleibt der
    // Pfad leer -- ein Weg aus einem Feld ist keiner, und ein Abonnent soll nicht
    // erst nachrechnen muessen, ob sich etwas bewegt hat.
    //
    if(valid_path.number_of_steps < 2)
    {
        valid_path.number_of_steps = 0;
    }

    //
    // Jetzt erst schreiben -- ueber die Welt, die Feld und Einheit synchron haelt
    // und den Zug meldet. Gezogen ist sie, sobald sie wenigstens ein Feld weit
    // kam, auch wenn der Weg danach besetzt war; gesetzt vor dem Zug, damit ein
    // Abonnent sie schon so sieht, wie beim Aufstellen (control/deploy_unit.c).
    // Kam sie gar nicht los, darf sie es noch einmal versuchen.
    //
    if(valid_path.number_of_steps >= 2)
    {
        unit->turn.moved = 1;
        if(RAD_WorldMoveUnitAlong(&game->world, unit->id, &valid_path, &recipients))
        {
            RAD_GameNotifyObservers(game, unit, &valid_path);
            return;
        }
        // Die Welt lehnt nur ab, was oben schon geprueft ist -- kommt es doch
        // vor, ist nichts geschehen, und gemeldet wird wie bei jedem Zug, der
        // nicht losging.
        unit->turn.moved = 0;
        valid_path.number_of_steps = 0;
    }

end:
    //
    // Nichts geschrieben, aber gemeldet: ein Absender wartet auf Antwort, und ein
    // abgewiesener Zug soll von einem verlorenen nicht zu unterscheiden sein.
    //
    RAD_WorldNotifyUnitMoved(&game->world, move->unit, &valid_path, result, &recipients);
}
