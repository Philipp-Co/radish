#include <radish/game/model/game.h>
#include <stddef.h>

///
/// Die Schreibseite der Tiles: das Gegenstueck zu view.c.
///
/// view.c beantwortet, was auf dem Feld steht -- diese Datei aendert es. Getrennt,
/// weil es zwei verschiedene Rechte sind: lesen darf jeder von aussen (tile.h),
/// schreiben nur, wer innerhalb von radish_game uebersetzt (model/game.h). Und
/// nicht in game.c, weil dort der Lebenszyklus eines Spiels und die Verteilung der
/// Kommandos stehen.
///
/// Beide Funktionen sind duenn und sollen es sein: die Regeln stehen in der Welt
/// (world.h), hier steht die Fassade davor.
///

bool RAD_GameAddTile(RAD_Game_t *game, int32_t x, int32_t y, int32_t z, RAD_TileType_t type)
{
    if(game == NULL)
    {
        return false;
    }

    return RAD_WorldAddTile(&game->world, x, y, z, type);
}

bool RAD_GameRemoveTile(RAD_Game_t *game, int32_t x, int32_t y)
{
    if(game == NULL)
    {
        return false;
    }

    return RAD_WorldRemoveTile(&game->world, x, y);
}
