#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <stddef.h>

///
/// Die drei Fabriken fuellen den Kopf gleich: Art, die naechste Sequenznummer und
/// den Absender aus game->local_user. Der Absender steht damit an einer Stelle --
/// wer ein Kommando erzeugt, muss ihn nicht kennen und kann ihn nicht vergessen.
///
/// Alle drei geben bool zurueck, und bisher war es immer true: aus zwei
/// Koordinaten und einer Id laesst sich nichts hinschreiben, was nicht erst beim
/// Ausfuehren auffaellt. RAD_GameMoveEntity ist die erste mit einem echten Grund
/// dagegen -- ein Pfad kann unmoeglich sein, bevor ihn jemand laeuft: kein
/// Zeiger, kein Schritt oder mehr Schritte, als einer traegt. Sie weist ihn ab,
/// ohne "output" anzufassen und ohne eine Sequenznummer zu verbrauchen. Ein
/// abgelehnter Aufruf hinterlaesst damit kein halbes Kommando und keine Luecke in
/// der Reihe des Absenders, die die Gegenseite fuer ein verlorenes Kommando
/// halten muesste.
///

bool RAD_GameSpawnEntity(RAD_Game_t *game, RAD_EntityType_t type, int32_t x, int32_t y, int32_t z, RAD_Command_t *output)
{
    output->header.type = RAD_COMMAND_TYPE_SPAWN_ENTITY;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.spawn_entity.entity_type = type; 
    output->command.spawn_entity.x = x;
    output->command.spawn_entity.y = y;
    output->command.spawn_entity.z = z;
    
    return true;
}

bool RAD_GameDestroyEntity(RAD_Game_t *game, RAD_EntityId_t id, RAD_Command_t *output)
{
    output->header.type = RAD_COMMAND_TYPE_REMOVE_ENTITY;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.remove_entity.entity = id;
    return true;
}

bool RAD_GameMoveEntity(RAD_Game_t *game, RAD_EntityId_t id, const RAD_EntityPath_t *path, RAD_Command_t *output)
{
    if((game == NULL) || (path == NULL) || (output == NULL))
    {
        return false;
    }

    // Erst pruefen, dann schreiben -- und die Sequenznummer laeuft erst danach
    // weiter. Sonst waere ein abgelehnter Zug eine verbrauchte Nummer, die nie
    // ueber die Strecke geht.
    if((path->number_of_steps < 1) || (path->number_of_steps > RAD_PATH_MAX_STEPS))
    {
        return false;
    }

    output->header.type = RAD_COMMAND_TYPE_MOVE_ENTITY;
    output->header.sequence = game->current_sequence_number++;
    output->header.user = game->local_user;

    output->command.move_entity.entity = id;
    output->command.move_entity.path.number_of_steps = path->number_of_steps;

    for(int32_t i = 0; i < RAD_PATH_MAX_STEPS; ++i)
    {
        // Kopiert wird der Pfad und nicht der Zeiger: ein Kommando ist Daten und
        // soll nichts festhalten, was dem Aufrufer gehoert. Hinter dem Zaehler
        // wird genullt, damit im Kommando kein Rest dessen steht, was beim
        // Aufrufer hinter seinen Schritten lag -- der Codec schreibt diese Plaetze
        // ohnehin als (0,0) heraus (move_entity.h).
        const bool used = (i < path->number_of_steps);

        output->command.move_entity.path.steps_to[i].x = used ? path->steps_to[i].x : 0;
        output->command.move_entity.path.steps_to[i].y = used ? path->steps_to[i].y : 0;
    }

    return true;
}
