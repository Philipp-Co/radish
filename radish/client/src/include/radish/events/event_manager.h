#ifndef __RAD_EVENTS_EVENT_MANAGER_H__
#define __RAD_EVENTS_EVENT_MANAGER_H__

#include <stddef.h>
#include <stdint.h>
#include <radish/io/net_types.h>

///
/// events/ -- was vom Server hereinkommt. Ein Abonnent je Gruppe, die Gruppe
/// wird als Ganzes gesetzt (Subscribe ersetzt sie komplett), und die Callbacks
/// sind bei Anlage No-Ops -- deshalb prueft keine Publish-Funktion auf NULL, und
/// ein Abonnent muss alle Zeiger seiner Gruppe fuellen.
///
/// **Drei Gruppen, entlang der oneof-Verzweigungen der Nachricht vom Server**
/// (protobuf/message.proto, event.proto, game.proto, tile.proto): eine
/// NetServerMessage ist entweder eine Antwort auf ein eigenes Kommando
/// (command_response) oder ein Ereignis (event), und ein Ereignis wiederum
/// entweder ein Spiel- oder ein Tile-Ereignis mit je eigenen Varianten. Das
/// Entpacken selbst -- protobuf-c, die generierten NetXxx-Typen -- bleibt
/// vollstaendig in net_codec.c (RAD_NetDispatchServerMessage); hier stehen nur
/// die Domaenentypen, die dabei herauskommen, und die Verteilung an Abonnenten.
///

///
/// Antwort auf ein eigenes Kommando (io/net_types.h).
///
typedef void (*RAD_OnNetCommandResponse_t)(void *user_argument, const RAD_NetCommandResponse_t *response);

typedef struct
{
    void *user_argument;
    RAD_OnNetCommandResponse_t received;
} RAD_NetEventsCommandResponseCallback_t;


///
/// Spielereignisse (event.proto NetGameEvent -> game.proto). created und
/// finished tragen nichts (NetGameCreatedEvent/NetGameFinishedEvent haben nur ein
/// bedeutungsloses "reserved"), ihre Callbacks bekommen also nur den Abonnenten
/// selbst.
///
/// Die vier uebrigen schickt der Server auf eine Discover-Anfrage:
///
///   current_player  wer dran ist, 0 wenn niemand
///   players         wer mitspielt, in Zugreihenfolge -- "user_ids" gilt nur,
///                   solange der Callback laeuft, und ist bei
///                   "number_of_users" == 0 nicht zu lesen
///   world_size      Breite und Hoehe der Welt in Feldern
///   tiles           Felder des angefragten Ausschnitts mit ihren Attributen,
///                   als Stand und nicht als Aenderung. Der Server schickt je
///                   Feld eine Nachricht, "number_of_tiles" ist dann 1 -- darauf
///                   verlassen soll sich niemand. "tiles" gilt wie "user_ids" nur,
///                   solange der Callback laeuft.
///
/// Die Uuids kommen als uint64 an, ungekuerzt.
///
typedef void (*RAD_OnNetGameCreated_t)(void *user_argument);
typedef void (*RAD_OnNetGameFinished_t)(void *user_argument);
typedef void (*RAD_OnNetCurrentPlayer_t)(void *user_argument, uint64_t user_id);
typedef void (*RAD_OnNetPlayers_t)(void *user_argument, const uint64_t *user_ids, size_t number_of_users);
typedef void (*RAD_OnNetWorldSize_t)(void *user_argument, uint32_t width, uint32_t height);

// RAD_NetTile_t steht in io/net_types.h.
typedef void (*RAD_OnNetTiles_t)(void *user_argument, const RAD_NetTile_t *tiles, size_t number_of_tiles);

// Eine Einheit der Reserve, als Antwort auf eine Reserve-Anfrage -- je Einheit ein
// Aufruf, ein Ende der Liste wird nicht gemeldet (protobuf/discover.proto). Weil
// zucchini an alle schickt, kommen auch die Antworten auf fremde Anfragen hier an.
typedef void (*RAD_OnNetReserveUnit_t)(void *user_argument, const RAD_NetReserveUnit_t *unit);

