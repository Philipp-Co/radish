#ifndef __RAD_WORLD_H__
#define __RAD_WORLD_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/game_definitions.h>
#include <radish/game/model/model.h>
#include <radish/game/model/tile/tile.h>
#include <radish/game/model/unit/unit.h>
#include <radish/game/model/unit_pool/unit_pool.h>
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
    /// Jede Einheit des Spiels, in jedem Zustand (RAD_UnitState_t) -- im
    /// Einheitenpool (unit_pool/unit_pool.h). **Geliehen:** der Pool gehoert dem
    /// Spiel, das ihn beim Anlegen der Welt hereinreicht und laenger lebt als sie.
    ///
    /// Die Welt legt darin an (RAD_WorldAddReserveUnit, RAD_WorldSpawnUnit) und
    /// fuehrt Zustand und Position, entfernt aber nie: eine zerstoerte Einheit
    /// bleibt mit RAD_UNIT_STATE_DESTROYED stehen. Ihre Id vergibt der Pool, und
    /// er vergibt keine zweimal -- ein Kommando, das eine zerstoerte Einheit nennt,
    /// trifft nie eine andere.
    ///
    /// Wem eine Einheit gehoert und ob sie in einer Reserve steht, fuehren dazu die
    /// Spieler (player/player.h) -- das Spiel haelt beides mit der Welt zusammen.
    ///
    RAD_UnitPool_t *units;

    RAD_EventManager_t *event_manager;
};

///
/// Legt die Welt in "world" an, in der groessten Form (RAD_WORLD_WIDTH x
/// RAD_WORLD_HEIGHT). Solange keine Weltdefinition geladen ist, ist das die Welt
/// des Spiels. Gefuellt wird sie erst mit RAD_InitWorld.
///
/// In den Speicher des Aufrufers und nicht als Rueckgabewert: das Raster macht die
/// Welt gross genug, dass eine Kopie bei jeder Erzeugung ein Aufrufrahmen waere,
/// den niemand will. "units" ist der Einheitenpool, den die Welt sich leiht
/// (oben); er muss laenger leben als sie.
///
void RAD_CreateWorld(RAD_World_t *world, RAD_EventManager_t *event_manager, RAD_UnitPool_t *units);

///
/// Bringt das Raster in den Grundzustand: jedes Feld Boden auf Hoehe 0, auf
/// keinem eine Einheit.
///
/// **Den Pool fasst das nicht an** -- er gehoert dem Spiel, und Spieler zeigen
/// hinein. Zurueckgesetzt wird deshalb nur, solange keine Einheit auf dem Feld
/// steht; das Laden einer Weltdefinition lehnt eine Welt mit Einheiten ohnehin ab
/// (RAD_SERIALIZE_ERROR_WORLD_OCCUPIED). Steht doch eine dort, zeigt sie danach
/// auf ein Feld, das nicht auf sie zurueckzeigt (RAD_WorldIsConsistent).
///
/// RAD_InitWorld meldet danach jedes Feld als added -- der Aufbau einer Welt,
/// die es vorher nicht gab. RAD_ResetWorld tut dasselbe still. Es ist fuer den
/// Fall, in dem der Grundzustand nur ein Zwischenschritt ist, wie beim Laden einer
/// Weltdefinition (world_definition.c): dort wuerde sonst jedes Feld als Boden
/// gemeldet, der gleich darauf ueberschrieben wird. Wer still zuruecksetzt, meldet
/// danach selbst, was sich geaendert hat (RAD_WorldPublishTileChanges).
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
RAD_Unit_t* RAD_WorldUnitById(RAD_World_t *world, RAD_UnitId_t id);
RAD_Unit_t* RAD_WorldUnitAt(RAD_World_t *world, int32_t x, int32_t y);

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
/// **Entfernen laesst die Stelle stehen:** x, y, z und die Einheit bleiben, nur
/// der Typ wird VOID. Weggenommen wird das Gelaende und nicht das Feld -- und wer
/// es wieder hinstellt, bringt seine Hoehe selbst mit.
///
/// **Eine Figur haelt ihr Gelaende.** RAD_WorldRemoveTile liefert false, solange
/// eine Einheit auf dem Feld steht, und schreibt nichts. Es ist der einzige Fall,
/// in dem eine Aenderung abgelehnt wird, obwohl sie sich hinschreiben liesse:
/// was mit einer Figur ueber dem Nichts geschieht -- fallen, stehenbleiben,
/// sterben --, ist eine Regel, die es noch nicht gibt, und die Welt erfindet sie
/// nicht still. Wer es trotzdem will, nimmt zuerst die Figur
/// (RAD_WorldRemoveUnit).
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
/// (RAD_DeserializeWorldDefinitionFromJson): ein Abonnent kennt den alten Stand und erfaehrt
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
/// Legt eine Einheit in der Reserve an: sie ist dem Spiel bekannt, steht aber auf
/// keinem Feld. So kommt eine Einheit aus der Armee ins Spiel.
///
/// Uebernommen werden aus "values" die Werte der Einheit -- type, name, movement,
/// transport_capacity, can_capture, attributes und die Mitglieder. Id, Zustand,
/// Besitzer und Position vergibt die Welt: die Id der Pool, der
/// Zustand RAD_UNIT_STATE_RESERVE, der Besitzer "owner", die Position (-1,-1). Ein
/// Ereignis gibt es nicht -- auf dem Feld hat sich nichts geaendert.
///
/// RAD_UNIT_NONE, wenn der Pool voll ist oder "values" sich nicht halten laesst:
/// NULL, keine oder zu viele Mitglieder, zu viele Waffen bei einem davon, oder ein
/// Name ohne abschliessende Null in seinem Feld. Die Welt aendert sich dann nicht.
///
RAD_UnitId_t RAD_WorldAddReserveUnit(RAD_World_t *world, const RAD_Unit_t *values, RAD_UserId_t owner);

