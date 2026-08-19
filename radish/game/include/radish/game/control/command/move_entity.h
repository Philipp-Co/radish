#ifndef __RAD_COMMAND_MOVE_ENTITY_H__
#define __RAD_COMMAND_MOVE_ENTITY_H__

#include <radish/game/control/command/codec.h>

///
/// Nutzlast von RAD_CommandMoveEntity_t, 69 Byte:
///
///     entity             4 Byte   int32, die RAD_EntityId_t
///     number_of_steps    1 Byte   int8
///     steps_to          64 Byte   RAD_PATH_MAX_STEPS mal x(2) y(2), je int16
///
/// Nur die Nutzlast -- den Kopf schreibt und liest RAD_SerializeCommand bzw.
/// RAD_DeserializeCommand.
///
/// **Die Schritte fahren immer alle mit, auch die ungenutzten.** Ein Zug ueber ein
/// Feld belegt dieselben 69 Byte wie einer ueber sechzehn und laesst sechzig davon
/// leer. Gekauft wird damit die Festlegung, auf der der ganze Codec steht: wer die
/// Kommandoart kennt, kennt die Laenge (codec.h). Ein Zaehler, aus dem sich
/// ergibt, wie viel dahinter noch kommt, waere das Laengenfeld, das es nicht geben
/// soll.
///
/// Die Plaetze hinter number_of_steps stehen deshalb auf beiden Seiten genullt: der
/// Serialisierer schreibt (0,0), gleich was im Speicher des Aufrufers hinter dem
/// Pfad stehengeblieben ist, der Deserialisierer nullt sie wieder. Zwei gleiche
/// Bewegungen ergeben so dieselbe Bytefolge, und ein gelesenes Kommando hat genau
/// eine Gestalt -- zwei lassen sich damit vergleichen, ohne den Pfad zu kennen.
///
/// **Geprueft wird die Anzahl, und zwar nach dem Lesen.** Sie muss in
/// [1, RAD_PATH_MAX_STEPS] liegen, sonst kommt
/// RAD_COMMAND_CODEC_ERROR_INVALID_STEP_COUNT: null Schritte sind keine Bewegung
/// (path.h), und mehr, als das Feld traegt, gibt es nicht. Dass die Pruefung erst
/// hinter dem Lesen steht, kostet nichts und ist der Vorteil der festen Laenge --
/// eine unsinnige Anzahl verbraucht dieselben Byte wie eine gueltige und kann den
/// Reader nicht aus dem Tritt bringen.
///
/// Sonst kann hier nichts unbekannt sein: alle Felder sind Zahlen, und jede
/// Bytefolge der richtigen Laenge mit einer gueltigen Anzahl ist ein lesbares
/// Kommando. Ob die Id auf eine Figur zeigt, ob sie dort steht, wo der Weg
/// anfaengt, ob die Schritte aneinandergrenzen und ob die Felder frei sind,
/// entscheidet erst das Ausfuehren. RAD_ENTITY_NONE (-1) wird deshalb auch nicht
/// abgewiesen; es ist eine gueltige Zahl und beim Ausfuehren ein Fehlschlag.
///
void RAD_SerializeCommandMoveEntity(RAD_ByteWriter_t *writer, const RAD_CommandMoveEntity_t *command);
RAD_CommandCodecResult_t RAD_DeserializeCommandMoveEntity(RAD_ByteReader_t *reader, RAD_CommandMoveEntity_t *command);

#endif
