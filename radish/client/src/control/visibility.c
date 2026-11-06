#include <radish/control/visibility.h>
#include <radish/control/line_of_sight.h>
#include <stddef.h>
#include <string.h>

/// Felder waagerecht plus senkrecht -- wie RAD_GameDistance im Spiel.
static int32_t RAD_ControlDistance(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y)
{
    const int32_t dx = to_x - from_x;
    const int32_t dy = to_y - from_y;
    return ((dx < 0) ? -dx : dx) + ((dy < 0) ? -dy : dy);
}

/// Sieht eine Einheit auf (from_x, from_y), die "range" Felder weit sieht, das Feld (x, y)?
static bool RAD_ControlSees(const RAD_ClientWorld_t *world, int32_t from_x, int32_t from_y, int32_t range,
                            int32_t x, int32_t y)
{
    // Erst die Entfernung, dann die Linie: die ist der teure Teil.
    return (RAD_ControlDistance(from_x, from_y, x, y) <= range)
        && RAD_ControlHasLineOfSight(world, from_x, from_y, x, y);
}

/// Sieht die Einheit auf "from" das Feld (x, y)?
static bool RAD_ControlUnitSees(const RAD_ClientWorld_t *world, const RAD_ClientTile_t *from, int32_t x, int32_t y)
{
    return RAD_ControlSees(world, from->x, from->y, from->unit->sight_range, x, y);
}

/// Steht auf "tile" eine Einheit von "player"?
static bool RAD_ControlIsOwnUnit(const RAD_ClientTile_t *tile, RAD_ClientPlayerId_t player)
{
    return (tile != NULL) && (tile->unit != NULL) && (tile->unit->owner == player);
}

bool RAD_ControlTileVisible(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t player, int32_t x, int32_t y)
{
    if((world == NULL) || (player == RAD_CLIENT_PLAYER_NONE) || (RAD_ClientWorldTileAt(world, x, y) == NULL))
    {
        return false;
    }

    for(int32_t uy = 0; uy < (int32_t)world->height; ++uy)
    {
        for(int32_t ux = 0; ux < (int32_t)world->width; ++ux)
        {
            const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(world, ux, uy);
            if(RAD_ControlIsOwnUnit(tile, player) && RAD_ControlUnitSees(world, tile, x, y))
            {
                return true;
            }
        }
    }
    return false;
}

void RAD_ControlVisibility(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t player,
                           const RAD_ControlUnitPosition_t *positions, size_t number_of_positions,
                           bool visible[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE])
{
    memset(visible, 0, sizeof(bool) * RAD_ISO_MAP_SIZE * RAD_ISO_MAP_SIZE);
    if(world == NULL)
    {
        return;
    }

    const int32_t width = ((int32_t)world->width < RAD_ISO_MAP_SIZE) ? (int32_t)world->width : RAD_ISO_MAP_SIZE;
    const int32_t height = ((int32_t)world->height < RAD_ISO_MAP_SIZE) ? (int32_t)world->height : RAD_ISO_MAP_SIZE;

    if(player == RAD_CLIENT_PLAYER_NONE)
    {
        for(int32_t y = 0; y < height; ++y)
        {
            for(int32_t x = 0; x < width; ++x)
            {
                visible[y][x] = true;
            }
        }
        return;
    }

    // Von jeder eigenen Einheit aus nur das Quadrat um sie, so weit sie sieht --
    // was darueber hinausliegt, sieht sie ohnehin nicht.
    for(int32_t uy = 0; uy < height; ++uy)
    {
        for(int32_t ux = 0; ux < width; ++ux)
        {
            const RAD_ClientTile_t *from = RAD_ClientWorldTileAt(world, ux, uy);
            if(!RAD_ControlIsOwnUnit(from, player))
            {
                continue;
            }

            // Faehrt sie gerade, von dort, wo sie zu sehen ist.
            int32_t origin_x = ux;
            int32_t origin_y = uy;
            for(size_t p = 0; (positions != NULL) && (p < number_of_positions); ++p)
            {
                if((positions[p].unit == from->unit->id)
                   && (positions[p].x >= 0) && (positions[p].x < width)
                   && (positions[p].y >= 0) && (positions[p].y < height))
                {
                    origin_x = positions[p].x;
                    origin_y = positions[p].y;
                    break;
                }
            }

            const int32_t range = from->unit->sight_range;
            const int32_t min_x = (origin_x - range < 0) ? 0 : origin_x - range;
            const int32_t max_x = (origin_x + range >= width) ? width - 1 : origin_x + range;
            const int32_t min_y = (origin_y - range < 0) ? 0 : origin_y - range;
            const int32_t max_y = (origin_y + range >= height) ? height - 1 : origin_y + range;
            for(int32_t y = min_y; y <= max_y; ++y)
            {
                for(int32_t x = min_x; x <= max_x; ++x)
                {
                    if(!visible[y][x] && RAD_ControlSees(world, origin_x, origin_y, range, x, y))
                    {
                        visible[y][x] = true;
                    }
                }
            }
        }
    }
}
