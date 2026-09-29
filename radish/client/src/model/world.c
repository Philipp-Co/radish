#include <radish/model/world.h>

#include <stddef.h>
#include <string.h>

static bool RAD_ClientWorldInBounds(int32_t x, int32_t y)
{
    return (x >= 0) && (x < RAD_ISO_MAP_SIZE) && (y >= 0) && (y < RAD_ISO_MAP_SIZE);
}

void RAD_ClientWorldInit(RAD_ClientWorld_t *world)
{
    memset(world, 0, sizeof(*world));
}

void RAD_ClientWorldSetSize(RAD_ClientWorld_t *world, uint32_t width, uint32_t height)
{
    world->width = width;
    world->height = height;
}

void RAD_ClientWorldApplyTile(RAD_ClientWorld_t *world, const RAD_NetTile_t *tile)
{
    if(!RAD_ClientWorldInBounds((int32_t)tile->x, (int32_t)tile->y))
    {
        return;
    }

    world->tiles[tile->y][tile->x] = *tile;
    world->known[tile->y][tile->x] = true;
}

void RAD_ClientWorldRemoveTile(RAD_ClientWorld_t *world, uint32_t x, uint32_t y)
{
    if(!RAD_ClientWorldInBounds((int32_t)x, (int32_t)y))
    {
        return;
    }

    world->known[y][x] = false;
}

bool RAD_ClientWorldTileAt(const RAD_ClientWorld_t *world, int32_t x, int32_t y, RAD_NetTile_t *output)
{
    if(!RAD_ClientWorldInBounds(x, y) || !world->known[y][x])
    {
        return false;
    }

    *output = world->tiles[y][x];
    return true;
}

void RAD_ClientWorldMoveEntity(RAD_ClientWorld_t *world, RAD_NetEntityId_t entity, const RAD_NetPath_t *path)
{
    // Unter zwei Feldern ist kein Weg beschrieben (net_types.h).
    if((path == NULL) || (path->number_of_steps < 2))
    {
        return;
    }

    const RAD_NetPosition_t from = path->steps_to[0];
    const RAD_NetPosition_t to = path->steps_to[path->number_of_steps - 1];

    if(!RAD_ClientWorldInBounds(from.x, from.y) || !RAD_ClientWorldInBounds(to.x, to.y))
    {
        return;
    }

    if(world->tiles[from.y][from.x].entity_id != entity)
    {
        return;
    }

    world->tiles[from.y][from.x].entity_id = RAD_NET_ENTITY_NONE;
    world->tiles[to.y][to.x].entity_id = entity;
}
