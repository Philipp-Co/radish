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
/// Ein Deployment, wie es hinausgeht (command.proto NetDeployCommand in einem
/// NetCommandRequest): die Einheit "entity" aus der Reserve des Absenders auf
/// das Feld "position", in Weltkoordinaten.
///
typedef struct
{
    RAD_NetSequence_t sequence;
    RAD_NetUserId_t user;
    RAD_NetEntityId_t entity;
    RAD_NetPosition_t position;
} RAD_NetDeployRequest_t;

///
/// Ein Angriff, wie er hinausgeht (command.proto NetAttackCommand in einem
/// NetCommandRequest): die Einheit "entity" des Absenders greift das Feld
/// "target" an, in Weltkoordinaten. Auch ein leeres Feld ist ein Ziel.
///
typedef struct
{
    RAD_NetSequence_t sequence;
    RAD_NetUserId_t user;
    RAD_NetEntityId_t entity;
    RAD_NetPosition_t target;
} RAD_NetAttackRequest_t;

///
/// Das Abgeben des Zuges, wie es hinausgeht (command.proto NetEndTurnCommand in
/// einem NetCommandRequest). Ohne Nutzlast: wer abgibt, ist der Absender.
///
typedef struct
{
    RAD_NetSequence_t sequence;
    RAD_NetUserId_t user;
} RAD_NetEndTurnRequest_t;

///
/// Platz fuer den Namen einer Einheit samt abschliessender Null -- derselbe wie im
/// Spiel (RAD_UNIT_NAME_MAX). Ein laengerer Name aus der Nachricht wird gekuerzt:
/// der Client zeigt ihn nur an.
///
#define RAD_NET_UNIT_NAME_MAX 32

///
/// Hoechstzahl der Mitglieder einer Einheit und der Waffen eines Mitglieds --
/// dieselben wie im Spiel (RAD_UNIT_MAX_MEMBERS, RAD_UNIT_MAX_WEAPONS). Eine
/// Nachricht mit mehr liest der Codec nicht (net_codec.h).
///
#define RAD_NET_UNIT_MEMBERS_MAX 10
#define RAD_NET_MEMBER_WEAPONS_MAX 4

///
/// Eine Waffe eines Mitglieds (game.proto NetWeapon). "weapon_class" wie im
/// Spiel: 0 standard, 1 heavy, 2 super heavy.
///
typedef struct
{
    char name[RAD_NET_UNIT_NAME_MAX];
    uint32_t weapon_class;
    uint32_t shots;
    uint32_t strength;
    uint32_t min_range;
    uint32_t max_range;
    uint32_t penetration;
} RAD_NetWeapon_t;

///
/// Ein Mitglied einer Einheit (game.proto NetUnitMember).
///
typedef struct
{
    char profile[RAD_NET_UNIT_NAME_MAX];
    uint32_t health;
    uint32_t armor;
    uint32_t strength;
    uint32_t accuracy;
    RAD_NetWeapon_t weapons[RAD_NET_MEMBER_WEAPONS_MAX];
    uint32_t number_of_weapons;
} RAD_NetUnitMember_t;

///
/// Eine Einheit, vollstaendig (game.proto NetUnit): wer sie ist, wem sie
/// gehoert -- "owner" ist die oeffentliche Spieler-Id --, ihre Werte und alle
/// Mitglieder mit ihren Waffen. Wo sie steht, sagt sie nicht.
///
/// So kommt sie als Antwort auf eine Einheiten-Anfrage (NetUnitEvent), und so
/// steckt sie in RAD_NetReserveUnit_t und RAD_NetUnitDeployed_t.
/// "number_of_members" ist die Zahl der Mitglieder in "members" -- nicht das
/// gleichnamige Feld der Nachricht, das daneben ueberzaehlig ist.
///
typedef struct
{
    RAD_NetEntityId_t unit;
    RAD_NetUserId_t owner;
    char name[RAD_NET_UNIT_NAME_MAX];
    uint32_t movement;
    uint32_t transport_capacity;
    bool can_capture;
    RAD_NetUnitMember_t members[RAD_NET_UNIT_MEMBERS_MAX];
    uint32_t number_of_members;

    /// Was sie im laufenden Zug schon getan hat (NetUnit.deployed, moved,
    /// attacked): aufgestellt, gezogen, angegriffen.
    bool deployed;
    bool moved;
    bool attacked;
} RAD_NetUnit_t;

///
/// Eine Einheit in der Reserve (game.proto NetReserveUnitEvent, eine Nachricht je
/// Einheit): dem Spiel bekannt, auf dem Feld noch nicht. Die Nachricht ist nur
/// die Einheit selbst.
///
typedef RAD_NetUnit_t RAD_NetReserveUnit_t;

///
/// Eine Einheit, die jetzt auf dem Feld "position" steht (game.proto
/// NetUnitDeployedEvent). Der Server schickt es jedem Client, sobald eine
/// Einheit aufgestellt wurde, noch vor der Antwort auf das Kommando -- und mit
/// der ganzen Einheit, damit auch ein Client sie aufstellen kann, der die Reserve
/// nicht kennt.
///
typedef struct
{
    RAD_NetUnit_t unit;
    RAD_NetPosition_t position;
} RAD_NetUnitDeployed_t;

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
/// Die Antwort des Servers auf ein Deployment (command.proto NetCommandResponse
/// mit NetDeployCommand), wie RAD_NetCommandResponse_t fuer einen Zug: der
/// Server wiederholt darin das Kommando, "success" sagt, ob er die Einheit
/// "entity" auf "position" gestellt hat. Er schickt sie jedem Client, nicht nur
/// dem, der deployt hat -- "user" nennt den.
///
typedef struct
{
    RAD_NetSequence_t sequence;
    RAD_NetUserId_t user;
    bool success;
    RAD_NetEntityId_t entity;
    RAD_NetPosition_t position;
} RAD_NetDeployResponse_t;

///
/// Platz fuer die Beschreibung in einer Antwort (command.proto
/// NetCommandResponse.description) samt abschliessender Null. Eine laengere
/// wird gekuerzt -- sie ist nur zum Anzeigen.
///
#define RAD_NET_DESCRIPTION_MAX 96

///
/// Die Antwort des Servers auf einen Angriff (command.proto NetCommandResponse
/// mit NetAttackCommand), wie RAD_NetDeployResponse_t: an jeden Client, "user"
/// nennt den, der angegriffen hat; "success" sagt, ob der Server den Angriff
/// angenommen hat, "description" sonst den Grund.
///
typedef struct
{
    RAD_NetSequence_t sequence;
    RAD_NetUserId_t user;
    bool success;
    RAD_NetEntityId_t entity;
    RAD_NetPosition_t target;
    char description[RAD_NET_DESCRIPTION_MAX];
} RAD_NetAttackResponse_t;

///
/// Die Antwort des Servers auf ein Abgeben (command.proto NetCommandResponse
/// mit NetEndTurnCommand), wie RAD_NetDeployResponse_t: an jeden Client, "user"
/// nennt den, der abgegeben hat. Wer danach dran ist, steht nicht darin -- das
/// kommt vorher als eigenes Ereignis (game.proto NetCurrentPlayerEvent).
///
typedef struct
{
    RAD_NetSequence_t sequence;
    RAD_NetUserId_t user;
    bool success;
} RAD_NetEndTurnResponse_t;

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
