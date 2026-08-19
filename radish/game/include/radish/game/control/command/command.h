#ifndef __RAD_COMMAND_H__
#define __RAD_COMMAND_H__

#include <stdint.h>
#include <radish/game/model/model.h>
#include <radish/game/model/path/path.h>
#include <radish/game/user.h>

///
/// control/command/ -- ein Kommando ist ein Anlass in Datenform: die Absicht,
/// den Spielzustand zu aendern, ohne ihn schon zu aendern. Wer ein Kommando
/// erzeugt (Eingabe im Client, eine Nachricht vom Server), muss die Spiellogik
/// nicht kennen; wer es ausfuehrt, muss nicht wissen, woher es kam. Umgesetzt
/// wird es ueber die Schnittstellen von game/ -- die Stelle, die das tut, gibt es
/// noch nicht.
///
/// Weil ein Kommando nur Daten sind, laesst es sich puffern, verschicken,
/// wiederholen und in der Reihenfolge pruefen -- dafuer traegt jedes seine
/// Sequenznummer.
///
/// Diese Datei sind nur die Typen und zieht keine Implementierung nach sich. Die
/// Uebersetzung auf die Strecke liegt daneben: codec.h beschreibt das Format und
/// den Kopf, dazu je eine Datei pro Kommandoart fuer ihre Nutzlast. Beides gehoert
/// zu radish_game.
///

///
/// Fortlaufende Nummer eines Kommandos, vom Erzeuger vergeben. Sie ordnet die
/// Kommandos einer Quelle und macht Luecken und Wiederholungen erkennbar --
/// beides braucht es, sobald Kommandos ueber eine Verbindung laufen, die
/// umsortieren oder doppelt zustellen kann.
///
/// "Eine Quelle" ist ein Benutzer: die Nummer zaehlt je "user" im Kopf, nicht
/// ueber alle Absender zusammen. Zwei Benutzer duerfen dieselbe 7 schicken, und
/// erst beide Felder zusammen benennen ein Kommando eindeutig.
///
/// Vorzeichenlos, weil eine Sequenz nur waechst; RAD_COMMAND_SEQUENCE_NONE
/// markiert "noch nicht vergeben", die erste echte Nummer ist also 1.
///
typedef uint64_t RAD_CommandSequence_t;


typedef enum
{
    RAD_COMMAND_TYPE_NONE = 0,
    RAD_COMMAND_TYPE_SPAWN_ENTITY,
    RAD_COMMAND_TYPE_MOVE_ENTITY,
    RAD_COMMAND_TYPE_REMOVE_ENTITY,
    RAD_COMMAND_TYPE_CREATE_TILE,
    RAD_COMMAND_TYPE_REMOVE_TILE,
    RAD_COMMAND_TYPE_SHOOT,
    RAD_COMMAND_TYPE_USE,

    ///
    /// Der Absender gibt seinen Zug ab. Als einzige Art ohne Nutzlast: wer ihn
    /// beendet, steht im Kopf, und mehr ist nicht zu sagen. Es gibt deshalb auch
    /// keine Struktur dafuer in der Union unten.
    ///
    /// Ein Zug endet auch von selbst, sobald die Aktionspunkte aufgebraucht sind
    /// (RAD_ControlExecuteCommand im Server). Dieses Kommando ist der andere Weg:
    /// abgeben, was man nicht mehr braucht.
    ///
    RAD_COMMAND_TYPE_END_TURN
} RAD_CommandType_t;

///
/// Gemeinsamer Kopf jedes Kommandos. Er liegt in RAD_Command_t vor der Union und
/// nicht darin: so tragen Kopf und Nutzlast gleichzeitig, und "type" darf immer
/// gelesen werden, gleich welche Variante zuletzt geschrieben wurde. Darauf
/// beruht das Muster "erst type lesen, dann die passende Variante", so wie es
/// SDL_Event vormacht.
///
/// Der Platz, den der Kopf dadurch kostet statt sich mit der Nutzlast zu
/// ueberlappen, ist genau das, was den Umgang mit einem Kommando unfaellig
/// macht: eine Variante zu fuellen kann den Kopf nicht loeschen.
///
typedef struct
{
    RAD_CommandType_t type;
    RAD_CommandSequence_t sequence;

    ///
    /// Wer das Kommando geschickt hat.
    ///
    /// Im Kopf und nicht in den einzelnen Kommandoarten, weil jedes Kommando
    /// einen Absender hat -- und zwar dieselbe Frage beantwortet: darf der, der
    /// das schickt, das auch? Wer sie beantwortet, ist der Server
    /// (radish/server/control/execute.h); die Angaben dazu holt er aus dem Spiel,
    /// das weiss, wer mitspielt, wer dran ist und wem welche Figur gehoert.
    ///
    /// Es geht auch nicht anders herum: aus der Nachricht selbst ist der
    /// Absender nicht zu erfahren. Das Codefeld, mit dem Zucchini den Rueckweg
    /// kennt, schneidet zucchini_server ab, bevor der Server die Nutzlast sieht.
    ///
    /// RAD_USER_NONE heisst "ohne Absender". Der Codec laesst das durch (siehe
    /// codec.h); was ein Kommando ohne Absender wert ist, entscheidet der, der es
    /// ausfuehren soll.
    ///
    RAD_UserId_t user;
} RAD_CommandHeader_t;

