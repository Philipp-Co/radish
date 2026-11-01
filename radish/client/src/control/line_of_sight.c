#include <radish/control/line_of_sight.h>
#include <stddef.h>

///
/// Gelaufen wird die Linie Feld fuer Feld, wie sie die Kanten des Rasters
/// kreuzt, in ganzen Zahlen -- genau wie RAD_GameHasLineOfSight: nach ix
/// Schritten waagerecht und iy senkrecht steht sie vor der Kante bei
/// (2*ix + 1) / (2*nx) bzw. (2*iy + 1) / (2*ny) ihrer Laenge. Welche sie zuerst
/// kreuzt, sagt der Vergleich ueber Kreuz; sind beide gleich, geht sie durch
/// die Ecke.
///
bool RAD_ControlLineOfSight(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y,
                            RAD_ControlBlocksSight_t blocks_sight, void *user_argument)
{
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
            if(blocks_sight(user_argument, x + sx, y) && blocks_sight(user_argument, x, y + sy))
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
        if(((x != to_x) || (y != to_y)) && blocks_sight(user_argument, x, y))
        {
            return false;
        }
    }

    return true;
}

/// Gelaende oder Einheit auf (x, y) in der Welt "user_argument"; unbekannt ist frei.
static bool RAD_ControlWorldBlocksSight(void *user_argument, int32_t x, int32_t y)
{
    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt((const RAD_ClientWorld_t *)user_argument, x, y);
    return (tile != NULL) && (tile->terrain.blocks_sight || RAD_ClientTileHasUnit(tile));
}

bool RAD_ControlHasLineOfSight(const RAD_ClientWorld_t *world, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y)
{
    if(world == NULL)
    {
        return false;
    }
    // Die Welt wird nur gelesen; die Rueckfrage nimmt ein void*, wie jede.
    return RAD_ControlLineOfSight(from_x, from_y, to_x, to_y, RAD_ControlWorldBlocksSight, (void *)world);
}
