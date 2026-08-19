#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <stddef.h>

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
    // Groesse des Rasters und keine Zaehlung.
    return (int32_t)(RAD_WORLD_WIDTH * RAD_WORLD_HEIGHT);
}

bool RAD_GameTileAt(const RAD_Game_t *game, int32_t index, RAD_Tile_t *output)
{
    if((game == NULL) || (output == NULL))
    {
        return false;
    }

    if((index < 0) || (index >= RAD_GameNumberOfTiles(game)))
    {
        return false;
    }

    // Zeilenweise, Zeile 0 zuerst -- dieselbe Reihenfolge wie im Serializer.
    const int32_t y = index / RAD_WORLD_WIDTH;
    const int32_t x = index % RAD_WORLD_WIDTH;

    *output = game->world.tiles[y][x];
    return true;
}

int32_t RAD_GameNumberOfEntities(const RAD_Game_t *game)
{
    if(game == NULL)
    {
        return 0;
    }

    // Der mitgefuehrte Zaehler und keine eigene Schleife: die Welt haelt ihn bei
    // jedem Setzen und Entfernen fort, und RAD_WorldIsConsistent prueft ihn gegen
    // die belegten Plaetze. Ihn hier nachzuzaehlen waere ein zweites Buch.
    return game->world.number_of_entities;
}

bool RAD_GameEntityAt(const RAD_Game_t *game, int32_t index, RAD_Entity_t *output)
{
    if((game == NULL) || (output == NULL))
    {
        return false;
    }

    if((index < 0) || (index >= RAD_GameNumberOfEntities(game)))
    {
        return false;
    }

    // Der Pool hat Luecken, der Index ist dicht: durchlaufen und die belegten
    // Plaetze zaehlen, bis der gesuchte erreicht ist. Dasselbe Verfahren wie
    // RAD_GameUserEntityAt in player.c, und aus demselben Grund -- bei hoechstens
    // RAD_MAX_ENTITIES Plaetzen ist jede Beschleunigung teurer als die Suche.
    int32_t seen = 0;
    for(RAD_EntityId_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        const RAD_Entity_t *entity = &game->world.entities[i];
        if(entity->id == RAD_ENTITY_NONE)
        {
            continue;
        }

        if(seen == index)
        {
            *output = *entity;
            return true;
        }
        seen++;
    }

    // Nicht zu erreichen, solange "number_of_entities" mit den belegten Plaetzen
    // uebereinstimmt -- der Index wurde oben dagegen geprueft. Laeuft der Zaehler
    // dennoch vor, ist false die ehrliche Antwort und kein halb gefuelltes output.
    return false;
}
