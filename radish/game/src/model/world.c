#include <radish/game/model/world/world.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void RAD_WorldPublishTileTransition(RAD_World_t *world, RAD_TileType_t previous_type, int32_t previous_z, const RAD_Tile_t *tile);
static bool RAD_WorldUnitValuesFit(const RAD_Unit_t *unit, int32_t min_members);
static bool RAD_WorldNameFits(const char name[RAD_UNIT_NAME_MAX]);

void RAD_CreateWorld(RAD_World_t *world, RAD_EventManager_t *event_manager, RAD_UnitPool_t *units)
{
    world->width = RAD_WORLD_WIDTH;
    world->height = RAD_WORLD_HEIGHT;
    world->units = units;
    world->event_manager = event_manager;
}

void RAD_InitWorld(RAD_World_t *world)
{
    RAD_ResetWorld(world);

    // Vorher war nichts, jetzt ist Boden: fuer einen Abonnenten ist das VOID -> X,
    // also added -- derselbe Uebergang wie in RAD_WorldAddTile.
    for(int32_t y=0;y < world->height; ++y)
    {
        for(int32_t x=0;x < world->width; ++x)
        {
            RAD_EventManagerPublishTileAddedToGameEvent(world->event_manager, &(world->tiles[y][x]));
        }
    }
}

void RAD_ResetWorld(RAD_World_t *world)
{
    RAD_ResetWorldToSize(world, world->width, world->height);
}

bool RAD_ResetWorldToSize(RAD_World_t *world, int32_t width, int32_t height)
{
    if(width < 1 || width > RAD_WORLD_WIDTH || height < 1 || height > RAD_WORLD_HEIGHT)
    {
        return false;
    }

    world->width = width;
    world->height = height;

    // Der ganze Speicher, nicht nur die Welt: auch ein Feld daneben traegt sein x
    // und y, denn RAD_WorldPublishTileChanges meldet es, wenn es beim Schrumpfen
    // wegfaellt (world.h).
    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            const bool inside = (x < width) && (y < height);
            world->tiles[y][x] = (RAD_Tile_t){
                .x = x,
                .y = y,
                .z = 0,
                .type = inside ? RAD_TILE_TYPE_GROUND : RAD_TILE_TYPE_VOID,
                .unit = RAD_UNIT_NONE
            };
        }
    }

    // Der Pool bleibt, wie er ist (world.h).
    return true;
}

bool RAD_WorldInBounds(const RAD_World_t *world, int32_t x, int32_t y)
{
    return (x >= 0) && (x < world->width) && (y >= 0) && (y < world->height);
}

RAD_Tile_t* RAD_WorldTileAt(RAD_World_t *world, int32_t x, int32_t y)
{
    if(!RAD_WorldInBounds(world, x, y))
    {
        return NULL;
    }
    return &world->tiles[y][x];
}

RAD_Unit_t* RAD_WorldUnitById(RAD_World_t *world, RAD_UnitId_t id)
{
    return RAD_UnitPoolUnitById(world->units, id);
}

RAD_Unit_t* RAD_WorldUnitAt(RAD_World_t *world, int32_t x, int32_t y)
{
    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if(tile == NULL)
    {
        return NULL;
    }
    return RAD_WorldUnitById(world, tile->unit);
}

bool RAD_WorldAddTile(RAD_World_t *world, int32_t x, int32_t y, int32_t z, RAD_TileType_t type)
{
    if(type == RAD_TILE_TYPE_VOID)
    {
        // Kein Gelaende hinzustellen ist Gelaende wegnehmen -- dasselbe Ergebnis,
        // also derselbe Weg. Das z faellt dabei weg: was nicht da ist, hat keine
        // Hoehe zu melden.
        return RAD_WorldRemoveTile(world, x, y);
    }

    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if(tile == NULL)
    {
        return false;
    }

    const RAD_TileType_t previous = tile->type;
    if((previous == type) && (tile->z == z))
    {
        // Derselbe Stand: geschrieben wird nichts und gemeldet auch nichts. Ein
        // Ereignis ohne Aenderung waere eine Nachricht ohne Inhalt.
        return true;
    }

    const int32_t previous_z = tile->z;
    tile->type = type;
    tile->z = z;

    RAD_WorldPublishTileTransition(world, previous, previous_z, tile);

    return true;
}

