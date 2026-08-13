#ifndef __RAD_GAME_SERIALIZER_H__
#define __RAD_GAME_SERIALIZER_H__

#include <radish/game/game.h>
#include <radish/serialization/serialization.h>
#include <radish/serialization/json_writer.h>
#include <radish/serialization/json_reader.h>

///
/// Schema -- das Spiel ist ein JSON-Objekt:
///
///     { "world": { ... } }
///
///   world    Die einzige Welt des Spiels, siehe world_serializer.h.
///
/// Unbekannte Schluessel werden beim Lesen uebergangen.
///
/// Vorerst duenn, weil RAD_Game_t nur die Welt haelt. Existiert fuer die
/// Symmetrie und waechst mit, sobald Tick-Zaehler, Spielzustand und Spieler-Id
/// dazukommen -- neue Felder erscheinen dann hier neben "world".
///
void RAD_SerializeGame(RAD_JsonWriter_t *writer, const RAD_Game_t *game);
RAD_SerializeResult_t RAD_DeserializeGame(RAD_JsonReader_t *reader, RAD_Game_t *game);

#endif
