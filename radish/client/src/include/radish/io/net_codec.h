#ifndef __RAD_IO_NET_CODEC_H__
#define __RAD_IO_NET_CODEC_H__

#include <stddef.h>
#include <stdint.h>
#include <radish/io/net_types.h>
#include <radish/events/event_manager.h>

///
/// Abstraktionsschicht zwischen dem WASM-Client und der generierten Protobuf-
/// Implementierung (protobuf/*.proto, uebersetzt nach
/// build/protobuf/generated/*.pb-c.* -- siehe protobuf/CMakeLists.txt). Aufrufer
/// in main.c sehen nur die Typen aus io/net_types.h und die Verteilung aus
/// events/event_manager.h, keinen der generierten NetXxx-Typen und kein
/// protobuf-c-Symbol. Die *.pb-c.h landen ausschliesslich in
/// net_codec.c.
///
/// **Der Server spricht dasselbe Format.** game-server-core/src/interface/
/// message.c ist das Gegenstueck: dieselben protobuf/*.proto, nur ueber
/// NetUserRequest gelesen und NetServerMessage geschrieben statt umgekehrt. Ein
/// hier gepacktes Kommando laesst sich damit gegen den laufenden Server
/// verschicken -- die Abschneidung der Sequenznummer auf 32 Bit unten bei
/// RAD_NetEncodeMoveRequest ist deshalb keine hypothetische Einschraenkung,
/// sondern wirkt auf der echten Strecke.
///
/// **Beim Kommando sind Zug, Deployment, Abgeben und Angriff abgebildet** --
/// alles, was protobuf/command.proto kennt. Der Angriff (NetAttackCommand)
/// zielt auf ein Feld, wie der Client es waehlt.
///
/// **Spiel- und Tile-Ereignisse sind dagegen vollstaendig abgebildet.**
/// protobuf/event.proto/game.proto/tile.proto kennen keine nicht modellierten
/// Varianten wie command.proto -- jede Variante von NetGameEvent/NetTileEvent
/// hat ein Gegenstueck in events/event_manager.h und wird dorthin
/// veroeffentlicht.
///
typedef enum
{
    RAD_NET_CODEC_OK = 0,

    /// Die Kommandoart ist im Client nicht abgebildet (siehe oben): eine
    /// Antwort, deren eingebettetes NetCommandRequest gar keinen Zweig traegt
    /// (commands_case NOT_SET).
    RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE,

    /// Die Anzahl der Felder eines Pfades liegt nicht in
    /// [2, RAD_NET_PATH_MAX_STEPS] (net_types.h).
    RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT,

    /// net_user_request__pack wuerde mehr Bytes schreiben, als der uebergebene
    /// Puffer fasst.
    RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL,

    /// net_server_message__unpack konnte die Bytes nicht als NetServerMessage
    /// lesen -- kaputte oder fremde Daten.
    RAD_NET_CODEC_ERROR_DECODE_FAILED,

    /// Die NetServerMessage (oder eine ihrer eingebetteten Nachrichten) ist
    /// strukturell nicht das, was ihr data_case/event_case behauptet -- ein
    /// als gesetzt markierter Zweig, dessen Zeiger trotzdem NULL ist, oder gar
    /// kein Zweig gesetzt (*__NOT_SET).
    RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE
} RAD_NetCodecResult_t;

///
/// Macht daraus einen Text zum Loggen, nach demselben Muster wie
/// RAD_ControlResultText (game-server-core). Immer ein gueltiger Zeiger, auch
/// bei einem Wert ausserhalb der Aufzaehlung.
///
const char* RAD_NetCodecResultText(RAD_NetCodecResult_t result);

///
/// Packt einen Zug als NetUserRequest (protobuf/message.proto) in "buffer".
/// "buffer_size" ist dessen Kapazitaet, "out_length" bekommt die tatsaechlich
/// geschriebene Laenge -- "buffer"/"out_length" werden nur bei RAD_NET_CODEC_OK
/// beschrieben.
///
/// request->sequence geht in NetCommandRequest.id, request->user in
/// NetMoveCommand.user_id. Die Sequenznummer ist im .proto uint32, im Client
/// uint64 -- sie wird auf 32 Bit abgeschnitten, und der Server erweitert sie beim
/// Empfang wieder. Der Absender geht ungekuerzt als uint64: er ist die gepackte
/// Spieler-Kennung und braucht alle 64 Bit.
///
/// Liefert RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT bei einer Schrittzahl
/// ausserhalb von [2, RAD_NET_PATH_MAX_STEPS] und
/// RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, wenn "buffer" zu klein ist.
///
RAD_NetCodecResult_t RAD_NetEncodeMoveRequest(const RAD_NetMoveRequest_t *request,
                                               uint8_t *buffer,
                                               size_t buffer_size,
                                               size_t *out_length);

///
/// Packt ein Deployment als NetUserRequest (protobuf/message.proto, Zweig
/// command_request mit NetDeployCommand) in "buffer", wie
/// RAD_NetEncodeMoveRequest einen Zug: Sequenznummer auf 32 Bit gekuerzt, der
/// Absender ungekuerzt. Die Koordinaten gehen als uint32 hinaus; eine negative
/// wird dabei riesig, und der Server lehnt sie ab.
///
/// Liefert RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, wenn "buffer" zu klein ist.
///
RAD_NetCodecResult_t RAD_NetEncodeDeployRequest(const RAD_NetDeployRequest_t *request,
                                                 uint8_t *buffer,
                                                 size_t buffer_size,
                                                 size_t *out_length);