typedef struct
{
    void *user_argument;
    RAD_OnNetGameCreated_t created;
    RAD_OnNetGameFinished_t finished;
    RAD_OnNetCurrentPlayer_t current_player;
    RAD_OnNetPlayers_t players;
    RAD_OnNetWorldSize_t world_size;
    RAD_OnNetTiles_t tiles;
    RAD_OnNetReserveUnit_t reserve_unit;
} RAD_NetEventsGameCallback_t;


///
/// Tile-Ereignisse (event.proto NetTileEvent -> tile.proto). "removed" traegt
/// nur x/y (NetTileRemovedEvent hat kein NetTile, anders als created/changed).
///
typedef void (*RAD_OnNetTileCreated_t)(void *user_argument, const RAD_NetTile_t *tile);
typedef void (*RAD_OnNetTileRemoved_t)(void *user_argument, uint32_t x, uint32_t y);
typedef void (*RAD_OnNetTileChanged_t)(void *user_argument, const RAD_NetTile_t *tile);

typedef struct
{
    void *user_argument;
    RAD_OnNetTileCreated_t created;
    RAD_OnNetTileRemoved_t removed;
    RAD_OnNetTileChanged_t changed;
} RAD_NetEventsTileCallback_t;


///
/// Der Manager selbst: nur ein Name. Was er haelt -- je eine Gruppe von oben --,
/// steht in event_manager.c.
///
typedef struct RAD_NetEventManager RAD_NetEventManager_t;

///
/// Legt einen Manager an, mit Callbacks, die nichts tun; NULL, wenn kein
/// Speicher da ist. RAD_DestroyNetEventManager nullt den Zeiger des Aufrufers.
///
RAD_NetEventManager_t* RAD_CreateNetEventManager(void);
void RAD_DestroyNetEventManager(RAD_NetEventManager_t **manager);

void RAD_NetEventManagerSubscribeToCommandResponseEvents(RAD_NetEventManager_t *manager, RAD_NetEventsCommandResponseCallback_t callbacks);
void RAD_NetEventManagerPublishCommandResponse(RAD_NetEventManager_t *manager, const RAD_NetCommandResponse_t *response);

void RAD_NetEventManagerSubscribeToGameEvents(RAD_NetEventManager_t *manager, RAD_NetEventsGameCallback_t callbacks);
void RAD_NetEventManagerPublishGameCreated(RAD_NetEventManager_t *manager);
void RAD_NetEventManagerPublishGameFinished(RAD_NetEventManager_t *manager);
void RAD_NetEventManagerPublishCurrentPlayer(RAD_NetEventManager_t *manager, uint64_t user_id);
void RAD_NetEventManagerPublishPlayers(RAD_NetEventManager_t *manager, const uint64_t *user_ids, size_t number_of_users);
void RAD_NetEventManagerPublishWorldSize(RAD_NetEventManager_t *manager, uint32_t width, uint32_t height);
void RAD_NetEventManagerPublishTiles(RAD_NetEventManager_t *manager, const RAD_NetTile_t *tiles, size_t number_of_tiles);
void RAD_NetEventManagerPublishReserveUnit(RAD_NetEventManager_t *manager, const RAD_NetReserveUnit_t *unit);

void RAD_NetEventManagerSubscribeToTileEvents(RAD_NetEventManager_t *manager, RAD_NetEventsTileCallback_t callbacks);
void RAD_NetEventManagerPublishTileCreated(RAD_NetEventManager_t *manager, const RAD_NetTile_t *tile);
void RAD_NetEventManagerPublishTileRemoved(RAD_NetEventManager_t *manager, uint32_t x, uint32_t y);
void RAD_NetEventManagerPublishTileChanged(RAD_NetEventManager_t *manager, const RAD_NetTile_t *tile);

#endif
