#ifndef __RAD_COMMAND_H__
#define __RAD_COMMAND_H__

#include <stdint.h>
#include <radish/game/entity.h>
#include <radish/game/tile.h>
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
/// Bewegt eine vorhandene Entitaet auf ein Tile. x/y sind das Ziel in
/// Weltkoordinaten, nicht die Verschiebung -- ein wiederholt zugestelltes
/// Kommando fuehrt so zum selben Ergebnis wie ein einmal zugestelltes.
///
typedef struct
{
    RAD_EntityId_t entity;
    int16_t from_x;
    int16_t from_y;
    int16_t to_x;
    int16_t to_y;
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
