#ifndef __RAD_GAME_UNIT_H__
#define __RAD_GAME_UNIT_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/model/model.h>
#include <radish/game/user.h>

///
/// Eine Einheit, wie sie im Pool der Welt steht.
///
/// Oeffentlich, weil eine Figur aus dem Spiel herauskommt: RAD_OnUnitSpawned_t
/// und seine Nachbarn (event_manager.h) reichen ein const RAD_Unit_t* an ihre
/// Abonnenten. Der Pool, in dem sie steht, bleibt privat -- wer eine Figur setzt,
/// bewegt oder entfernt, geht ueber die Welt (world.h), denn nur sie haelt x/y und
/// RAD_Tile_t.unit synchron.
///
/// Ein Kommando braucht die Struktur dagegen nicht: es benennt eine Figur mit
/// ihrer RAD_UnitId_t (model.h), ohne in ihr zu lesen.
///
/// **Der Name steht hier und nicht in model.h**, wie bei Tile und aus demselben
/// Grund: dort stehen nur die Strukturen, deren Inhalt nicht heraus soll. Wer
/// RAD_Unit_t nennt, bindet diese Datei ein.
///
/// **Und mit dem Namen der Zugriff.** Unten stehen die vier Funktionen, mit denen
/// ein Aufrufer von aussen an die Figuren kommt -- alle Figuren des Spiels und die
/// eines einzelnen Benutzers. Sie heissen RAD_Game*, weil sie ein Spiel nehmen,
/// gehoeren aber zur Figur und stehen deshalb hier und nicht in game.h: wer Figuren
/// lesen will, braucht genau eine Datei dafuer.
///
/// **Was eine Einheit ist, kommt aus der Armee.** Die Werte stehen so in ihr, wie
/// das Backend sie beim Spielstart uebergibt (game/schema/spielstart.schema.json):
/// der Einheitentyp mit Bewegung und Transport, darunter ihre Mitglieder mit
/// Profil, Werten und Waffen. Das Backend nennt ein Mitglied "Entitaet"; hier ist
/// es ein RAD_UnitMember_t, denn auf dem Feld steht die Einheit und nicht ihr
/// Mitglied.
///
/// **Alles in festen Feldern, nichts auf dem Heap.** Eine Einheit wird als Ganzes
/// kopiert -- in den Pool, aus ihm heraus (RAD_GameUnitAt) und in die Ereignisse --,
/// und ein Zeiger darin wuerde bei jeder Kopie mitwandern. Die Grenzen unten
/// bemessen den Platz; was nicht hineinpasst, weist die Welt beim Anlegen ab
/// (RAD_WorldAddReserveUnit), statt es abzuschneiden.
///

///
/// Laenge eines Namens samt abschliessender Null: Einheitentyp, Profil, Waffe. Das
/// Backend erlaubt 255 Zeichen; im Spiel reichen 31 -- ein laengerer Name ist ein
/// Fehler beim Einlesen und wird nicht gekuerzt.
///
#define RAD_UNIT_NAME_MAX 32

///
/// Mitglieder einer Einheit, wie MAX_UNIT_ENTITIES im Backend (api/models.py).
///
#define RAD_UNIT_MAX_MEMBERS 10

///
/// Waffen eines Mitglieds, gleich welcher Klasse. Das Backend begrenzt sie nur ueber
/// die Slots des Profils; vier deckt die heutigen Profile ab, und das Backend muss
/// die Grenze beim Zusammenstellen einer Armee mitpruefen.
///
#define RAD_UNIT_MAX_WEAPONS 4

///
/// Waffenklasse, wie WeaponClass im Backend.
///
typedef enum
{
    RAD_WEAPON_CLASS_STANDARD = 0,
    RAD_WEAPON_CLASS_HEAVY,
    RAD_WEAPON_CLASS_SUPER_HEAVY
} RAD_WeaponClass_t;

