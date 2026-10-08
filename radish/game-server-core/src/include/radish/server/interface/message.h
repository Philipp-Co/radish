#ifndef __RAD_INTERFACE_MESSAGE_H__
#define __RAD_INTERFACE_MESSAGE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/game/control/command/command.h>
#include <radish/game/model/tile/tile.h>
#include <radish/game/model/unit/unit.h>

///
/// interface/ -- die Aussengrenze des Servers. Hier wird aus einer Nachricht ein
/// Kommando und aus einer Antwort wieder eine Nachricht. Was dazwischen liegt --
/// zu entscheiden, ob das Kommando gilt, und es auszufuehren -- gehoert nach
/// control/; dieses Modul aendert nie einen Spielzustand und deutet ein Kommando
/// auch nicht.
///
///     Nachricht ──►│ interface/ │──► RAD_Command_t ──►│ control/ │──► game/
///     Nachricht ◄──│            │◄── RAD_CommandResponse_t ◄──────┘
///
/// Ersetzt command.{h,c}: das Byteformat ist jetzt Protobuf
/// (radish/protobuf/*.proto, uebersetzt nach message.pb-c.h ueber das Ziel
/// radish_protobuf -- siehe protobuf/CMakeLists.txt und die Root-CMakeLists.txt,
/// die radish_protobuf jetzt auch fuer den Host-Build baut), nicht mehr der
/// handgeschriebene Codec aus game/.../control/command/codec.h. Den kennt
/// dieses Modul nicht mehr -- RAD_ByteReader_t/RAD_ByteWriter_t und
/// RAD_SerializeCommand/RAD_DeserializeCommand werden hier nicht benutzt.
///
/// Wie beim Client (client/src/io/net_codec.h) bleiben die generierten NetXxx-
/// Typen und jedes protobuf-c-Symbol auf message.c beschraenkt: Aufrufer sehen
/// nur RAD_Command_t/RAD_CommandResponse_t aus game/ und die eigene
/// RAD_NetCodecResult_t.
///
/// **Abgebildet sind move_unit, deploy_unit, end_turn und attack** -- alles, was
/// protobuf/command.proto kennt. Eine Nachricht ohne Kommando liest
/// RAD_ParseCommandFromMessage nicht, und RAD_SerializeCommandResponseToMessage
/// packt keine Antwort auf eine Kommandoart, die command.proto nicht kennt
/// (remove_unit, create_tile, remove_tile, use) -- beide liefern
/// RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE.
///
/// Das Modul kennt zucchini nicht. Es bekommt einen Bytebereich und schreibt in
/// einen -- wer die Bytes gebracht hat, ist ihm gleich. Damit laesst es sich ohne
/// Shared Memory pruefen, und die Strecke waere austauschbar.
///
/// Was ankommt, ist die reine Nutzlast: das 8-Byte-Codefeld vor jedem Paket
/// wertet zucchini_server selbst aus (Whitelist) und schneidet es ab.
///
/// Eine Nachricht traegt genau ein Kommando. Kaeme spaeter mehr als eines pro
/// Nachricht, waere das eine Erweiterung dieser Schnittstelle und keine Frage des
/// Formats.
///
/// **"success"/"description" statt eines einzelnen "value".** protobuf/
/// command.proto kennt fuer NetCommandResponse kein "value"-Feld mehr, sondern
/// einen Erfolg (bool) und einen Text. Was daraus wird, entscheidet dieses
/// Modul nicht -- es kennt RAD_ControlResult_t nicht (siehe oben, "deutet ein
/// Kommando auch nicht"). RAD_SerializeCommandResponseToMessage nimmt beides
/// deshalb als eigene Parameter entgegen; main.c fuellt sie aus response->value
/// (RAD_ControlResult_t) und RAD_ControlResultText davon.
///
typedef enum
{
    RAD_NET_CODEC_OK = 0,

    /// Die Kommandoart ist im aktuellen command.proto nicht abgebildet (siehe
    /// oben): beim Lesen ein eingebettetes NetCommandRequest ganz ohne gesetzten
    /// Zweig (commands_case NOT_SET), beim Schreiben eine Antwort auf ein anderes
    /// Kommando als move_unit, deploy_unit, end_turn oder attack.
    RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE,

    /// Die Anzahl der Felder eines Pfades liegt nicht in [2, RAD_PATH_MAX_STEPS]
    /// (path.h) -- symmetrisch zu RAD_COMMAND_CODEC_ERROR_INVALID_STEP_COUNT im
    /// alten Codec (game/.../command/move_unit.h).
    RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT,

    /// net_server_message__pack wuerde mehr Bytes schreiben, als der uebergebene
    /// Puffer fasst.
    RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL,

    /// net_user_request__unpack konnte die Bytes nicht als NetUserRequest lesen
    /// -- kaputte oder fremde Daten.
    RAD_NET_CODEC_ERROR_DECODE_FAILED,

    /// Die NetUserRequest (oder eine ihrer eingebetteten Nachrichten) ist
    /// strukturell nicht das, was ihr data_case/commands_case behauptet -- ein
    /// als gesetzt markierter Zweig, dessen Zeiger trotzdem NULL ist, oder gar
    /// kein Zweig gesetzt (*__NOT_SET).
    RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE,

    /// Die Nachricht ist kuerzer als die 8 Byte Absender, die ihr das Backend
    /// voranstellt (RAD_ParseSenderFromMessage).
    RAD_NET_CODEC_ERROR_NO_SENDER
} RAD_NetCodecResult_t;

