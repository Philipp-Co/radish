#ifndef __RAD_COMMAND_CODEC_H__
#define __RAD_COMMAND_CODEC_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/control/command/command.h>
#include <radish/game/control/command/byte_writer.h>
#include <radish/game/control/command/byte_reader.h>

///
/// Uebertragungsformat der Kommandos -- die Bytefolge, die zwischen Client und
/// Server laeuft. Zu jeder Kommandoart gehoert eine eigene Datei in diesem
/// Verzeichnis, die genau ihre Nutzlast beschreibt; Kopf und Verzweigung stehen
/// hier.
///
///     Kopf, 9 Byte:
///
///       Offset 0     type       1 Byte   Wire-Nummer der Kommandoart
///       Offset 1     sequence   8 Byte   uint64
///
///     Nutzlast dahinter, je Art mit fester Laenge:
///
///       spawn_entity    entity_type(1) x(2) y(2) z(1)                    6 -> 15
///       move_entity     entity(4) from_x(2) from_y(2) to_x(2) to_y(2)   12 -> 21
///       remove_entity   entity(4)                                        4 -> 13
///       create_tile     tile_type(1) x(2) y(2) z(1)                      6 -> 15
///       remove_tile     x(2) y(2)                                        4 -> 13
///
///     Wire-Nummern:
///
///       Kommandoart    1 spawn_entity   2 move_entity   3 remove_entity
///                      4 create_tile    5 remove_tile
///       Entitaetstyp   1 player         2 npc
///       Tile-Typ       1 void           2 ground        3 water
///
/// Drei Festlegungen stecken darin:
///
/// **Die Nummern auf der Strecke haengen nicht an der Reihenfolge im enum.** Ein
/// spaeter in der Mitte eingefuegter Tile-Typ verschiebt die C-Werte, aber nicht
/// diese Tabelle -- sonst deutete er unterwegs Nachrichten um. Dasselbe Motiv wie
/// im Speicherformat, wo statt der Zahlen die Namen stehen; hier waeren Namen nur
/// zu teuer. Client und Server werden ohnehin nicht gemeinsam ausgerollt: im
/// Browser kann eine gecachte client.wasm liegen, waehrend der Server neu ist.
///
/// **Die 0 ist bei jeder Aufzaehlung reserviert** und nie gueltig. Damit wird eine
/// genullte oder halb ueberschriebene Nachricht immer abgelehnt, statt als
/// Kommando 0 durchzugehen.
///
/// **Die feste Laenge je Art ist die Pruefung.** Ein Laengenfeld gibt es nicht:
/// wer die Art kennt, kennt die Laenge. Fehlen Bytes, kommt TRUNCATED; sind es zu
/// viele, TRAILING_BYTES. Auch ein Versionsbyte gibt es nicht -- eine unpassende
/// Gegenseite faellt schon ueber die Art oder die Laenge auf.
///
/// Eine Nachricht traegt genau ein Kommando. RAD_DeserializeCommand erwartet
/// deshalb einen Reader, der genau ueber diese eine Nachricht laeuft.
///
/// Der Rueckweg steht daneben in response.h: dieselben neun Byte Kopf, dahinter
/// die vier Byte der Antwort.
///

typedef enum
{
    RAD_COMMAND_CODEC_OK = 0,

    /// Die Nachricht endet mitten im Kommando.
    RAD_COMMAND_CODEC_ERROR_TRUNCATED,

    /// Das Kommando ist vollstaendig, danach steht noch etwas.
    RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES,

    /// Wire-Nummer der Kommandoart gehoert zu keiner Art -- auch die 0.
    RAD_COMMAND_CODEC_ERROR_UNKNOWN_COMMAND_TYPE,

    RAD_COMMAND_CODEC_ERROR_UNKNOWN_ENTITY_TYPE,
    RAD_COMMAND_CODEC_ERROR_UNKNOWN_TILE_TYPE
} RAD_CommandCodecResult_t;

///
/// Macht daraus einen Text zum Loggen. Immer ein gueltiger Zeiger, auch bei einem
/// Wert ausserhalb der Aufzaehlung.
///
const char* RAD_CommandCodecResultText(RAD_CommandCodecResult_t result);

///
/// Kopf allein, die neun Byte von oben. Zwei Einstiegspunkte brauchen ihn --
/// RAD_SerializeCommand und der Codec der Antwort in response.h -- und deshalb
/// steht er hier und nicht in beiden. Ein Kommando und die Antwort darauf tragen
/// denselben Kopf; liefe er auseinander, koennte der Absender die Antwort seinem
/// Kommando nicht mehr zuordnen.
///
/// Beim Lesen wird die Art geprueft (RAD_COMMAND_CODEC_ERROR_UNKNOWN_COMMAND_TYPE,
/// die reservierte 0 eingeschlossen), die Sequenznummer nicht -- jede Zahl ist
/// eine gueltige Sequenz.
///
void RAD_SerializeCommandHeader(RAD_ByteWriter_t *writer, const RAD_CommandHeader_t *header);
RAD_CommandCodecResult_t RAD_DeserializeCommandHeader(RAD_ByteReader_t *reader, RAD_CommandHeader_t *header);

///
/// Schreibt Kopf und Nutzlast des Kommandos in den Writer.
///
/// Gibt nichts zurueck: ein zu kleiner Puffer klebt im Writer, gefragt wird
/// einmal am Ende mit RAD_ByteWriterOk. Ein Kommando mit
/// RAD_COMMAND_TYPE_NONE -- also ein nie gefuelltes -- ergibt einen Kopf mit der
/// reservierten 0 und keine Nutzlast; die Gegenseite lehnt das ab, statt es zu
/// deuten.
///
void RAD_SerializeCommand(RAD_ByteWriter_t *writer, const RAD_Command_t *command);

///
/// Liest ein Kommando aus dem Reader.
///
/// "command" wird nur bei RAD_COMMAND_CODEC_OK beschrieben -- bei jedem Fehler
/// bleibt es unberuehrt, es gibt also kein halb gefuelltes Kommando. Der Reader
/// muss genau ueber eine Nachricht laufen: bleibt danach ein Byte uebrig, ist das
/// TRAILING_BYTES.
///
/// Die Sequenznummer wird uebernommen, nicht neu vergeben; sie gehoert dem
/// Absender.
///
RAD_CommandCodecResult_t RAD_DeserializeCommand(RAD_ByteReader_t *reader, RAD_Command_t *command);

///
/// Aufzaehlung und Wire-Nummer ineinander. Nach dem Muster von
/// RAD_TileTypeToString/RAD_TileTypeFromString: "ok" darf NULL sein, und im
/// Fehlerfall kommt der Nullwert der Aufzaehlung zurueck. ToWire liefert fuer
/// alles Unbekannte die reservierte 0.
///
uint8_t RAD_CommandTypeToWire(RAD_CommandType_t type);
RAD_CommandType_t RAD_CommandTypeFromWire(uint8_t wire, bool *ok);

uint8_t RAD_EntityTypeToWire(RAD_EntityType_t type);
RAD_EntityType_t RAD_EntityTypeFromWire(uint8_t wire, bool *ok);

uint8_t RAD_TileTypeToWire(RAD_TileType_t type);
RAD_TileType_t RAD_TileTypeFromWire(uint8_t wire, bool *ok);

#endif