///
/// Packt einen Angriff als NetUserRequest (protobuf/message.proto, Zweig
/// command_request mit NetAttackCommand) in "buffer", wie
/// RAD_NetEncodeDeployRequest ein Deployment: das Zielfeld als uint32, eine
/// negative Koordinate lehnt der Server ab.
///
/// Liefert RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, wenn "buffer" zu klein ist.
///
RAD_NetCodecResult_t RAD_NetEncodeAttackRequest(const RAD_NetAttackRequest_t *request,
                                                 uint8_t *buffer,
                                                 size_t buffer_size,
                                                 size_t *out_length);

///
/// Packt ein Abgeben als NetUserRequest (protobuf/message.proto, Zweig
/// command_request mit NetEndTurnCommand) in "buffer", wie
/// RAD_NetEncodeDeployRequest ein Deployment.
///
/// Liefert RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, wenn "buffer" zu klein ist.
///
RAD_NetCodecResult_t RAD_NetEncodeEndTurnRequest(const RAD_NetEndTurnRequest_t *request,
                                                  uint8_t *buffer,
                                                  size_t buffer_size,
                                                  size_t *out_length);

///
/// Packt eine Discover-Anfrage als NetUserRequest (protobuf/message.proto,
/// Zweig discover_request) in "buffer": den Ausschnitt der Welt mit linker
/// oberer Ecke (x, y), Breite "w" und Hoehe "h", in Feldern. Kein Kommando --
/// sie traegt weder Sequenznummer noch Absender.
///
/// "buffer"/"out_length" werden nur bei RAD_NET_CODEC_OK beschrieben;
/// RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, wenn "buffer" zu klein ist.
///
RAD_NetCodecResult_t RAD_NetEncodeDiscover(uint32_t x,
                                            uint32_t y,
                                            uint32_t w,
                                            uint32_t h,
                                            uint8_t *buffer,
                                            size_t buffer_size,
                                            size_t *out_length);

///
/// Packt die Anfrage nach den Reserven als NetUserRequest (Zweig
/// discover_reserve_request, protobuf/discover.proto). Sie traegt nichts: auf der
/// Leitung sind es zwei Bytes, 1A 00. Der Server antwortet mit einem
/// NetReserveUnitEvent je Einheit (RAD_NetEventManagerPublishReserveUnit).
///
/// "buffer"/"out_length" werden nur bei RAD_NET_CODEC_OK beschrieben;
/// RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, wenn "buffer" zu klein ist.
///
RAD_NetCodecResult_t RAD_NetEncodeDiscoverReserve(uint8_t *buffer,
                                                   size_t buffer_size,
                                                   size_t *out_length);

///
/// Packt die Anfrage nach allen Einheiten der Spieler als NetUserRequest (Zweig
/// discover_units_request, protobuf/discover.proto), wie
/// RAD_NetEncodeDiscoverReserve die nach den Reserven. Der Server antwortet mit
/// einem NetUnitEvent je Einheit (RAD_NetEventManagerPublishUnit).
///
RAD_NetCodecResult_t RAD_NetEncodeDiscoverUnits(uint8_t *buffer,
                                                 size_t buffer_size,
                                                 size_t *out_length);

///
/// Liest eine NetServerMessage aus "data"/"length" (protobuf/message.proto) und
/// veroeffentlicht sie ueber "events" (events/event_manager.h). Das ist der
/// einzige Weg, eine eingehende Nachricht vom Server zu lesen; main.c ruft ihn
/// aus RAD_OnMessageReceived.
///
/// Je nach Zweig ruft sie genau eine der RAD_NetEventManagerPublish...-
/// Funktionen auf "events" auf:
///
///   command_response (nur move, siehe oben)        -> RAD_NetEventManagerPublishCommandResponse
///   command_response mit deploy                    -> RAD_NetEventManagerPublishDeployResponse
///   command_response mit end_turn                  -> RAD_NetEventManagerPublishEndTurnResponse
///   event.game.created                             -> RAD_NetEventManagerPublishGameCreated
///   event.game.finished                             -> RAD_NetEventManagerPublishGameFinished
///   event.game.current_player                       -> RAD_NetEventManagerPublishCurrentPlayer
///   event.game.players                              -> RAD_NetEventManagerPublishPlayers
///   event.game.world_size                           -> RAD_NetEventManagerPublishWorldSize
///   event.game.reserve_unit                         -> RAD_NetEventManagerPublishReserveUnit
///   event.game.unit_deployed                        -> RAD_NetEventManagerPublishUnitDeployed
///   event.game.unit                                 -> RAD_NetEventManagerPublishUnit
///   event.tile.created                              -> RAD_NetEventManagerPublishTileCreated
///   event.tile.removed                              -> RAD_NetEventManagerPublishTileRemoved
///   event.tile.changed                              -> RAD_NetEventManagerPublishTileChanged
///
/// Von NetCommandResponse kommt "success" mit (RAD_NetCommandResponse_t);
/// "description" geht verloren.
///
/// Nichts wird veroeffentlicht, wenn die Funktion einen Fehler zurueckgibt --
/// weder bei einem unlesbaren Kommandozweig (RAD_NET_CODEC_ERROR_UNSUPPORTED_
/// COMMAND_TYPE/RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT) noch bei kaputten oder
/// strukturell unerwarteten Bytes (RAD_NET_CODEC_ERROR_DECODE_FAILED/
/// RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE).
///
RAD_NetCodecResult_t RAD_NetDispatchServerMessage(RAD_NetEventManager_t *events,
                                                   const uint8_t *data,
                                                   size_t length);

#endif
