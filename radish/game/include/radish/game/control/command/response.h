#ifndef __RAD_COMMAND_RESPONSE_H__
#define __RAD_COMMAND_RESPONSE_H__

#include <radish/game/control/command/codec.h>

///
/// Uebertragungsformat der Antwort auf ein Kommando, 13 Byte:
///
///     Offset 0     type       1 Byte   Wire-Nummer der Kommandoart
///     Offset 1     sequence   8 Byte   uint64
///     Offset 9     value      4 Byte   uint32
///
/// Die ersten neun Byte sind derselbe Kopf wie beim Kommando und werden von
/// derselben Stelle geschrieben und gelesen (RAD_SerializeCommandHeader in
/// codec.h). Genau das ist der Zweck des Kopfes auf dem Rueckweg: die Antwort
/// traegt Art und Sequenznummer ihres Kommandos zurueck, und nur daran erkennt
/// der Absender, worauf sie antwortet.
///
/// Anders als die Dateien der einzelnen Kommandoarten, die nur ihre Nutzlast
/// beschreiben, ist das hier beides -- Kopf und Nutzlast. Es gibt nur eine Art von
/// Antwort, also braucht es auch keine Verzweigung, die den Kopf voranstellen
/// koennte. Diese beiden Funktionen sind damit der Einstieg fuer eine ganze
/// Nachricht, wie RAD_SerializeCommand und RAD_DeserializeCommand.
///
/// "value" geht unveraendert durch. Was darin steht, legt der fest, der das
/// Kommando ausgefuehrt hat; die Bedeutung pro Kommandoart ist noch offen, und
/// solange sie es ist, deutet der Codec die vier Byte nicht.
///
/// Offen ist auch die Antwort auf eine Nachricht, die sich nicht lesen liess: dann
/// gibt es weder Art noch Sequenznummer, die zurueckgetragen werden koennten. Das
/// ist keine Frage des Formats, sondern eine des Protokolls -- zu entscheiden von
/// dem, der die Nachrichten annimmt.
///

///
/// Schreibt Kopf und Wert der Antwort in den Writer.
///
/// Gibt nichts zurueck: ein zu kleiner Puffer klebt im Writer, gefragt wird einmal
/// am Ende mit RAD_ByteWriterOk. Eine Antwort mit RAD_COMMAND_TYPE_NONE im Kopf
/// ergibt die reservierte 0 und wird von der Gegenseite abgelehnt -- so wie ein
/// Kommando ohne Art.
///
void RAD_SerializeCommandResponse(RAD_ByteWriter_t *writer, const RAD_CommandResponse_t *response);

///
/// Liest eine Antwort aus dem Reader.
///
/// "response" wird nur bei RAD_COMMAND_CODEC_OK beschrieben und bleibt bei jedem
/// Fehler unberuehrt. Der Reader muss genau ueber eine Nachricht laufen: bleibt
/// danach ein Byte uebrig, ist das RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES.
///
RAD_CommandCodecResult_t RAD_DeserializeCommandResponse(RAD_ByteReader_t *reader, RAD_CommandResponse_t *response);

#endif