///
/// Macht daraus einen Text zum Loggen, wie RAD_ControlResultText. Immer ein
/// gueltiger Zeiger, auch bei einem Wert ausserhalb der Aufzaehlung.
///
const char* RAD_NetCodecResultText(RAD_NetCodecResult_t result);

///
/// Liest ein Kommando aus einer eingehenden Nachricht (NetUserRequest,
/// protobuf/message.proto).
///
/// "message" ist die Nutzlast, "size" ihre Laenge; nullterminiert ist sie nicht,
/// und beides wird nur gelesen. "out_command" nimmt das Kommando auf und bleibt
/// bei jedem Ergebnis ausser RAD_NET_CODEC_OK unberuehrt.
///
/// command_request.id geht in header.sequence, NetMoveCommand.user_id in
/// header.user. Die Sequenznummer ist im .proto uint32, im Kommando uint64 --
/// die Umkehrung der Anmerkung beim Client zum Encodieren (net_codec.h dort),
/// hier also eine Erweiterung. Der Absender ist auf beiden Seiten uint64.
///
/// Geprueft wird der Absender hier nicht: ob der Benutzer mitspielt und ob ihm
/// gehoert, was das Kommando anfasst, ist keine Frage der Nachricht. Sie wird
/// daneben beantwortet, in radish/server/control/execute.h.
///
RAD_NetCodecResult_t RAD_ParseCommandFromMessage(const uint8_t *message, uint16_t size, RAD_Command_t *out_command);

///
/// Trennt den Absender von der Nutzlast.
///
/// **Den Absender setzt das Backend, nicht der Client.** Jede Nachricht, die hier
/// ankommt, hat der WebSocket-Consumer des Backends gebaut (radish/backend/api/
/// consumers.py):
///
///     [ Zucchini-Code 8 Byte ][ Spieler-Id 8 Byte ][ NetUserRequest ]
///
/// Den Code -- geheim, je Spieler und Spiel vom Backend vergeben -- prueft
/// zucchini_server gegen seine Whitelist und schneidet ihn ab. Was hier ankommt,
/// beginnt also mit der Spieler-Id: der gepackten, oeffentlichen Kennung
/// (RAD_ControlUserIdFromIdentifier), Big-Endian. Weil nur das Backend die Codes
/// kennt, kommt keine Nachricht durch, die es nicht gebaut hat -- die Id ist damit
/// so vertrauenswuerdig wie die Anmeldung beim Backend.
///
/// "sender" bekommt die Id, "payload"/"payload_size" den Rest, die NetUserRequest
/// fuer RAD_ParseDiscoverFromMessage, RAD_ParseDiscoverReserveFromMessage und
/// RAD_ParseCommandFromMessage. RAD_NET_CODEC_ERROR_NO_SENDER, wenn die Nachricht
/// keine 8 Byte lang ist; die Ausgaben bleiben dann unberuehrt.
///
/// Ein user_id im Kommando selbst (command.proto) zaehlt nicht: der Absender
/// eines Kommandos ist "sender", der Aufrufer setzt ihn als header.user.
///
RAD_NetCodecResult_t RAD_ParseSenderFromMessage(const uint8_t *message, uint16_t size,
                                                 RAD_UserId_t *sender,
                                                 const uint8_t **payload, uint16_t *payload_size);