///
/// Setzt eine neue Entitaet auf ein Tile. Ohne Id: die vergibt die Welt beim
/// Ausfuehren (RAD_WorldSpawnEntity).
///
typedef struct
{
    RAD_EntityType_t entity_type;
    int16_t x;
    int16_t y;
    int8_t z;
} RAD_CommandSpawnEntity_t;

///
/// Bewegt eine vorhandene Entitaet -- nicht auf ein Feld, sondern ueber einen Weg
/// aus mehreren. Eine Bewegung ist ein Pfad (model/path/path.h), und wie lang er
/// hoechstens sein darf, ist die Laenge seines Feldes: RAD_PATH_MAX_STEPS.
///
/// **Wo es losgeht, steht nicht darin.** Startfeld ist das Tile, auf dem die
/// genannte Figur steht -- sie weiss das selbst (RAD_Entity_t.x/y), und wer das
/// Kommando ausfuehrt, schlaegt es ueber die Id nach. path.steps_to[0] ist damit
/// das erste Feld, auf das sie sich bewegt, der letzte Eintrag ihr Ziel.
///
/// Jeder Schritt ist ein Feld in Weltkoordinaten und keine Verschiebung. Das ist
/// die Festlegung von path.h, und sie galt hier schon, als es nur ein Ziel gab.
///
/// **Zur Wiederholung ist dieses Kommando nicht mehr aus sich heraus dasselbe.**
/// Solange das Startfeld darin stand, fuehrte ein zweimal zugestelltes Kommando
/// zum Ergebnis eines einmal zugestellten. Mit dem Startfeld ist das
/// weggefallen: steht die Figur schon am Ziel, beschreibt derselbe Weg von dort
/// aus einen anderen. Erkennbar bleibt eine Wiederholung an der Sequenznummer im
/// Kopf, und aufzuhalten ist sie an steps_to[0], das von der neuen Position aus
/// nicht mehr zu erreichen ist -- beides Sache dessen, der ausfuehrt, und nicht
/// des Formats.
///
/// Was einen Weg begehbar macht -- ob die Schritte aneinandergrenzen, ob die
/// Felder frei und betretbar sind, was er kostet --, prueft das Kommando nicht.
/// Es benennt eine Absicht, und jede Folge von Feldern ist eine lesbare.
///
typedef struct
{
    RAD_EntityId_t entity;
    RAD_EntityPath_t path;
} RAD_CommandMoveEntity_t;

///
/// Nimmt eine Entitaet aus der Welt.
///
typedef struct
{
    RAD_EntityId_t entity;
} RAD_CommandRemoveEntity_t;

///
/// Legt Gelaende auf einem Tile an. "Anlegen" heisst hier nicht, dass ein Tile
/// entsteht -- das Raster der Welt steht fest, an jedem (x,y) innerhalb der
/// Grenzen liegt immer ein RAD_Tile_t. Gemeint ist sein Gelaende: aus
/// RAD_TILE_TYPE_VOID -- der Zustand "hier ist nichts" -- wird begehbarer oder
/// bespielbarer Grund.
///
/// Damit ist das Kommando zugleich das zum Aendern von Gelaende: es setzt den
/// Typ, gleich was vorher dastand. Ein eigenes Change-Kommando gaebe es nicht
/// mehr her, und Anlegen und Aendern getrennt zu fuehren hiesse, dass der
/// Erzeuger den bisherigen Zustand kennen muesste.
///
/// "z" ist die Hoehe des Tiles; sie geht in die isometrische Darstellung ein
/// (RAD_MapAddIsoObject). 0 ist die Grundebene.
///
typedef struct
{
    RAD_TileType_t tile_type;
    int16_t x;
    int16_t y;
    int8_t z;
} RAD_CommandCreateTile_t;

