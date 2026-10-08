#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <stddef.h>
#include <assert.h>

///
/// Die Leseseite des Spiels: was von aussen aus der Welt herauszusehen ist.
///
/// Sie steht neben player.c und nicht darin, weil es die andere Haelfte derselben
/// Fassade ist: player.c beantwortet, wer mitspielt und wem was gehoert -- diese
/// Datei, was auf dem Feld steht. Beides fuehrt nichts selbst, beides legt nur
/// offen, was die Welt (world.h) und der Zug (turn.h) schon wissen.
///
/// **Herausgegeben wird kopiert, nie verwiesen.** Jede Funktion hier schreibt
/// einen Stand in den Speicher des Aufrufers. Gaebe sie einen Zeiger in die Welt
/// heraus, waere die Kapselung wieder offen -- ein Aufrufer koennte an den Regeln
/// vorbei hineinschreiben, und sein Zeiger wuerde baumeln, sobald sich die Welt
/// weiterdreht. Die Begruendung im Langen steht in game.h.
///
/// Die Reihenfolge der Pruefungen ist in allen vier Funktionen dieselbe: erst der
/// Zeiger, dann der Index. Erst danach wird geschrieben, damit ein abgelehnter
/// Aufruf "output" garantiert unberuehrt laesst.
///

int32_t RAD_GameNumberOfTiles(const RAD_Game_t *game)
{
    if(game == NULL)
    {
        return 0;
    }

    // Das Raster ist vollstaendig besetzt -- es gibt kein "leeres" Tile, ein Feld
    // ohne Inhalt ist RAD_TILE_TYPE_VOID und zaehlt mit. Die Anzahl ist damit die
    // Groesse der Welt und keine Zaehlung.
    return game->world.width * game->world.height;
}

int32_t RAD_GameWorldWidth(const RAD_Game_t *game)
{
    return (game == NULL) ? 0 : game->world.width;
}

int32_t RAD_GameWorldHeight(const RAD_Game_t *game)
{
    return (game == NULL) ? 0 : game->world.height;
}

bool RAD_GameTileAt(const RAD_Game_t *game, int16_t x, int16_t y, RAD_Tile_t *output)
{
    if((game == NULL) || (output == NULL))
    {
        return false;
    }

    // Erst nach dem Zeiger: die Grenze steht in der Welt, nicht mehr in den
    // Konstanten.
    assert((x >= 0) && (x < game->world.width));
    assert((y >= 0) && (y < game->world.height));

    // Zeilenweise, Zeile 0 zuerst -- dieselbe Reihenfolge wie in der Weltdefinition.
    *output = game->world.tiles[y][x];
    return true;
}

int32_t RAD_GameNumberOfUnits(const RAD_Game_t *game)
{
    if(game == NULL)
    {
        return 0;
    }

    // Der Zaehler des Pools und keine eigene Schleife: ihn hier nachzuzaehlen
    // waere ein zweites Buch.
    return RAD_UnitPoolNumberOfUnits(game->unit_pool);
}

bool RAD_GameUnitAt(const RAD_Game_t *game, int32_t index, RAD_Unit_t *output)
{
    if((game == NULL) || (output == NULL))
    {
        return false;
    }

    if((index < 0) || (index >= RAD_GameNumberOfUnits(game)))
    {
        return false;
    }

    // Dicht ueber die belegten Plaetze, in der Reihenfolge des Pools -- solange
    // nichts aus ihm entfernt wird, und das tut das Spiel nie, ist das die
    // Reihenfolge der Ids (unit_pool.h).
    const RAD_Unit_t *unit = RAD_UnitPoolUnitAt(game->unit_pool, index);
    if(unit == NULL)
    {
        return false;
    }
    *output = *unit;
    return true;
}
