#ifndef __RAD_COMMAND_RESPONSE_H__
#define __RAD_COMMAND_RESPONSE_H__

#include <radish/game/control/command/codec.h>

///
/// Uebertragungsformat der Antwort auf ein Kommando, 42 bis 50 Byte:
///
///     Offset 0     type       1 Byte      Wire-Nummer der Kommandoart
///     Offset 1     sequence   8 Byte      uint64
///     Offset 9     user       8 Byte      uint64, Uuid des Absenders
///     Offset 17    value      4 Byte      uint32
///     Offset 21    command    21-29 Byte  ganze Kommandonachricht
///
/// Die ersten siebzehn Byte sind derselbe Kopf wie beim Kommando und werden von
/// derselben Stelle geschrieben und gelesen (RAD_SerializeCommandHeader in
/// codec.h). Genau das ist der Zweck des Kopfes auf dem Rueckweg: die Antwort
/// traegt Art, Sequenznummer und Absender ihres Kommandos zurueck, und nur daran
/// erkennt der Absender, worauf sie antwortet -- die Sequenznummer allein reicht
/// dafuer nicht, sie zaehlt je Benutzer (siehe command.h).
///
/// Anders als die Dateien der einzelnen Kommandoarten, die nur ihre Nutzlast
/// beschreiben, ist das hier alles -- Kopf, Wert und Kommando. Es gibt nur eine Art
/// von Antwort, also braucht es auch keine Verzweigung, die den Kopf voranstellen
/// koennte. Diese beiden Funktionen sind damit der Einstieg fuer eine ganze
/// Nachricht, wie RAD_SerializeCommand und RAD_DeserializeCommand.
///
/// "value" geht unveraendert durch. Was darin steht, legt der fest, der das
/// Kommando ausgefuehrt hat -- im Server ist es RAD_ControlResult_t, fuer jede
/// Kommandoart dieselbe Aufzaehlung. Der Codec deutet die vier Byte nicht.
///
/// **Hinter dem Wert steht das Kommando ein zweites Mal, vollstaendig** -- nicht
/// nur seine Nutzlast, sondern die ganze Nachricht in genau dem Format aus codec.h,
/// Kopf eingeschlossen. Damit ist die Antwort aus sich heraus lesbar (warum, steht
/// bei RAD_CommandResponse_t), und der Codec des Kommandos wird als Ganzes
/// wiederverwendet, statt fuer die Antwort in Kopf und Nutzlast aufgebrochen zu
/// werden.
///
/// Der Preis sind siebzehn Byte, die schon davor stehen, und der Fehlerfall, den sie
/// moeglich machen: RAD_COMMAND_CODEC_ERROR_HEADER_MISMATCH, wenn die beiden Koepfe
/// nicht dasselbe tragen. Wegfallen koennte nur einer der beiden, und keiner ist
/// entbehrlich -- ohne den vorderen liesse sich die Antwort nicht zuordnen, ohne den
/// hinteren das Kommando nicht mit dem Codec lesen, den es dafuer schon gibt.
///
/// Laengen je Kommandoart, Kopf und Wert eingeschlossen:
///
///     remove_entity, remove_tile    21 + 21 = 42
///     spawn_entity, create_tile     21 + 23 = 44
///     move_entity                   21 + 29 = 50
///
/// Weil das Kommando am Ende steht, bleibt die feste Laenge je Art die Pruefung,
/// wie beim Kommando selbst: fehlen Bytes, kommt TRUNCATED; sind es zu viele,
/// TRAILING_BYTES. Beides meldet RAD_DeserializeCommand, das dafuer nicht angefasst
/// werden musste.
///
/// Offen ist auch die Antwort auf eine Nachricht, die sich nicht lesen liess: dann
/// gibt es weder Art noch Sequenznummer, die zurueckgetragen werden koennten. Das
/// ist keine Frage des Formats, sondern eine des Protokolls -- zu entscheiden von
/// dem, der die Nachrichten annimmt.
///

///
/// Schreibt Kopf, Wert und Kommando der Antwort in den Writer.
///
/// Gibt nichts zurueck: ein zu kleiner Puffer klebt im Writer, gefragt wird einmal
/// am Ende mit RAD_ByteWriterOk. Eine Antwort mit RAD_COMMAND_TYPE_NONE im Kopf
/// ergibt die reservierte 0 und wird von der Gegenseite abgelehnt -- so wie ein
/// Kommando ohne Art.
///
/// Die Invariante header == command.header wird hier nicht geprueft: geschrieben
/// wird, was dasteht. Hergestellt wird sie beim Erzeugen der Antwort
/// (RAD_ControlExecuteCommand im Server), und wer sie bricht, faellt auf der
/// Gegenseite mit RAD_COMMAND_CODEC_ERROR_HEADER_MISMATCH auf.
///
void RAD_SerializeCommandResponse(RAD_ByteWriter_t *writer, const RAD_CommandResponse_t *response);

///
/// Liest eine Antwort aus dem Reader.
///
/// "response" wird nur bei RAD_COMMAND_CODEC_OK beschrieben und bleibt bei jedem
/// Fehler unberuehrt. Der Reader muss genau ueber eine Nachricht laufen: bleibt
/// danach ein Byte uebrig, ist das RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES.
///
/// Neben den Fehlern des Kommandos -- die Antwort traegt eines, und es wird mit
/// demselben Codec gelesen -- kann hier RAD_COMMAND_CODEC_ERROR_HEADER_MISMATCH
/// kommen: beide Koepfe muessen Art, Sequenznummer und Absender gleich tragen.
///
RAD_CommandCodecResult_t RAD_DeserializeCommandResponse(RAD_ByteReader_t *reader, RAD_CommandResponse_t *response);

#endif
