#ifndef __RAD_WORLD_H__
#define __RAD_WORLD_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/game_definitions.h>
#include <radish/game/model/model.h>
#include <radish/game/model/tile/tile.h>
#include <radish/game/model/entity/entity.h>
#include <radish/game/control/events/event_manager.h>

struct RAD_World
{
    ///
    /// Das Raster, in der groessten Form, die das Programm kennt. Zur Welt gehoert
    /// davon nur das Rechteck width x height ab (0,0); was daneben liegt, ist
    /// Speicher und kein Feld. Die Groesse steht in der Welt und nicht in den
    /// Konstanten, weil sie aus der Weltdefinition kommt (world_definition.h) --
    /// RAD_WORLD_WIDTH und RAD_WORLD_HEIGHT sind nur die Obergrenze.
    ///
    RAD_Tile_t tiles[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];
    int32_t width;
    int32_t height;

    ///
    /// Pool mit Luecken: freie Slots tragen id == RAD_ENTITY_NONE. Der
    /// Array-Index ist die RAD_EntityId_t, weshalb Slots beim Entfernen nicht
    /// zusammengeschoben werden duerfen.
    ///
    RAD_Entity_t entities[RAD_MAX_ENTITIES];

    /// Anzahl belegter Slots, nicht der hoechste vergebene Index.
    int32_t number_of_entities;

    RAD_EventManager_t *event_manager;
};

///
/// Eine Welt in der groessten Form (RAD_WORLD_WIDTH x RAD_WORLD_HEIGHT). Solange
/// keine Weltdefinition geladen ist, ist das die Welt des Spiels.
///
RAD_World_t RAD_CreateWorld(RAD_EventManager_t *event_manager);

///
/// Bringt die Welt in den Grundzustand: jedes Feld Boden auf Hoehe 0, keine Figur.
///
/// RAD_InitWorld meldet danach jedes Feld als added -- der Aufbau einer Welt,
/// die es vorher nicht gab. RAD_ResetWorld tut dasselbe still. Es ist fuer den
/// Fall, in dem der Grundzustand nur ein Zwischenschritt ist, wie beim Laden
/// (world_serializer.c): dort wuerde sonst jedes Feld als Boden gemeldet, der
/// gleich darauf ueberschrieben wird. Wer still zuruecksetzt, meldet danach
/// selbst, was sich geaendert hat (RAD_WorldPublishTileChanges).
///
/// Beide behalten die Groesse der Welt. RAD_ResetWorldToSize setzt genauso still
/// zurueck und gibt der Welt dabei eine neue; sie liefert false und schreibt
/// nichts, wenn die Groesse nicht in 1..RAD_WORLD_WIDTH bzw. 1..RAD_WORLD_HEIGHT
/// liegt. Der Speicher ausserhalb der neuen Groesse wird VOID, damit dort nichts
/// Altes stehenbleibt, das nach Gelaende aussieht.
///
void RAD_InitWorld(RAD_World_t *world);
void RAD_ResetWorld(RAD_World_t *world);
bool RAD_ResetWorldToSize(RAD_World_t *world, int32_t width, int32_t height);

bool RAD_WorldInBounds(const RAD_World_t *world, int32_t x, int32_t y);
RAD_Tile_t* RAD_WorldTileAt(RAD_World_t *world, int32_t x, int32_t y);
RAD_Entity_t* RAD_WorldEntityById(RAD_World_t *world, RAD_EntityId_t id);
RAD_Entity_t* RAD_WorldEntityAt(RAD_World_t *world, int32_t x, int32_t y);

///
/// Die zwei Funktionen, die den Typ eines Tiles schreiben duerfen.
///
/// **Ein Tile kommt nicht dazu und faellt nicht weg.** Das Raster ist von
/// RAD_InitWorld an vollstaendig besetzt und bleibt es -- deshalb ist
/// RAD_GameNumberOfTiles eine Festlegung und keine Zaehlung (tile.h).
/// "Hinzufuegen" und "Entfernen" heisst hier also: Gelaende auf ein Feld stellen
/// und es davon wegnehmen. Kein Gelaende ist RAD_TILE_TYPE_VOID, ein Zustand des
/// Feldes und keine fehlende Angabe -- genauso sieht es das Kommando dazu
/// (RAD_CommandCreateTile_t, control/command/command.h).
///
/// **Das Ereignis folgt dem Uebergang, nicht dem Namen der Funktion:**
///
///     VOID -> X                        added
///     X -> VOID                        removed
///     X -> Y, oder X mit anderem z     changed
///     nichts geaendert                 keines
///
/// Ein Abonnent erfaehrt damit, was geschehen ist, und nicht, wie es hiess: ein
/// zweites "added" fuer ein Feld, das er schon kennt, waere fuer ihn von einem
/// ersten nicht zu unterscheiden. Aus demselben Grund ist RAD_WorldAddTile mit
/// RAD_TILE_TYPE_VOID kein Fehler, sondern ein Entfernen -- beide Wege fuehren zum
/// selben Zustand, und heraus geht der Zustand.
///
/// **Entfernen laesst die Stelle stehen:** x, y, z und die Entitaet bleiben, nur
/// der Typ wird VOID. Weggenommen wird das Gelaende und nicht das Feld -- und wer
/// es wieder hinstellt, bringt seine Hoehe selbst mit.
///
/// **Eine Figur haelt ihr Gelaende.** RAD_WorldRemoveTile liefert false, solange
/// eine Entitaet auf dem Feld steht, und schreibt nichts. Es ist der einzige Fall,
/// in dem eine Aenderung abgelehnt wird, obwohl sie sich hinschreiben liesse:
/// was mit einer Figur ueber dem Nichts geschieht -- fallen, stehenbleiben,
/// sterben --, ist eine Regel, die es noch nicht gibt, und die Welt erfindet sie
/// nicht still. Wer es trotzdem will, nimmt zuerst die Figur
/// (RAD_WorldRemoveEntity).
///
/// Sonst gibt es false nur fuer ein (x,y) ausserhalb der Welt. Ein Feld ohne
/// Gelaende zu entfernen ist true und tut nichts, und ein Feld auf denselben Typ
/// mit derselben Hoehe zu setzen genauso: nichts zu tun ist kein Fehler, wenn das
/// Ergebnis stimmt -- wie bei RAD_GameRemovePlayer.
///
bool RAD_WorldAddTile(RAD_World_t *world, int32_t x, int32_t y, int32_t z, RAD_TileType_t type);
bool RAD_WorldRemoveTile(RAD_World_t *world, int32_t x, int32_t y);