bool RAD_WorldRemoveTile(RAD_World_t *world, int32_t x, int32_t y)
{
    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if(tile == NULL)
    {
        return false;
    }

    if(tile->type == RAD_TILE_TYPE_VOID)
    {
        // Schon leer, und zwar auch unter einer Figur: die Pruefung darunter
        // schuetzt eine Aenderung, nicht das Feld.
        return true;
    }

    // Erst pruefen, dann schreiben -- dieselbe Reihenfolge wie in
    // RAD_WorldMoveUnit. Eine Figur haelt ihr Gelaende (world.h).
    if(tile->unit != RAD_UNIT_NONE)
    {
        return false;
    }

    // Nur der Typ. x, y, z und die Einheit bleiben stehen: weggenommen wird das
    // Gelaende und nicht das Feld.
    const RAD_TileType_t previous = tile->type;
    tile->type = RAD_TILE_TYPE_VOID;

    RAD_WorldPublishTileTransition(world, previous, tile->z, tile);

    return true;
}

void RAD_WorldPublishTileChanges(
    RAD_World_t *world,
    const RAD_Tile_t previous[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH],
    int32_t previous_width,
    int32_t previous_height)
{
    const int32_t width = (world->width > previous_width) ? world->width : previous_width;
    const int32_t height = (world->height > previous_height) ? world->height : previous_height;

    for(int32_t y=0;y < height; ++y)
    {
        for(int32_t x=0;x < width; ++x)
        {
            // Ausserhalb der alten Welt war nichts. Ausserhalb der neuen steht nach
            // RAD_ResetWorldToSize schon VOID im Speicher, das Feld selbst sagt es.
            const bool was_inside = (x < previous_width) && (y < previous_height);
            const RAD_TileType_t previous_type = was_inside ? previous[y][x].type : RAD_TILE_TYPE_VOID;

            RAD_WorldPublishTileTransition(world, previous_type, previous[y][x].z, &world->tiles[y][x]);
        }
    }
}

///
/// Die Tabelle aus world.h, an einer Stelle: RAD_WorldAddTile, RAD_WorldRemoveTile
/// und RAD_WorldPublishTileChanges melden alle ueber sie, damit ein Uebergang nicht
/// je nach Weg anders heisst. Die Hoehe zaehlt nur zwischen zwei Feldern mit
/// Gelaende -- was nicht da ist, hat keine Hoehe zu melden.
///
static void RAD_WorldPublishTileTransition(RAD_World_t *world, RAD_TileType_t previous_type, int32_t previous_z, const RAD_Tile_t *tile)
{
    const bool was_void = (previous_type == RAD_TILE_TYPE_VOID);
    const bool is_void = (tile->type == RAD_TILE_TYPE_VOID);

    if(was_void && is_void)
    {
        return;
    }

    if(was_void)
    {
        RAD_EventManagerPublishTileAddedToGameEvent(world->event_manager, tile);
    }
    else if(is_void)
    {
        RAD_EventManagerPublishTileRemovedFromGameEvent(world->event_manager, tile);
    }
    else if((previous_type != tile->type) || (previous_z != tile->z))
    {
        RAD_EventManagerPublishTileStateChangeEvent(world->event_manager, tile);
    }
}

RAD_UnitId_t RAD_WorldAddReserveUnit(RAD_World_t *world, const RAD_Unit_t *values, RAD_UserId_t owner)
{
    // Erst pruefen, dann schreiben: eine abgelehnte Einheit belegt keinen Slot.
    // Mindestens ein Mitglied -- eine Einheit aus einer Armee ohne Mitglieder gibt
    // es nicht (spielstart.schema.json).
    if((values == NULL) || !RAD_WorldUnitValuesFit(values, 1))
    {
        return RAD_UNIT_NONE;
    }

    // Der Pool legt an und vergibt die Id; NULL heisst, er ist voll.
    RAD_Unit_t *unit = RAD_UnitPoolAddUnit(world->units);
    if(unit == NULL)
    {
        return RAD_UNIT_NONE;
    }
    const RAD_UnitId_t id = unit->id;

    // Die Werte als Ganzes und danach das, was die Welt vergibt. So kommt aus
    // "values" nichts durch, was sie selbst fuehrt -- auch kein Zustand und keine
    // Bedingung, die der Aufrufer zufaellig gesetzt hat.
    *unit = *values;
    unit->id = id;
    unit->state = RAD_UNIT_STATE_RESERVE;
    unit->owner = owner;
    unit->x = -1;
    unit->y = -1;
    unit->conditions.burning = 0;
    unit->conditions.poisoned = 0;
    unit->turn.deployed = 0;
    unit->turn.moved = 0;
    unit->turn.attacked = 0;

    return id;
}