///
/// Nimmt das Gelaende von einem Tile: es faellt auf RAD_TILE_TYPE_VOID zurueck.
/// Das Tile selbst bleibt im Raster -- entfernt wird, was darauf liegt, nicht
/// der Platz.
///
/// Kein Tile-Typ als Feld: es gibt nur einen Weg, nichts zu sein.
///
typedef struct
{
    int16_t x;
    int16_t y;
} RAD_CommandRemoveTile_t;

///
/// Schiesst auf ein Tile.
///
/// Das Ziel ist ein Feld und keine Figur: getroffen wird, was dort steht, und ob
/// dort etwas steht, entscheidet sich beim Ausfuehren und nicht beim Zielen. Ein
/// Schuss ins Leere ist damit ein moegliches Kommando und kein fehlerhaftes --
/// dieselbe Ueberlegung wie beim absoluten Ziel von move_entity.
///
typedef struct
{
    ///
    /// Wer schiesst. Der Absender im Kopf sagt, in wessen Namen -- diese Id sagt,
    /// mit welcher seiner Figuren; ein Benutzer kann mehrere fuehren, und ohne
    /// die Angabe waere nicht entschieden, welche handelt.
    ///
    /// Damit ist es auch die Figur, an der die Berechtigung haengt: sie muss dem
    /// Absender gehoeren, so wie bei move_entity.
    ///
    RAD_EntityId_t entity;

    int16_t x;
    int16_t y;

    ///
    /// Womit geschossen wird -- eine Nummer, keine Aufzaehlung. Waffen gibt es im
    /// Spiel noch nicht; bis es sie gibt, ist das eine Zahl, die der Codec
    /// durchreicht, ohne sie zu deuten, so wie die Uuid des Absenders. Wer sie
    /// ausgibt und was 0 heisst, entscheidet, wer das Kommando ausfuehrt.
    ///
    uint8_t weapon;
} RAD_CommandShoot_t;

///
/// Benutzt, was auf einem Tile steht -- eine Tuer, einen Schalter, was dort
/// aufliegt.
///
/// Wie beim Schuss ist das Ziel ein Feld: was dort benutzt wird, weiss das
/// Kommando nicht. Wer benutzt, steht dagegen darin -- aus demselben Grund wie
/// dort.
///
/// Ohne eine Angabe, *was* getan werden soll: ein Feld traegt hoechstens eine
/// Sache, und was sie kann, weiss sie selbst. Zwei Dinge auf einem Feld
/// auseinanderzuhalten waere eine Angabe mehr -- die kommt hinzu, wenn es sie
/// gibt.
///
typedef struct
{
    /// Wer benutzt; muss dem Absender gehoeren, wie bei shoot.
    RAD_EntityId_t entity;

    int16_t x;
    int16_t y;
} RAD_CommandUse_t;

typedef struct
{
    RAD_CommandHeader_t header;
    union
    {
        RAD_CommandSpawnEntity_t spawn_entity;
        RAD_CommandMoveEntity_t move_entity;
        RAD_CommandRemoveEntity_t remove_entity;
        RAD_CommandCreateTile_t create_tile;
        RAD_CommandRemoveTile_t remove_tile;
        RAD_CommandShoot_t shoot;
        RAD_CommandUse_t use;
    } command;
} RAD_Command_t;

///
/// Antwort auf ein Kommando. Der Kopf ist der des Kommandos, auf das sie
/// antwortet -- nur daran erkennt der Absender, worauf sie geht.
///
/// Dahinter liegt das Kommando noch einmal als Ganzes. Damit ist die Antwort aus
/// sich heraus lesbar: wer sie bekommt, muss nicht nachhalten, was er unter
/// welcher Sequenznummer verschickt hat, um zu wissen, was beantwortet wurde. Und
/// weil es ein vollstaendiges RAD_Command_t ist, liesse sich das Kommando aus der
/// Antwort unveraendert ausfuehren -- der Rueckweg kann damit tragen, was der
/// Ausfuehrende am Kommando zurechtgelegt hat, statt nur ja oder nein. Der Server
/// nutzt das nicht aus: er schickt eine genaue Kopie zurueck und legt alles, was
/// er zu sagen hat, in "value".
///
/// "header" und "command.header" tragen dasselbe. Hergestellt wird das an einer
/// Stelle (RAD_ControlExecuteCommand im Server), geprueft beim Lesen
/// (RAD_COMMAND_CODEC_ERROR_HEADER_MISMATCH); dazwischen ist es Invariante.
///
typedef struct
{
    RAD_CommandHeader_t header;
    uint32_t value;
    RAD_Command_t command;
} RAD_CommandResponse_t;

#endif