typedef struct
{
    char name[RAD_UNIT_NAME_MAX];
    RAD_WeaponClass_t weapon_class;
    int16_t shots;
    int16_t strength;

    ///
    /// Unterhalb von min_range feuert die Waffe nicht, 0 heisst ohne Mindestweite.
    /// min_range <= max_range prueft das Backend.
    ///
    int16_t min_range;
    int16_t max_range;

    /// Panzerungsdurchschlag.
    int16_t penetration;
} RAD_Weapon_t;

///
/// Ein Mitglied einer Einheit -- im Backend eine "Entitaet" mit ihrem Profil.
///
typedef struct
{
    char profile[RAD_UNIT_NAME_MAX];
    int16_t health;
    int16_t armor;
    int16_t strength;
    int16_t accuracy;

    RAD_Weapon_t weapons[RAD_UNIT_MAX_WEAPONS];
    int32_t number_of_weapons;
} RAD_UnitMember_t;

///
/// Wo eine Einheit in ihrem Leben steht.
///
///     RESERVE ──► DEPLOYED ──► DESTROYED
///        │                         ▲
///        └─────────────────────────┘
///
/// RESERVE: dem Spiel bekannt, aber auf keinem Feld -- so kommt eine Einheit aus
/// der Armee ins Spiel (RAD_WorldAddReserveUnit). DEPLOYED: auf dem Feld, das in
/// x/y steht (RAD_WorldDeployUnit, oder direkt mit RAD_WorldSpawnUnit).
/// DESTROYED: vom Feld genommen oder aus der Reserve gestrichen
/// (RAD_WorldRemoveUnit). Zurueck geht es von keinem Zustand.
///
typedef enum
{
    RAD_UNIT_STATE_RESERVE = 0,
    RAD_UNIT_STATE_DEPLOYED,
    RAD_UNIT_STATE_DESTROYED
} RAD_UnitState_t;

typedef struct RAD_Unit RAD_Unit_t;

struct RAD_Unit
{
    ///
    /// Eigene Id, zugleich der Slot-Index im Pool. RAD_UNIT_NONE markiert
    /// einen freien Slot. Eine Id wird nur einmal vergeben: auch eine zerstoerte
    /// Einheit behaelt ihren Slot, damit ein Kommando, das sie nennt, nie eine
    /// andere trifft.
    ///
    RAD_UnitId_t id;
    RAD_UnitType_t type;
    RAD_UnitState_t state;

    ///
    /// Wem die Einheit gehoert, RAD_USER_NONE fuer herrenlos. Sie steht damit in
    /// der Einheit und nicht in einer Liste je Benutzer -- eine Liste daneben
    /// muesste bei jedem Anlegen und Entfernen mitgezogen werden. Eine zerstoerte
    /// Einheit behaelt ihren Besitzer; nur ein freier Slot hat keinen.
    ///
    /// Herrenlos ist der Normalfall und kein Fehler -- alles, was nicht
    /// ausdruecklich zugeordnet wurde, gehoert niemandem.
    ///
    RAD_UserId_t owner;

    ///
    /// Tile, auf dem die Einheit steht -- nur bei RAD_UNIT_STATE_DEPLOYED, sonst
    /// beide -1. Immer synchron zu world->tiles[y][x].unit; beide Seiten werden
    /// ausschliesslich von den vier Funktionen in world.h fortgeschrieben, die eine
    /// Position schreiben.
    ///
    int16_t x;
    int16_t y;

    ///
    /// Der Einheitentyp aus der Armee, z.B. "Trupp", und seine Werte.
    ///
    char name[RAD_UNIT_NAME_MAX];
    int16_t movement;
    int16_t transport_capacity;
    bool can_capture;

    RAD_UnitMember_t members[RAD_UNIT_MAX_MEMBERS];
    int32_t number_of_members;

    struct 
    {
        uint32_t movable: 1;
        uint32_t collision: 1;
        uint32_t draw: 1;
    } attributes;
    struct
    {
        uint32_t burning: 1;
        uint32_t poisoned: 1;
    } conditions;