RAD_UnitId_t RAD_WorldSpawnUnit(RAD_World_t *world, RAD_UnitType_t type, int32_t x, int32_t y)
{
    // Erst das Feld, dann der Pool: eine abgelehnte Figur belegt keinen Platz.
    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if((tile == NULL) || (tile->unit != RAD_UNIT_NONE))
    {
        return RAD_UNIT_NONE;
    }

    RAD_Unit_t *unit = RAD_UnitPoolAddUnit(world->units);
    if(unit == NULL)
    {
        return RAD_UNIT_NONE;
    }

    // Eine neue Figur faengt herrenlos an (so legt der Pool sie an). Wer sie
    // zuordnen will, tut das danach.
    unit->type = type;
    unit->state = RAD_UNIT_STATE_DEPLOYED;
    unit->x = x;
    unit->y = y;
    tile->unit = unit->id;

    RAD_EventManagerPublishUnitSpawned(world->event_manager, unit, x, y);

    return unit->id;
}

bool RAD_WorldDeployUnit(RAD_World_t *world, RAD_UnitId_t id, int32_t x, int32_t y)
{
    RAD_Unit_t *unit = RAD_WorldUnitById(world, id);
    if((unit == NULL) || (unit->state != RAD_UNIT_STATE_RESERVE))
    {
        return false;
    }

    // Anders als RAD_WorldSpawnUnit auch nicht auf ein Feld ohne Gelaende: wer aus
    // der Reserve kommt, wird aufgestellt, und aufstellen laesst sich nur, wo
    // etwas ist. Dieselbe Haltung wie bei RAD_WorldRemoveTile -- eine Figur ueber
    // dem Nichts ist eine Regel, die es nicht gibt.
    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if((tile == NULL) || (tile->type == RAD_TILE_TYPE_VOID) || (tile->unit != RAD_UNIT_NONE))
    {
        return false;
    }

    unit->state = RAD_UNIT_STATE_DEPLOYED;
    unit->x = x;
    unit->y = y;
    tile->unit = id;

    // Fuer einen Abonnenten ist das dasselbe wie ein Spawn: eine Figur steht, wo
    // vorher keine stand. Woher sie kommt, aendert fuer ihn nichts.
    RAD_EventManagerPublishUnitSpawned(world->event_manager, unit, x, y);

    return true;
}

bool RAD_WorldMoveUnit(RAD_World_t *world, RAD_UnitId_t id, int32_t x, int32_t y)
{
    RAD_Unit_t *unit = RAD_WorldUnitById(world, id);
    if((unit == NULL) || (unit->state != RAD_UNIT_STATE_DEPLOYED))
    {
        return false;
    }

    if(unit->x == x && unit->y == y)
    {
        return true;
    }

    RAD_Tile_t *target = RAD_WorldTileAt(world, x, y);
    if(target == NULL || target->unit != RAD_UNIT_NONE)
    {
        return false;
    }

    // Woher die Figur kommt, muss vor dem Schreiben festgehalten werden: gleich
    // steht in unit->x/y das Ziel, und das Ereignis unten braucht beides.
    const int16_t from_x = unit->x;
    const int16_t from_y = unit->y;

    // Ab hier ist der Zug gueltig; erst jetzt wird geschrieben, damit ein
    // abgelehnter Zug die Welt garantiert unveraendert laesst.
    RAD_Tile_t *source = RAD_WorldTileAt(world, unit->x, unit->y);
    if(source != NULL)
    {
        source->unit = RAD_UNIT_NONE;
    }

    target->unit = id;
    unit->x = x;
    unit->y = y;

    // Ein Zug ueber ein Feld sind zwei Felder: das verlassene und das betretene.
    // Beide gehoeren in den Pfad -- ein Abonnent hat eine Figur umzuhaengen und
    // braucht dafuer beide Enden (path.h). Aus unit->x/y liesse sich das nicht
    // mehr holen: dort steht seit drei Zeilen das Ziel.
    const RAD_Path_t path = {
        .steps_to = {
            { .x = from_x,     .y = from_y     },
            { .x = (int16_t)x, .y = (int16_t)y }
        },
        .number_of_steps = 2
    };

    RAD_EventManagerPublishUnitMoved(world->event_manager, unit, &path, 0);

    return true;
}

void RAD_WorldRemoveUnit(RAD_World_t *world, RAD_UnitId_t id)
{
    RAD_Unit_t *unit = RAD_WorldUnitById(world, id);
    if((unit == NULL) || (unit->state == RAD_UNIT_STATE_DESTROYED))
    {
        return;
    }

    const bool was_deployed = (unit->state == RAD_UNIT_STATE_DEPLOYED);
    const int16_t x = unit->x;
    const int16_t y = unit->y;

    // Die Einheit bleibt im Pool, samt Besitzer und Werten -- nur das Feld wird
    // frei (world.h).
    if(was_deployed)
    {
        RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
        if(tile != NULL)
        {
            tile->unit = RAD_UNIT_NONE;
        }
    }

    unit->state = RAD_UNIT_STATE_DESTROYED;
    unit->x = -1;
    unit->y = -1;

    // Gemeldet wird nur, was auf dem Feld stand -- und mit dem Feld, auf dem es
    // stand, denn in der Einheit steht es jetzt nicht mehr. Aus der Reserve
    // verschwindet eine Einheit still: ein Abonnent hat sie nie gesehen.
    if(was_deployed)
    {
        RAD_EventManagerPublishUnitDestroyed(world->event_manager, unit, x, y);
    }
}

