#ifndef __RAD_MODEL_UNIT_H__
#define __RAD_MODEL_UNIT_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/model/events/observable.h>

///
/// Was der Client von einer Einheit weiss -- und nur das.
///
/// **Ein Abbild, keine Simulation**, wie die Welt (model/world.h): gefuellt wird
/// es aus dem, was der Server schickt; Regeln wendet es keine an.
///
/// **Eine Einheit besteht aus Mitgliedern.** Auf dem Feld steht die Einheit, mit
/// ihrer Id (RAD_ClientUnitId_t, im Tile als entity_id); ihre Mitglieder haben
/// keine eigene Id und stehen nur in ihr. Derselbe Begriff wie im Spiel
/// (RAD_UnitMember_t, game/model/unit/unit.h); das Backend nennt ein Mitglied
/// "Entitaet".
///
/// **Alles in festen Feldern, nichts auf dem Heap.** Die Grenzen sind dieselben
/// wie im Spiel; was dort nicht hineinpasst, schickt der Server nicht.
///
/// **Beobachtbar** (model/events/observable.h): eine Einheit meldet "changed",
/// sobald sich an ihr etwas aendert -- auch an einem ihrer Mitglieder, die selbst
/// nicht beobachtbar sind --, und "removed", wenn der, der sie haelt, sie
/// entfernt (RAD_ClientUnitRemove).
///
/// **Nicht per Zuweisung ersetzen.** "*a = *b" ueberschriebe die Beobachter von a
/// mit denen von b und meldete nichts. Eine Einheit, die schon jemand beobachten
/// kann, wird mit RAD_ClientUnitAssign ersetzt.
///
/// **Unabhaengig von der Verbindung.** Das Modell hat eigene Typen und Grenzen;
/// sie entsprechen denen in io/net_types.h, uebersetzt wird beim Befuellen.
///

///
/// Die Id einer Einheit. Ids sind nicht negativ, die 0 eingeschlossen;
/// RAD_CLIENT_UNIT_ID_NONE heisst: keine Einheit.
///
typedef int32_t RAD_ClientUnitId_t;
#define RAD_CLIENT_UNIT_ID_NONE ((RAD_ClientUnitId_t)-1)

///
/// Die oeffentliche Id eines Spielers; RAD_CLIENT_PLAYER_NONE heisst: niemand.
///
typedef uint64_t RAD_ClientPlayerId_t;
#define RAD_CLIENT_PLAYER_NONE ((RAD_ClientPlayerId_t)0)

///
/// Platz fuer einen Namen (Einheit, Profil, Waffe), mit abschliessender Null --
/// wie RAD_UNIT_NAME_MAX im Spiel. Ein laengerer wird gekuerzt.
///
#define RAD_CLIENT_UNIT_NAME_MAX 32

///
/// Mitglieder einer Einheit, wie RAD_UNIT_MAX_MEMBERS im Spiel.
///
#define RAD_CLIENT_UNIT_MEMBERS_MAX 10

///
/// Waffen eines Mitglieds, wie RAD_UNIT_MAX_WEAPONS im Spiel.
///
#define RAD_CLIENT_MEMBER_WEAPONS_MAX 4

typedef struct
{
    char name[RAD_CLIENT_UNIT_NAME_MAX];
    int16_t shots;
    int16_t strength;

    /// 0 heisst ohne Mindestweite.
    int16_t min_range;
    int16_t max_range;

    /// Panzerungsdurchschlag.
    int16_t penetration;
} RAD_ClientWeapon_t;

///
/// Ein Mitglied einer Einheit, mit seinem Profil.
///
typedef struct
{
    char profile[RAD_CLIENT_UNIT_NAME_MAX];

    /// Lebenspunkte: was es noch hat und was es hoechstens hat.
    int16_t health;
    int16_t health_max;

    int16_t armor;
    int16_t strength;
    int16_t accuracy;

    RAD_ClientWeapon_t weapons[RAD_CLIENT_MEMBER_WEAPONS_MAX];
    uint32_t number_of_weapons;
} RAD_ClientMember_t;