///
/// Ein Ausschnitt der Welt, den ein Client erkunden will: die linke obere Ecke
/// (x, y) und Breite und Hoehe (w, h), in Feldern. Kein Kommando -- es aendert
/// nichts am Spiel und hat deshalb auch keinen Platz in RAD_Command_t.
///
typedef struct
{
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} RAD_DiscoverRequest_t;

///
/// Liest eine Discover-Anfrage aus einer eingehenden Nachricht (NetUserRequest
/// mit gesetztem discover_request, protobuf/message.proto).
///
/// RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, wenn die Nachricht zwar lesbar ist,
/// aber einen anderen Zweig traegt -- der Aufrufer versucht es dann als Kommando
/// (RAD_ParseCommandFromMessage). "out_request" bleibt bei jedem Ergebnis ausser
/// RAD_NET_CODEC_OK unberuehrt.
///
/// Geprueft wird der Ausschnitt nicht: ob er in der Welt liegt, ist keine Frage
/// der Nachricht.
///
RAD_NetCodecResult_t RAD_ParseDiscoverFromMessage(const uint8_t *message, uint16_t size, RAD_DiscoverRequest_t *out_request);

///
/// Erkennt die Anfrage nach den Reserven (NetUserRequest mit gesetztem
/// discover_reserve_request, protobuf/discover.proto). Sie traegt nichts -- was
/// zu tun ist, sagt allein ihr Zweig, deshalb gibt es hier kein "out_request".
///
/// RAD_NET_CODEC_OK fuer genau diese Anfrage, RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE
/// fuer eine lesbare Nachricht mit einem anderen Zweig -- der Aufrufer versucht es
/// dann anders, wie bei RAD_ParseDiscoverFromMessage.
///
RAD_NetCodecResult_t RAD_ParseDiscoverReserveFromMessage(const uint8_t *message, uint16_t size);

///
/// Erkennt die Anfrage nach allen Einheiten der Spieler (NetUserRequest mit
/// gesetztem discover_units_request, protobuf/discover.proto), wie
/// RAD_ParseDiscoverReserveFromMessage die nach den Reserven.
///
RAD_NetCodecResult_t RAD_ParseDiscoverUnitsFromMessage(const uint8_t *message, uint16_t size);

///
/// Eine Einheit der Reserve als eigene Nachricht (NetServerMessage -> NetEvent ->
/// NetGameEvent mit reserve_unit, protobuf/game.proto): die ganze Einheit --
/// Id, Besitzer, Name, ihre Werte und alle Mitglieder mit ihren Waffen. Ob sie
/// wirklich in der Reserve steht, entscheidet der Aufrufer -- dieses Modul
/// deutet keinen Zustand.
///
/// **Eine volle Einheit passt nicht immer** in einen Ringpuffer-Platz (game.proto
/// NetUnit); dass sie es tut, stellt der Server sicher, wenn er Einheiten
/// aufnimmt. Reicht "capacity" nicht, wie bei den uebrigen Ereignissen
/// RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL -- es wird nichts geschrieben.
///
RAD_NetCodecResult_t RAD_SerializeReserveUnitEventToMessage(const RAD_Unit_t *unit,
                                                              uint8_t *out_message,
                                                              uint16_t capacity,
                                                              uint16_t *out_size);

///
/// Eine Einheit als eigene Nachricht (NetServerMessage -> NetEvent ->
/// NetGameEvent mit unit, protobuf/game.proto): dieselbe Einheit wie bei der
/// Reserve, ohne zu sagen, wo sie steht. Wem sie gehoert, prueft dieses Modul
/// nicht. Groesse und Fehler wie bei der Reserve.
///
RAD_NetCodecResult_t RAD_SerializeUnitEventToMessage(const RAD_Unit_t *unit,
                                                      uint8_t *out_message,
                                                      uint16_t capacity,
                                                      uint16_t *out_size);

///
/// Eine Einheit, die jetzt auf dem Feld (x, y) steht (NetServerMessage -> NetEvent
/// -> NetGameEvent mit unit_deployed, protobuf/game.proto): die Einheit wie bei
/// der Reserve, dazu das Feld. Geschickt wird es, wenn das Spiel meldet, dass eine
/// Figur aufgestellt wurde (RAD_OnUnitSpawned_t) -- ob das stimmt, prueft dieses
/// Modul nicht.
///
/// x und y liegen in der Welt und sind damit nie negativ; sie gehen als uint32
/// hinaus wie bei den Feldern. Groesse und Fehler wie bei der Reserve.
///
RAD_NetCodecResult_t RAD_SerializeUnitDeployedEventToMessage(const RAD_Unit_t *unit,
                                                               int32_t x,
                                                               int32_t y,
                                                               uint8_t *out_message,
                                                               uint16_t capacity,
                                                               uint16_t *out_size);

