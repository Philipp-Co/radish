#ifndef __RAD_IO_NET_TYPES_H__
#define __RAD_IO_NET_TYPES_H__

#include <stdbool.h>
#include <stdint.h>

///
/// Die Typen, in denen der Client mit dem Server spricht -- eigene, kleine
/// Abbilder von protobuf/command.proto und tile.proto, keine aus game/.
///
/// **Der Client kennt das Spielmodul nicht.** Was er ueber die Welt weiss, kommt
/// ueber die Verbindung (net_codec.h) und steht in diesen Typen; die Regeln des
/// Spiels wendet der Server an. Die einzige Schnittstelle zwischen beiden ist
/// damit das .proto -- aendert sich game/, muss der Client davon nichts merken,
/// solange die Nachrichten gleich bleiben.
///

///
/// Id einer Figur, wie der Server sie schickt: -1 heisst keine
/// (RAD_NET_ENTITY_NONE), jede andere Zahl ist eine, die 0 eingeschlossen.
///
typedef int32_t RAD_NetEntityId_t;
#define RAD_NET_ENTITY_NONE ((RAD_NetEntityId_t)-1)

///
/// Uuid eines Benutzers. 0 heisst niemand.
///
typedef uint64_t RAD_NetUserId_t;

///
/// Sequenznummer eines Kommandos. Der Client vergibt sie selbst und zaehlt je
/// Kommando weiter; der Server schickt sie in der Antwort zurueck.
///
typedef uint64_t RAD_NetSequence_t;

typedef struct
{
    int16_t x;
    int16_t y;
} RAD_NetPosition_t;

///
/// Hoechstzahl der Felder eines Weges, das Startfeld eingeschlossen. Dieselbe
/// Grenze, die der Server anlegt -- ein laengerer Weg wird dort abgelehnt.
///
#define RAD_NET_PATH_MAX_STEPS 16

///
/// Ein Weg ueber das Raster. **steps_to[0] ist das Startfeld**, der Standort der
/// Figur, und kein Schritt fuer sich; steps_to[number_of_steps - 1] ist das Ziel.
/// Ein Weg braucht also mindestens zwei Felder.
///
typedef struct
{
    RAD_NetPosition_t steps_to[RAD_NET_PATH_MAX_STEPS];
    int8_t number_of_steps;
} RAD_NetPath_t;

///
/// Ein Zug, wie er hinausgeht (command.proto NetMoveCommand in einem
/// NetCommandRequest).
///
typedef struct
{
    RAD_NetSequence_t sequence;
    RAD_NetUserId_t user;
    RAD_NetEntityId_t entity;
    RAD_NetPath_t path;
} RAD_NetMoveRequest_t;

///
/// Die Antwort des Servers auf einen Zug (command.proto NetCommandResponse). Der
/// Server wiederholt darin das Kommando; "success" sagt, ob er es ausgefuehrt hat.
/// Die Beschreibung, die er mitschickt, faellt weg.
///
typedef struct
{
    RAD_NetSequence_t sequence;
    RAD_NetUserId_t user;
    bool success;
    RAD_NetEntityId_t entity;
    RAD_NetPath_t path;
} RAD_NetCommandResponse_t;

///
/// Wire-Typ eines Tiles (tile.proto NetTileType). Die Wire-Werte sind eine eigene
/// Tabelle (0 unbekannt, 1 ground, 2 water, 3 void).
///
typedef enum
{
    RAD_NET_TILE_TYPE_UNKNOWN = 0,
    RAD_NET_TILE_TYPE_GROUND,
    RAD_NET_TILE_TYPE_WATER,
    RAD_NET_TILE_TYPE_VOID
} RAD_NetTileType_t;

///
/// Wire-Abbild eines Tiles (tile.proto NetTile).
///
typedef struct RAD_NetTile RAD_NetTile_t;
struct RAD_NetTile
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
    RAD_NetTileType_t type;
    RAD_NetEntityId_t entity_id;
};

///
/// Name eines Wire-Typs fuers Log: "ground", "water", "void", "unbekannt".
/// Immer ein gueltiger Zeiger.
///
const char* RAD_NetTileTypeText(RAD_NetTileType_t type);

#endif