typedef struct
{
    RAD_ClientUnitId_t id;

    /// Oeffentliche Spieler-Id des Besitzers, RAD_CLIENT_PLAYER_NONE fuer herrenlos.
    RAD_ClientPlayerId_t owner;

    /// Der Einheitentyp, z.B. "Trupp".
    char name[RAD_CLIENT_UNIT_NAME_MAX];

    /// Bewegungsreichweite in Feldern.
    int16_t movement;

    ///
    /// Was sie im laufenden Zug schon getan hat, wie der Server es meldet: in
    /// diesem Zug aufgestellt -- dann darf sie weder ziehen noch angreifen --,
    /// gezogen, angegriffen; beides darf sie je Zug einmal. Mit jedem Zugwechsel
    /// schickt der Server sie wieder ohne.
    ///
    bool deployed;
    bool moved;
    bool attacked;

    RAD_ClientMember_t members[RAD_CLIENT_UNIT_MEMBERS_MAX];
    uint32_t number_of_members;

    /// Wer die Einheit beobachtet -- nur ueber RAD_ClientUnitSubscribe.
    RAD_ModelObservable_t observable;
} RAD_ClientUnit_t;

typedef enum
{
    RAD_CLIENT_UNIT_EVENT_CHANGED = 0,
    RAD_CLIENT_UNIT_EVENT_REMOVED
} RAD_ClientUnitEvent_t;

///
/// Ein Beobachter einer Einheit. "unit" ist die beobachtete Einheit selbst und
/// gilt, solange sie an ihrem Platz steht. Ein NULL-Callback heisst: dieses
/// Ereignis nicht.
///
/// Nach "removed" ist der Beobachter abgemeldet -- die Einheit gibt es dort
/// nicht mehr.
///
typedef struct
{
    void *user_argument;
    void (*changed)(void *user_argument, const RAD_ClientUnit_t *unit);
    void (*removed)(void *user_argument, const RAD_ClientUnit_t *unit);
} RAD_ClientUnitObserver_t;

///
/// Bringt "unit" in den Grundzustand: Id "id", Besitzer "owner", Name "name",
/// Bewegung 0, ohne Mitglieder und **ohne Beobachter** -- fuer eine neue Einheit,
/// nicht fuer eine, die schon jemand beobachtet. Ein zu langer Name wird
/// gekuerzt -- der Client zeigt ihn nur an --, NULL gilt als leerer Name.
///
void RAD_ClientUnitInit(RAD_ClientUnit_t *unit, RAD_ClientUnitId_t id, RAD_ClientPlayerId_t owner, const char *name);

///
/// Uebernimmt alles aus "source" ausser dessen Beobachtern: die von "unit"
/// bleiben und erfahren es mit "changed".
///
void RAD_ClientUnitAssign(RAD_ClientUnit_t *unit, const RAD_ClientUnit_t *source);

///
/// Haengt eine Kopie von "member" an und meldet "changed". false, wenn schon
/// RAD_CLIENT_UNIT_MEMBERS_MAX Mitglieder da sind oder "member" mehr als
/// RAD_CLIENT_MEMBER_WEAPONS_MAX Waffen nennt -- dann bleibt alles, wie es war,
/// und gemeldet wird nichts.
///
bool RAD_ClientUnitAddMember(RAD_ClientUnit_t *unit, const RAD_ClientMember_t *member);

///
/// Entfernt alle Mitglieder und meldet "changed".
///
void RAD_ClientUnitClearMembers(RAD_ClientUnit_t *unit);

///
/// Meldet "removed" und danach alle Beobachter ab. Fuer den, der die Einheit
/// haelt, wenn er sie entfernt (RAD_ClientUnitRepositoryRemove); an den Daten
/// aendert es nichts.
///
void RAD_ClientUnitRemove(RAD_ClientUnit_t *unit);

///
/// Meldet "observer" an der Einheit an, bzw. den mit "user_argument" ab
/// (model/events/observable.h). false, wenn die Einheit schon
/// RAD_MODEL_OBSERVERS_MAX Beobachter hat, das user_argument schon angemeldet
/// ist bzw. beim Abmelden keiner so heisst.
///
bool RAD_ClientUnitSubscribe(const RAD_ClientUnit_t *unit, RAD_ClientUnitObserver_t observer);
bool RAD_ClientUnitUnsubscribe(const RAD_ClientUnit_t *unit, const void *user_argument);

///
/// Das Mitglied an Stelle "index", oder false, wenn der Index ausserhalb
/// [0, number_of_members) liegt. "output" wird nur bei true beschrieben.
///
bool RAD_ClientUnitMemberAt(const RAD_ClientUnit_t *unit, size_t index, RAD_ClientMember_t *output);

///
/// Die Lebenspunkte der ganzen Einheit: in "current" die Summe dessen, was ihre
/// Mitglieder noch haben, in "maximum" die Summe ihrer Hoechstwerte. Ohne
/// Mitglieder beide 0. Gedacht fuer RAD_UnitInfoViewSetHitPoints.
///
void RAD_ClientUnitHitPoints(const RAD_ClientUnit_t *unit, int32_t *current, int32_t *maximum);

#endif