///
/// Schreibt eine Antwort als ausgehende Nachricht (NetServerMessage mit
/// gesetztem command_response, protobuf/message.proto).
///
/// "success"/"description" sind, was RAD_ControlResult_t auf der Strecke wird
/// (siehe oben) -- dieses Modul berechnet sie nicht, es traegt sie nur ein.
/// "out_message" nimmt die Bytes auf, "capacity" ist der Platz darin, "out_size"
/// die geschriebene Laenge; beide werden nur bei RAD_NET_CODEC_OK beschrieben.
///
/// response->command muss move_unit oder deploy_unit sein (siehe oben) -- sonst
/// RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE. RAD_NET_CODEC_ERROR_
/// BUFFER_TOO_SMALL, wenn "capacity" nicht reicht; anders als beim alten Codec
/// klebt ein Ueberlauf hier nicht in einem Writer, sondern wird vor dem
/// Schreiben ueber net_server_message__get_packed_size erkannt.
///
///
/// Die Spiel-Ereignisse, die ein Client auf eine Discover-Anfrage bekommt, je als
/// eigene Nachricht (NetServerMessage -> NetEvent -> NetGameEvent,
/// protobuf/game.proto):
///
///   RAD_SerializeCurrentPlayerEventToMessage  current_player -- wer dran ist,
///                                             RAD_USER_NONE wenn niemand
///   RAD_SerializePlayersEventToMessage        players -- "users" in Zugreihenfolge,
///                                             "number_of_users" darf 0 sein
///   RAD_SerializeWorldSizeEventToMessage      world_size -- Breite und Hoehe in Feldern
///   RAD_SerializeTilesEventToMessage          tiles -- "number_of_tiles" Felder mit
///                                             ihren Attributen, darf 0 sein
///
/// Wie bei der Antwort: "out_message"/"out_size" werden nur bei RAD_NET_CODEC_OK
/// beschrieben, RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, wenn "capacity" nicht
/// reicht. Die Ids gehen ungekuerzt als uint64 hinaus, wie der Absender eines
/// Kommandos.
///
RAD_NetCodecResult_t RAD_SerializeCurrentPlayerEventToMessage(RAD_UserId_t user,
                                                                uint8_t *out_message,
                                                                uint16_t capacity,
                                                                uint16_t *out_size);
RAD_NetCodecResult_t RAD_SerializePlayersEventToMessage(const RAD_UserId_t *users,
                                                          size_t number_of_users,
                                                          uint8_t *out_message,
                                                          uint16_t capacity,
                                                          uint16_t *out_size);
RAD_NetCodecResult_t RAD_SerializeWorldSizeEventToMessage(uint32_t width,
                                                            uint32_t height,
                                                            uint8_t *out_message,
                                                            uint16_t capacity,
                                                            uint16_t *out_size);


///
/// Felder als NetTile (protobuf/tile.proto), Attribut fuer Attribut:
///
///   x, y, z    unveraendert
///   type       RAD_TileType_t auf NetTileType, VOID eingeschlossen
///   unit_id    die Id der Figur, unveraendert, RAD_UNIT_NONE (-1)
///              eingeschlossen -- welche Ids es gibt, bestimmt das Spiel.
///
/// Wie viele Felder in eine Nachricht passen, entscheidet der Aufrufer ueber
/// "number_of_tiles"; reicht "capacity" nicht, kommt
/// RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL und er schickt kleinere Stuecke.
///
RAD_NetCodecResult_t RAD_SerializeTilesEventToMessage(const RAD_Tile_t *tiles,
                                                        size_t number_of_tiles,
                                                        uint8_t *out_message,
                                                        uint16_t capacity,
                                                        uint16_t *out_size);

RAD_NetCodecResult_t RAD_SerializeCommandResponseToMessage(const RAD_CommandResponse_t *response,
                                                             bool success,
                                                             const char *description,
                                                             uint8_t *out_message,
                                                             uint16_t capacity,
                                                             uint16_t *out_size);

#endif
