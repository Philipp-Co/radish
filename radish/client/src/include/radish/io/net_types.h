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

/// Kein Spieler -- solange der Client seine eigene Id nicht kennt.
#define RAD_NET_USER_NONE ((RAD_NetUserId_t)0)

///
/// Die oeffentliche Spieler-Id zur Kennung (Player.identifier im Backend): die
/// acht Zeichen als Big-Endian-Zahl, "aB3xK9pQ" -> 0x614233784B397051. Dieselbe
/// Rechnung wie RAD_ControlUserIdFromIdentifier im Spielserver
/// (game-server-core/src/include/radish/server/control/game_start.h) und
/// access.player_id im Backend -- nur so erkennt der Client sich selbst in dem,
/// was der Server schickt.
///
/// RAD_NET_USER_NONE fuer alles, was nicht genau acht Zeichen aus [A-Za-z0-9] ist.
///
static inline RAD_NetUserId_t RAD_NetUserIdFromIdentifier(const char *identifier)
{
    if(identifier == 0)
    {
        return RAD_NET_USER_NONE;
    }

    RAD_NetUserId_t user = 0;
    for(int i = 0; i < 8; ++i)
    {
        const char c = identifier[i];
        const int valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if(!valid)
        {
            return RAD_NET_USER_NONE;
        }
        user = (user << 8) | (RAD_NetUserId_t)(unsigned char)c;
    }
    return (identifier[8] == '\0') ? user : RAD_NET_USER_NONE;
}

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
/// Platz fuer den Namen einer Einheit samt abschliessender Null -- derselbe wie im
/// Spiel (RAD_UNIT_NAME_MAX). Ein laengerer Name aus der Nachricht wird gekuerzt:
/// der Client zeigt ihn nur an.
///
#define RAD_NET_UNIT_NAME_MAX 32

///
/// Eine Einheit in der Reserve (game.proto NetReserveUnitEvent, eine Nachricht je
/// Einheit): dem Spiel bekannt, auf dem Feld noch nicht. "owner" ist die
/// oeffentliche Spieler-Id ihres Besitzers.
///
typedef struct
{
    RAD_NetEntityId_t unit;
    RAD_NetUserId_t owner;
    char name[RAD_NET_UNIT_NAME_MAX];
    uint32_t number_of_members;
} RAD_NetReserveUnit_t;

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