RAD_UserId_t RAD_WorldUnitOwner(const RAD_World_t *world, RAD_UnitId_t id)
{
    const RAD_Unit_t *unit = RAD_UnitPoolUnitById(world->units, id);
    return (unit == NULL) ? RAD_USER_NONE : unit->owner;
}

bool RAD_WorldSetUnitOwner(RAD_World_t *world, RAD_UnitId_t id, RAD_UserId_t owner)
{
    RAD_Unit_t *unit = RAD_WorldUnitById(world, id);
    if(unit == NULL)
    {
        return false;
    }

    unit->owner = owner;
    return true;
}

bool RAD_WorldIsConsistent(const RAD_World_t *world)
{
    const int32_t number_of_units = RAD_UnitPoolNumberOfUnits(world->units);
    for(int32_t i=0;i < number_of_units; ++i)
    {
        const RAD_Unit_t *unit = RAD_UnitPoolUnitAt(world->units, i);
        if(!RAD_WorldUnitValuesFit(unit, 0))
        {
            return false;
        }

        switch(unit->state)
        {
            case RAD_UNIT_STATE_DEPLOYED:
                if(!RAD_WorldInBounds(world, unit->x, unit->y))
                {
                    return false;
                }
                // Das Tile unter der Einheit muss auf sie zurueckzeigen.
                if(world->tiles[unit->y][unit->x].unit != unit->id)
                {
                    return false;
                }
                break;

            case RAD_UNIT_STATE_RESERVE:
            case RAD_UNIT_STATE_DESTROYED:
                // Nicht auf dem Feld. Dass auch kein Tile auf sie zeigt, prueft
                // die Schleife ueber die Tiles unten.
                if(unit->x != -1 || unit->y != -1)
                {
                    return false;
                }
                break;

            default:
                return false;
        }
    }

    if(world->width < 1 || world->width > RAD_WORLD_WIDTH || world->height < 1 || world->height > RAD_WORLD_HEIGHT)
    {
        return false;
    }

    for(int32_t y=0;y < world->height; ++y)
    {
        for(int32_t x=0;x < world->width; ++x)
        {
            const RAD_Tile_t *tile = &world->tiles[y][x];
            if(tile->x != x || tile->y != y)
            {
                return false;
            }
            if(tile->unit == RAD_UNIT_NONE)
            {
                continue;
            }
            // Und die Einheit muss genau hier stehen -- auf dem Feld und nicht in
            // der Reserve oder zerstoert. Eine Id, die der Pool nicht kennt, ist
            // ein Verweis ins Leere.
            const RAD_Unit_t *unit = RAD_UnitPoolUnitById(world->units, tile->unit);
            if(unit == NULL || unit->state != RAD_UNIT_STATE_DEPLOYED || unit->x != x || unit->y != y)
            {
                return false;
            }
        }
    }

    return true;
}

///
/// Ob sich die Werte einer Einheit in ihren festen Feldern halten lassen: die
/// Zaehler innerhalb der Grenzen aus unit.h und jeder Name mit seiner
/// abschliessenden Null. "min_members" ist 1 fuer eine Einheit aus einer Armee und
/// 0 fuer eine, die ohne Werte direkt auf dem Feld entsteht (RAD_WorldSpawnUnit).
///
static bool RAD_WorldUnitValuesFit(const RAD_Unit_t *unit, int32_t min_members)
{
    if((unit->number_of_members < min_members) || (unit->number_of_members > RAD_UNIT_MAX_MEMBERS))
    {
        return false;
    }
    if(!RAD_WorldNameFits(unit->name))
    {
        return false;
    }

    for(int32_t m=0;m < unit->number_of_members; ++m)
    {
        const RAD_UnitMember_t *member = &unit->members[m];
        if((member->number_of_weapons < 0) || (member->number_of_weapons > RAD_UNIT_MAX_WEAPONS))
        {
            return false;
        }
        if(!RAD_WorldNameFits(member->profile))
        {
            return false;
        }

        for(int32_t w=0;w < member->number_of_weapons; ++w)
        {
            if(!RAD_WorldNameFits(member->weapons[w].name))
            {
                return false;
            }
        }
    }

    return true;
}

static bool RAD_WorldNameFits(const char name[RAD_UNIT_NAME_MAX])
{
    return memchr(name, '\0', RAD_UNIT_NAME_MAX) != NULL;
}
