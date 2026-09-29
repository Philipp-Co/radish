#ifndef __RAD_WORLD_DEFINITION_H__
#define __RAD_WORLD_DEFINITION_H__

#include <stddef.h>
#include <radish/game/model/world/world.h>
#include <radish/game/serialization/serialization.h>
#include <radish/game/serialization/json_reader.h>

///
/// Weltdefinition -- die Welt, bevor gespielt wird. Das Format beschreibt
/// game/schema/world.schema.json:
///
///     {
///       "$schema": "../../game/schema/world.schema.json",
///       "version": 1,
///       "name": "Teich",
///       "rows":    [ "....", ".~~.", "_.._" ],
///       "heights": [ "1000", "0000", "0000" ]
///     }
///
///   version  Muss RAD_WORLD_DEFINITION_VERSION sein, sonst
///            RAD_SERIALIZE_ERROR_VERSION. Geprueft, bevor irgendetwas anderes
///            gelesen wird.
///   rows     Das Gelaende, eine Zeile je String, Zeile 0 zuerst; das Zeichen an
///            Stelle x ist das Feld (x, y). '_' VOID, '.' GROUND, '~' WATER.
///   heights  Optional: z je Feld als Ziffer 0-9, in derselben Form wie "rows".
///            Fehlt es, steht alles auf 0.
///   name     Optional, wird geprueft (ein String, der in RAD_JSON_NAME_MAX
///            passt) und nicht behalten -- die Welt hat keinen Namen.
///   $schema  Optional, fuer Editoren; uebergangen.
///
/// **Die Groesse steht nicht in der Datei.** Die Hoehe ist die Zahl der Zeilen,
/// die Breite die Laenge einer Zeile. Was das Schema dazu nicht fassen kann, prueft
/// dieser Leser, und es hat jeweils seinen eigenen Fehler:
///
///   RAD_SERIALIZE_ERROR_SIZE_MISMATCH  keine Zeile, mehr als RAD_WORLD_HEIGHT
///       Zeilen, eine leere oder laengere Zeile als RAD_WORLD_WIDTH, Zeilen
///       ungleicher Laenge, "heights" in anderer Form als "rows".
///   RAD_SERIALIZE_ERROR_TILE_TYPE      ein Zeichen in "rows", das kein Gelaende ist.
///   RAD_SERIALIZE_ERROR_SCHEMA         ein Zeichen in "heights", das keine
///       Ziffer ist; ein Schluessel, den es nicht gibt; ein Wert vom falschen Typ.
///   RAD_SERIALIZE_ERROR_INCONSISTENT   eine Hoehe ungleich 0 auf einem Feld ohne
///       Gelaende -- was nicht da ist, hat keine Hoehe (world.h).
///
/// **Strenger als der Spielstand.** Ein unbekannter Schluessel ist ein Fehler und
/// wird nicht uebergangen. Eine Weltdefinition wird von Hand geschrieben, und ein
/// vertipptes "heigths" soll beim Laden auffallen, statt still eine flache Welt
/// zu ergeben -- wie additionalProperties: false im Schema.
///
#define RAD_WORLD_DEFINITION_VERSION 1

///
/// Liest die Weltdefinition an der Cursorposition und baut daraus "world" auf.
///
/// Geschrieben wird erst, wenn alles gelesen und geprueft ist: bei einem Fehler
/// ist "world" unberuehrt. Aufgebaut wird still (RAD_ResetWorldToSize) -- wer
/// melden will, was sich geaendert hat, tut das danach selbst.
///
/// Figuren kennt die Datei nicht; die Welt hat danach keine.
///
RAD_SerializeResult_t RAD_DeserializeWorldDefinition(RAD_JsonReader_t *reader, RAD_World_t *world);

///
/// Liest die Weltdefinition aus JSON in die Welt von "game" und meldet danach die
/// geaenderten Felder, gegen den Stand und die Groesse vorher
/// (RAD_WorldPublishTileChanges). Ein misslungener Ladevorgang meldet nichts.
///
/// RAD_SERIALIZE_ERROR_WORLD_OCCUPIED, wenn im Spiel schon Figuren stehen --
/// geprueft vor allem anderen, denn dann kommt es auf die Datei nicht an.
///
RAD_SerializeResult_t RAD_DeserializeWorldDefinitionFromJson(RAD_Game_t *game, const char *json, size_t length);

#endif
