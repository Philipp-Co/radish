#ifndef __RAD_COMMAND_H__
#define __RAD_COMMAND_H__

#include <stdint.h>
#include <radish/game/entity.h>
#include <radish/game/tile.h>

///
/// control/command/ -- ein Kommando ist ein Anlass in Datenform: die Absicht,
/// den Spielzustand zu aendern, ohne ihn schon zu aendern. Wer ein Kommando
/// erzeugt (Eingabe im Client, eine Nachricht vom Server), muss die Spiellogik
/// nicht kennen; wer es ausfuehrt, muss nicht wissen, woher es kam. Umgesetzt
/// wird es ueber die Schnittstellen von game/ -- die Stelle, die das tut, gibt es
/// noch nicht.
///
/// Weil ein Kommando nur Daten sind, laesst es sich puffern, verschicken,
/// wiederholen und in der Reihenfolge pruefen -- dafuer traegt jedes seine
/// Sequenznummer.
///
/// Diese Datei sind nur die Typen und zieht keine Implementierung nach sich. Die
/// Uebersetzung auf die Strecke liegt daneben: codec.h beschreibt das Format und
/// den Kopf, dazu je eine Datei pro Kommandoart fuer ihre Nutzlast. Beides gehoert
/// zu radish_game.
///

///
/// Fortlaufende Nummer eines Kommandos, vom Erzeuger vergeben. Sie ordnet die
/// Kommandos einer Quelle und macht Luecken und Wiederholungen erkennbar --
/// beides braucht es, sobald Kommandos ueber eine Verbindung laufen, die
/// umsortieren oder doppelt zustellen kann.
///
/// Vorzeichenlos, weil eine Sequenz nur waechst; RAD_COMMAND_SEQUENCE_NONE
/// markiert "noch nicht vergeben", die erste echte Nummer ist also 1.
///
typedef uint64_t RAD_CommandSequence_t;


typedef enum
{
    RAD_COMMAND_TYPE_NONE = 0,
    RAD_COMMAND_TYPE_SPAWN_ENTITY,
    RAD_COMMAND_TYPE_MOVE_ENTITY,
    RAD_COMMAND_TYPE_REMOVE_ENTITY,
    RAD_COMMAND_TYPE_CREATE_TILE,
    RAD_COMMAND_TYPE_REMOVE_TILE
} RAD_CommandType_t;

///
/// Gemeinsamer Kopf jedes Kommandos. Er liegt in RAD_Command_t vor der Union und
/// nicht darin: so tragen Kopf und Nutzlast gleichzeitig, und "type" darf immer
/// gelesen werden, gleich welche Variante zuletzt geschrieben wurde. Darauf
/// beruht das Muster "erst type lesen, dann die passende Variante", so wie es
/// SDL_Event vormacht.
///
/// Die acht Byte, die der Kopf dadurch kostet statt sich mit der Nutzlast zu
/// ueberlappen, sind genau das, was den Umgang mit einem Kommando unfaellig
/// macht: eine Variante zu fuellen kann den Kopf nicht loeschen.
///
typedef struct
{
    RAD_CommandType_t type;
    RAD_CommandSequence_t sequence;
} RAD_CommandHeader_t;

///
/// Setzt eine neue Entitaet auf ein Tile. Ohne Id: die vergibt die Welt beim
/// Ausfuehren (RAD_WorldSpawnEntity).
///
typedef struct
{
    RAD_EntityType_t entity_type;
    int16_t x;
    int16_t y;
    int8_t z;
} RAD_CommandSpawnEntity_t;

///
/// Bewegt eine vorhandene Entitaet auf ein Tile. x/y sind das Ziel in
/// Weltkoordinaten, nicht die Verschiebung -- ein wiederholt zugestelltes
/// Kommando fuehrt so zum selben Ergebnis wie ein einmal zugestelltes.
///
typedef struct
{
    RAD_EntityId_t entity;
    int16_t from_x;
    int16_t from_y;
    int16_t to_x;
    int16_t to_y;
} RAD_CommandMoveEntity_t;

///
/// Nimmt eine Entitaet aus der Welt.
///
typedef struct
{
    RAD_EntityId_t entity;
} RAD_CommandRemoveEntity_t;

///
/// Legt Gelaende auf einem Tile an. "Anlegen" heisst hier nicht, dass ein Tile
/// entsteht -- das Raster der Welt steht fest, an jedem (x,y) innerhalb der
/// Grenzen liegt immer ein RAD_Tile_t. Gemeint ist sein Gelaende: aus
/// RAD_TILE_TYPE_VOID -- der Zustand "hier ist nichts" -- wird begehbarer oder
/// bespielbarer Grund.
///
/// Damit ist das Kommando zugleich das zum Aendern von Gelaende: es setzt den
/// Typ, gleich was vorher dastand. Ein eigenes Change-Kommando gaebe es nicht
/// mehr her, und Anlegen und Aendern getrennt zu fuehren hiesse, dass der
/// Erzeuger den bisherigen Zustand kennen muesste.
///
/// "z" ist die Hoehe des Tiles; sie geht in die isometrische Darstellung ein
/// (RAD_MapAddIsoObject). 0 ist die Grundebene.
///
typedef struct
{
    RAD_TileType_t tile_type;
    int16_t x;
    int16_t y;
    int8_t z;
} RAD_CommandCreateTile_t;

///
/// Nimmt das Gelaende von einem Tile: es faellt auf RAD_TILE_TYPE_VOID zurueck.
/// Das Tile selbst bleibt im Raster -- entfernt wird, was darauf liegt, nicht
/// der Platz.
///
/// Kein Tile-Typ als Feld: es gibt nur einen Weg, nichts zu sein.
///
typedef struct
{
    int16_t x;
    int16_t y;
} RAD_CommandRemoveTile_t;

typedef struct
{
    RAD_CommandHeader_t header;
    union 
    {
        RAD_CommandSpawnEntity_t spawn_entity;
        RAD_CommandMoveEntity_t move_entity;
        RAD_CommandRemoveEntity_t remove_entity;
        RAD_CommandCreateTile_t create_tile;
        RAD_CommandRemoveTile_t remove_tile;
    } command;
} RAD_Command_t;

typedef struct
{
    RAD_CommandHeader_t header;
    uint32_t value;
} RAD_CommandResponse_t;

#endif