///
/// Meldet fuer jedes Feld den Uebergang von "previous" zum jetzigen Stand, nach
/// derselben Tabelle wie oben. Fuer eine Welt, die als Ganzes ersetzt wurde
/// (RAD_DeserializeGameFromJson): ein Abonnent kennt den alten Stand und erfaehrt
/// so genau die Felder, die anders sind -- und die Zeiger in den Ereignissen
/// zeigen in diese Welt und nicht in einen Zwischenpuffer.
///
/// **Die Groesse darf sich dabei geaendert haben.** previous_width und
/// previous_height sind die Groesse, zu der "previous" gehoert. Verglichen wird
/// ueber beide Rechtecke, und ein Feld ausserhalb einer Welt zaehlt als VOID --
/// es hat kein Gelaende, so wie ein leeres Feld. Damit bleibt es bei der Tabelle:
/// schrumpft die Welt, sind die wegfallenden Felder removed, waechst sie, sind die
/// neuen added. Das Ereignis fuer ein weggefallenes Feld zeigt in den Speicher
/// ausserhalb der Welt; dort steht es mit seinem x und y und dem Typ VOID.
///
void RAD_WorldPublishTileChanges(
    RAD_World_t *world,
    const RAD_Tile_t previous[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH],
    int32_t previous_width,
    int32_t previous_height
);

///
/// Die einzigen drei Funktionen, die eine Entitaetsposition schreiben duerfen.
/// Sie halten RAD_Entity_t.x/y und RAD_Tile_t.entity synchron und sichern damit
/// die Invariante "hoechstens eine Entitaet pro Tile".
///
RAD_EntityId_t RAD_WorldSpawnEntity(RAD_World_t *world, RAD_EntityType_t type, int32_t x, int32_t y);
bool RAD_WorldMoveEntity(RAD_World_t *world, RAD_EntityId_t id, int32_t x, int32_t y);
void RAD_WorldRemoveEntity(RAD_World_t *world, RAD_EntityId_t id);

///
/// Wie RAD_WorldSpawnEntity, aber mit vorgegebener Id statt dem naechsten freien
/// Slot. Wird beim Laden gebraucht, damit gespeicherte Ids erhalten bleiben --
/// nach Loeschungen ist der Pool luecklenhaft und ein Neuvergeben wuerde alle
/// Ids verschieben. Liefert RAD_ENTITY_NONE, wenn die Id ausserhalb des Pools
/// liegt, ihr Slot schon belegt ist oder das Ziel-Tile nicht frei ist.
///
RAD_EntityId_t RAD_WorldSpawnEntityWithId(RAD_World_t *world, RAD_EntityId_t id, RAD_EntityType_t type, int32_t x, int32_t y);

///
/// Besitz einer Entitaet lesen und setzen (RAD_Entity_t.owner).
///
/// Getrennt vom Setzen, statt als Parameter beim Setzen einer Figur: eine Figur
/// entsteht in der Welt, ein Benutzer steht aber nicht in ihr -- wer wem etwas
/// zuordnen darf, entscheidet das Spiel (RAD_GameBindEntity). Die Welt fuehrt das
/// Feld nur.
///
/// Ein Besitzerwechsel haelt keine zweite Angabe synchron und ist deshalb, anders
/// als eine Positionsaenderung, an keine der drei Funktionen oben gebunden.
/// Gesetzt wird ohne Pruefung, ob der Benutzer mitspielt oder die Figur schon
/// jemandem gehoert -- das sind Fragen des Spiels.
///
/// RAD_WorldEntityOwner liefert RAD_USER_NONE fuer eine Entitaet, die es nicht
/// gibt: sie gehoert niemandem, so wie eine herrenlose. RAD_WorldSetEntityOwner
/// liefert false, wenn es sie nicht gibt -- hier ist der Unterschied wichtig,
/// weil sonst ein Zuordnen ins Leere unbemerkt bliebe.
///
RAD_UserId_t RAD_WorldEntityOwner(const RAD_World_t *world, RAD_EntityId_t id);
bool RAD_WorldSetEntityOwner(RAD_World_t *world, RAD_EntityId_t id, RAD_UserId_t owner);

///
/// Prueft die Doppelbuchfuehrung zwischen Tiles und Entitaeten vollstaendig
/// gegeneinander, dazu die eine Zusage ueber den Besitz: ein freier Slot traegt
/// keinen Besitzer. Beim regulaeren Spielverlauf immer true -- interessant nach
/// dem Laden und als Zusicherung im Test.
///
bool RAD_WorldIsConsistent(const RAD_World_t *world);

#endif
