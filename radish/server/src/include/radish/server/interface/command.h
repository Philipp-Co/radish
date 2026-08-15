#ifndef __RAD_INTERFACE_COMMAND_H__
#define __RAD_INTERFACE_COMMAND_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/control/command/codec.h>
#include <radish/game/control/command/response.h>

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
/// Das Byteformat steht nicht hier, sondern im Spielmodul:
/// <radish/game/control/command/codec.h> beschreibt es und uebersetzt in beide
/// Richtungen, mit einer Datei je Kommandoart. Dieses Modul ist der Adapter
/// darauf und duenn mit Absicht -- ihm bleiben genau die Fragen der Nachricht:
/// wo die Bytes anfangen und aufhoeren, wie viel Platz die Antwort hat, und dass
/// nichts Abgeschnittenes hinausgeht.
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
/// Ein eigenes Ergebnis-Enum gibt es hier nicht mehr: das des Codecs
/// (RAD_CommandCodecResult_t) geht unveraendert durch. Ein zweites daneben waere
/// entweder ungenauer -- der Codec unterscheidet Gruende, die es nicht kennt --
/// oder eine Kopie. Mit dem Ergebnis kommt RAD_CommandCodecResultText fuers Log
/// gleich mit.
///

///
/// Liest ein Kommando aus einer eingehenden Nachricht.
///
/// "message" ist die Nutzlast, "size" ihre Laenge; nullterminiert ist sie nicht,
/// und beides wird nur gelesen. "out_command" nimmt das Kommando auf und bleibt
/// bei jedem Ergebnis ausser RAD_COMMAND_CODEC_OK unberuehrt.
///
/// Die Nachricht muss genau ein Kommando sein und sonst nichts: bleibt ein Byte
/// uebrig, ist das RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES. Sequenznummer und
/// Absender aus dem Kopf werden uebernommen, nicht neu vergeben -- beide gehoeren
/// dem Absender, und nur mit ihnen kann er die Antwort seinem Kommando zuordnen.
///
/// Geprueft wird der Absender hier nicht: ob der Benutzer mitspielt und ob ihm
/// gehoert, was das Kommando anfasst, ist keine Frage der Nachricht. Sie wird
/// daneben beantwortet, in radish/server/control/execute.h.
///
RAD_CommandCodecResult_t RAD_ParseCommandFromMessage(const uint8_t *message, uint16_t size, RAD_Command_t *out_command);

///
/// Schreibt eine Antwort als ausgehende Nachricht.
///
/// "out_message" nimmt die Bytes auf, "capacity" ist der Platz darin, "out_size"
/// die geschriebene Laenge.
///
/// Liefert false, wenn die Antwort nicht in "capacity" passt -- der einzige Weg,
/// auf dem das hier scheitern kann, deshalb ein bool und kein Ergebniswert.
/// "out_size" bleibt dann unberuehrt und "out_message" gilt als unbrauchbar: eine
/// abgeschnittene Nachricht darf nicht hinausgehen. Sie ist 42 bis 50 Byte lang, je
/// Kommandoart eine feste Zahl (siehe response.h); ein Fehlschlag ist also kein
/// Grenzfall, sondern ein zu klein gewaehlter Puffer.
///
bool RAD_SerializeCommandResponseToMessage(const RAD_CommandResponse_t *response, uint8_t *out_message, uint16_t capacity, uint16_t *out_size);

#endif
