#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <stddef.h>

///
/// Die Fabriken fuellen den Kopf gleich: Art, die naechste Sequenznummer und den
/// Absender aus game->local_user. Der Absender steht damit an einer Stelle --
/// wer ein Kommando erzeugt, muss ihn nicht kennen und kann ihn nicht vergessen.
///
/// Alle geben bool zurueck. Aus Koordinaten und einer Id laesst sich wenig
/// hinschreiben, was nicht erst beim Ausfuehren auffaellt; RAD_GameDeployUnit
/// lehnt nur eine Id unter 0 ab. RAD_GameMoveUnit hat einen echten Grund
/// dagegen -- ein Pfad kann unmoeglich sein, bevor ihn jemand laeuft: kein
/// Zeiger, kein Schritt oder mehr Schritte, als einer traegt. Sie weist ihn ab,
/// ohne "output" anzufassen und ohne eine Sequenznummer zu verbrauchen. Ein
/// abgelehnter Aufruf hinterlaesst damit kein halbes Kommando und keine Luecke in
/// der Reihe des Absenders, die die Gegenseite fuer ein verlorenes Kommando
/// halten muesste.
///

bool RAD_GameDeployUnit(RAD_Game_t *game, RAD_UnitId_t unit, int16_t x, int16_t y, RAD_Command_t *output)
{
    // Erst pruefen, dann schreiben: ein abgelehnter Aufruf fasst "output" nicht an
    // und verbraucht keine Sequenznummer.
    if((game == NULL) || (output == NULL) || (unit < 0))
    {
        return false;
    }

    output->header.type = RAD_COMMAND_TYPE_DEPLOY_UNIT;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.deploy_unit.unit = unit;
    output->command.deploy_unit.x = x;
    output->command.deploy_unit.y = y;

    return true;
}

bool RAD_GameDestroyUnit(RAD_Game_t *game, RAD_UnitId_t id, RAD_Command_t *output)
{
    output->header.type = RAD_COMMAND_TYPE_REMOVE_UNIT;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.remove_unit.unit = id;
    return true;
}

bool RAD_GameShoot(RAD_Game_t *game, RAD_UnitId_t id, int16_t x, int16_t y, RAD_Command_t *output)
{
    output->header.sequence = game->current_sequence_number;
    output->header.type = RAD_COMMAND_TYPE_NONE;
    output->header.user = game->local_user;
    output->command.shoot.unit = RAD_COMMAND_TYPE_NONE;
    output->command.shoot.weapon = 0;
    output->command.shoot.x = 0;
    output->command.shoot.y = 0;

    if((game == NULL) || (output == NULL))
    {
        return false;
    }
    
    output->header.type = RAD_COMMAND_TYPE_SHOOT;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.shoot.unit = id;
    output->command.shoot.x = x;
    output->command.shoot.y = y;
    return true;
}

bool RAD_GameMoveUnit(RAD_Game_t *game, RAD_UnitId_t id, const RAD_Path_t *path, RAD_Command_t *output)
{
    output->header.sequence = game->current_sequence_number;
    output->header.type = RAD_COMMAND_TYPE_NONE;
    output->header.user = game->local_user;
    output->command.move_unit.unit = RAD_UNIT_NONE;
    output->command.move_unit.path.number_of_steps = 0;

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