///
/// Die einzigen vier Funktionen, die eine Einheitenposition schreiben duerfen.
/// Sie halten RAD_Unit_t.x/y und RAD_Tile_t.unit synchron und sichern damit
/// die Invariante "hoechstens eine Einheit pro Tile".
///
/// RAD_WorldSpawnUnit legt eine Einheit direkt auf dem Feld an, ohne Werte aus
/// einer Armee -- fuer Figuren, die keiner Armee angehoeren.
///
/// RAD_WorldDeployUnit holt eine Einheit aus der Reserve auf das Feld (x, y) und
/// meldet sie wie ein Spawn (RAD_OnUnitSpawned_t). false und keine Aenderung, wenn
/// es die Einheit nicht gibt, sie nicht in der Reserve steht oder das Feld nicht
/// taugt: ausserhalb der Welt, ohne Gelaende (RAD_TILE_TYPE_VOID) oder besetzt.
///
/// RAD_WorldMoveUnit bewegt nur eine Einheit auf dem Feld.
///
/// RAD_WorldRemoveUnit zerstoert eine Einheit: vom Feld genommen, mit
/// RAD_OnUnitDestroyed_t; aus der Reserve gestrichen, ohne Ereignis -- auf dem
/// Feld hat sie nie gestanden. Sie bleibt im Pool (RAD_UNIT_STATE_DESTROYED),
/// und eine schon zerstoerte Einheit noch einmal zu entfernen tut nichts.
///
RAD_UnitId_t RAD_WorldSpawnUnit(RAD_World_t *world, RAD_UnitType_t type, int32_t x, int32_t y);
bool RAD_WorldDeployUnit(RAD_World_t *world, RAD_UnitId_t id, int32_t x, int32_t y);
bool RAD_WorldMoveUnit(RAD_World_t *world, RAD_UnitId_t id, int32_t x, int32_t y);
void RAD_WorldRemoveUnit(RAD_World_t *world, RAD_UnitId_t id);

///
/// Besitz einer Einheit lesen und setzen (RAD_Unit_t.owner).
///
/// Getrennt vom Setzen, statt als Parameter beim Setzen einer Figur: eine Figur
/// entsteht in der Welt, ein Benutzer steht aber nicht in ihr -- wer wem etwas
/// zuordnen darf, entscheidet das Spiel (RAD_GameBindUnit). Die Welt fuehrt das
/// Feld nur.
///
/// Ein Besitzerwechsel haelt keine zweite Angabe synchron und ist deshalb, anders
/// als eine Positionsaenderung, an keine der vier Funktionen oben gebunden.
/// Gesetzt wird ohne Pruefung, ob der Benutzer mitspielt oder die Figur schon
/// jemandem gehoert -- das sind Fragen des Spiels.
///
/// RAD_WorldUnitOwner liefert RAD_USER_NONE fuer eine Einheit, die es nicht
/// gibt: sie gehoert niemandem, so wie eine herrenlose. RAD_WorldSetUnitOwner
/// liefert false, wenn es sie nicht gibt -- hier ist der Unterschied wichtig,
/// weil sonst ein Zuordnen ins Leere unbemerkt bliebe.
///
RAD_UserId_t RAD_WorldUnitOwner(const RAD_World_t *world, RAD_UnitId_t id);
bool RAD_WorldSetUnitOwner(RAD_World_t *world, RAD_UnitId_t id, RAD_UserId_t owner);

///
/// Prueft die Doppelbuchfuehrung zwischen Tiles und Einheiten vollstaendig
/// gegeneinander: eine Einheit auf dem Feld steht auf einem Tile, das auf sie
/// zurueckzeigt; eine in der Reserve oder zerstoerte steht auf keinem und traegt
/// (-1,-1). Dazu die Zusage ueber den Pool: keine Einheit hat mehr Mitglieder
/// oder Waffen, als Platz ist.
/// Beim regulaeren Spielverlauf immer true -- eine Zusicherung im Test.
///
bool RAD_WorldIsConsistent(const RAD_World_t *world);

#endif