    ///
    /// Was die Einheit im laufenden Zug schon getan hat. Eine Einheit darf je
    /// Zug ihres Besitzers einmal ziehen und einmal angreifen, unabhaengig
    /// voneinander; wurde sie in diesem Zug aufgestellt, darf sie keins von
    /// beiden (RAD_GameCheckMoveUnit, RAD_GameCheckAttack).
    ///
    /// Gesetzt wird beim Ausfuehren des Kommandos -- "moved" nur, wenn sie
    /// wenigstens ein Feld weit gekommen ist --, zurueckgesetzt fuer alle
    /// Einheiten mit jedem Zugwechsel (RAD_GameEndTurn). Eine neue Einheit
    /// faengt mit allem frei an.
    ///
    struct
    {
        uint32_t deployed: 1;
        uint32_t moved: 1;
        uint32_t attacked: 1;
    } turn;
};

///
/// Die Figuren eines Spiels zum Nachlesen: erst zaehlen, dann einzeln holen.
/// Dasselbe Muster wie bei den Tiles (tile.h) und in turn.h.
///
/// **Gezaehlt wird jede Einheit, die das Spiel kennt** -- in der Reserve, auf dem
/// Feld und zerstoert. Wer nur das Feld will, fragt RAD_Unit_t.state.
///
/// **Der Index ist nicht die Id.** Der Pool der Welt kann Luecken haben. Gezaehlt
/// und geholt wird dicht ab 0 ueber die belegten Plaetze, aufsteigend nach Id. Wer die Id braucht, liest sie aus der Figur
/// (RAD_Unit_t.id); wer eine bestimmte sucht, laeuft ueber die Anzahl und
/// vergleicht. Ein Index gilt nur, solange keine Figur dazukommt oder wegfaellt --
/// danach kann derselbe Index eine andere treffen.
///
/// **Herausgegeben wird kopiert.** RAD_GameUnitAt schreibt einen Stand in den
/// Speicher des Aufrufers und gibt keinen Zeiger in die Welt heraus: ein Zeiger
/// liesse sich an den Regeln vorbei beschreiben und wuerde baumeln, sobald sich
/// die Welt weiterdreht. Die ausfuehrliche Begruendung steht in tile.h.
///

///
/// Wie viele Einheiten es gibt, in jedem Zustand -- ein freier Platz im Pool ist
/// keine. 0 fuer ein NULL-Spiel.
///
int32_t RAD_GameNumberOfUnits(const RAD_Game_t *game);

///
/// Holt die Figur an dieser Stelle. Liefert false, wenn "game" oder "output" NULL
/// ist oder der Index ausserhalb [0, RAD_GameNumberOfUnits) liegt. **"output"
/// bleibt dann unangetastet** -- es wird nichts halb hineingeschrieben.
///
bool RAD_GameUnitAt(const RAD_Game_t *game, int32_t index, RAD_Unit_t *output);

///
/// Dieselben zwei Fragen, eingeschraenkt auf die Einheiten eines Benutzers: erst
/// zaehlen, dann einzeln holen. Das ist zugleich seine Einheitenliste -- die
/// Einheiten in der Reserve stehen darin genauso wie die auf dem Feld.
///
/// RAD_GameUserUnitAt liefert die **Id** und nicht die Figur -- anders als
/// RAD_GameUnitAt oben, und das ist gewachsen und nicht entworfen: es gab die
/// Funktion, bevor es einen Getter mit Ausgabezeiger gab. Wer die Figur selbst
/// will, holt sich mit der Id ihren Platz ueber die Anzahl. Sie liefert
/// RAD_UNIT_NONE fuer einen Index ausserhalb
/// [0, RAD_GameNumberOfUserUnits).
///
/// Gezaehlt wird auch hier in der Reihenfolge der Ids, ein Index gilt also,
/// solange keine Figur dazukommt oder wegfaellt. Beides laeuft ueber den
/// Einheitenpool -- bei hoechstens RAD_MAX_UNITS Plaetzen ist jede
/// Beschleunigung teurer als die Suche, und eine Liste daneben waere ein zweites
/// Buch (siehe RAD_Unit_t.owner oben).
///
int32_t RAD_GameNumberOfUserUnits(const RAD_Game_t *game, RAD_UserId_t user);
RAD_UnitId_t RAD_GameUserUnitAt(const RAD_Game_t *game, RAD_UserId_t user, int32_t index);

#endif
