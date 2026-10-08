#ifndef __RAD_MODEL_UNIT_REPOSITORY_H__
#define __RAD_MODEL_UNIT_REPOSITORY_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/model/unit.h>

///
/// Einheiten, die das Repository hoechstens haelt, wie RAD_MAX_UNITS im Spiel --
/// dort fuer alle Spieler zusammen, mehr gibt es in einem Spiel nicht.
///
#define RAD_CLIENT_UNIT_REPOSITORY_UNITS_MAX 64

///
/// Alle Einheiten, die der Client zur Laufzeit kennt -- jede genau einmal, unter
/// ihrer Id.
///
/// **Ids sind nicht negativ**, die 0 eingeschlossen (RAD_ClientUnitId_t); eine
/// negative Id nennt keine Einheit.
///
/// **Alles in festen Feldern, nichts auf dem Heap**, wie im uebrigen Modell.
///
/// **Die Einheiten bleiben an ihrem Platz.** Eine Einheit wird nie verschoben;
/// ein Zeiger auf sie (aus RAD_ClientUnitRepositoryCreate oder
/// RAD_ClientUnitRepositoryFind) gilt, bis sie entfernt wird. Danach ist ihr
/// Platz frei und kann an eine **andere** Einheit vergeben werden -- wer einen
/// Zeiger haelt, erfaehrt das ueber das "removed" der Einheit (model/unit.h).
///
/// **Nicht per Zuweisung kopieren.** Die Einheiten darin tragen ihre Beobachter
/// (RAD_ClientUnit_t.observable); eine Kopie des Repositorys haette dieselben,
/// ohne dass die je davon erfuehren.
///
typedef struct
{
    RAD_ClientUnit_t units[RAD_CLIENT_UNIT_REPOSITORY_UNITS_MAX];

    /// Ob units[i] eine Einheit haelt.
    bool present[RAD_CLIENT_UNIT_REPOSITORY_UNITS_MAX];

    /// Wie viele Plaetze belegt sind.
    uint32_t number_of_units;
} RAD_ClientUnitRepository_t;

///
/// Bringt "repository" in den Grundzustand: leer. Fuer ein neues Repository --
/// Einheiten, die noch darin stehen, werden nicht entfernt und melden nichts.
///
void RAD_ClientUnitRepositoryInit(RAD_ClientUnitRepository_t *repository);

///
/// Legt eine neue Einheit an, im Grundzustand von RAD_ClientUnitInit (Id "id",
/// Besitzer "owner", Name "name", ohne Mitglieder, ohne Beobachter), und gibt
/// sie zurueck -- zum Befuellen, etwa mit RAD_ClientUnitAddMember.
///
/// NULL, wenn "id" negativ ist, schon eine Einheit so heisst oder kein Platz
/// mehr frei ist -- dann bleibt alles, wie es war.
///
RAD_ClientUnit_t* RAD_ClientUnitRepositoryCreate(RAD_ClientUnitRepository_t *repository,
                                                 RAD_ClientUnitId_t id,
                                                 RAD_ClientPlayerId_t owner,
                                                 const char *name);

///
/// Entfernt die Einheit mit der Id "id": sie meldet "removed", danach sind ihre
/// Beobachter abgemeldet (RAD_ClientUnitRemove), und ihr Platz ist frei.
///
/// false, wenn keine Einheit so heisst.
///
bool RAD_ClientUnitRepositoryRemove(RAD_ClientUnitRepository_t *repository, RAD_ClientUnitId_t id);

///
/// Die Einheit mit der Id "id", oder NULL, wenn keine so heisst. Die
/// Const-Variante fuer die, die nur lesen.
///
RAD_ClientUnit_t* RAD_ClientUnitRepositoryFind(RAD_ClientUnitRepository_t *repository, RAD_ClientUnitId_t id);
const RAD_ClientUnit_t* RAD_ClientUnitRepositoryFindConst(const RAD_ClientUnitRepository_t *repository, RAD_ClientUnitId_t id);

#endif
