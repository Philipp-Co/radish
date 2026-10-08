#include <radish/game/model/unit_pool/unit_pool.h>
#include <radish/game/game_definitions.h>
#include <stdlib.h>
#include <stddef.h>

///
/// Die Struktur steht hier und nicht im Header -- wie beim Event-Manager: was
/// niemand sieht, kann niemand an RAD_UnitPoolAddUnit vorbei belegen.
///
/// Ein freier Platz traegt id == RAD_UNIT_NONE, wie in der Welt.
///
struct RAD_UnitPool
{
    RAD_Unit_t units[RAD_MAX_UNITS];

    /// Anzahl belegter Plaetze.
    int32_t number_of_units;

    /// Die naechste Id. Sie zaehlt nur hoch, auch wenn ein Platz frei wird.
    RAD_UnitId_t next_id;
};

static RAD_Unit_t RAD_UnitPoolFreeSlot(void);

RAD_UnitPool_t* RAD_CreateUnitPool(void)
{
    RAD_UnitPool_t *pool = malloc(sizeof(struct RAD_UnitPool));
    if(pool == NULL)
    {
        return NULL;
    }

    for(int32_t i = 0; i < RAD_MAX_UNITS; ++i)
    {
        pool->units[i] = RAD_UnitPoolFreeSlot();
    }
    pool->number_of_units = 0;
    pool->next_id = 0;

    return pool;
}

void RAD_DestroyUnitPool(RAD_UnitPool_t **pool)
{
    if(pool == NULL)
    {
        return;
    }
    free(*pool);
    *pool = NULL;
}

RAD_Unit_t* RAD_UnitPoolAddUnit(RAD_UnitPool_t *pool)
{
    if(pool == NULL || pool->next_id == INT32_MAX)
    {
        return NULL;
    }

    for(int32_t i = 0; i < RAD_MAX_UNITS; ++i)
    {
        RAD_Unit_t *unit = &pool->units[i];
        if(unit->id != RAD_UNIT_NONE)
        {
            continue;
        }

        *unit = (RAD_Unit_t){
            .id = pool->next_id,
            .type = RAD_UNIT_TYPE_NONE,
            .state = RAD_UNIT_STATE_RESERVE,
            .owner = RAD_USER_NONE,
            .x = -1,
            .y = -1
        };
        pool->next_id++;
        pool->number_of_units++;
        return unit;
    }

    return NULL;
}

bool RAD_UnitPoolRemoveUnit(RAD_UnitPool_t *pool, RAD_Unit_t **unit)
{
    if(pool == NULL || unit == NULL || *unit == NULL)
    {
        return false;
    }

    // Gesucht wird ueber die Plaetze und nicht ueber die Adresse: zwei Zeiger nach
    // < oder > zu vergleichen ist in C nur innerhalb eines Objekts erlaubt, und ob
    // "*unit" in diesem liegt, ist gerade die Frage. Auf Gleichheit pruefen darf
    // man jeden Zeiger.
    for(int32_t i = 0; i < RAD_MAX_UNITS; ++i)
    {
        RAD_Unit_t *slot = &pool->units[i];
        if(slot != *unit)
        {
            continue;
        }
        if(slot->id == RAD_UNIT_NONE)
        {
            return false;
        }

        *slot = RAD_UnitPoolFreeSlot();
        pool->number_of_units--;
        *unit = NULL;
        return true;
    }

    return false;
}

int32_t RAD_UnitPoolNumberOfUnits(const RAD_UnitPool_t *pool)
{
    return pool == NULL ? 0 : pool->number_of_units;
}

RAD_Unit_t* RAD_UnitPoolUnitById(const RAD_UnitPool_t *pool, RAD_UnitId_t id)
{
    if(pool == NULL || id == RAD_UNIT_NONE)
    {
        return NULL;
    }

    for(int32_t i = 0; i < RAD_MAX_UNITS; ++i)
    {
        if(pool->units[i].id == id)
        {
            // Der Pool gibt seine Einheiten zum Schreiben heraus -- dafuer ist er
            // da (unit_pool.h). Die Konstanz gilt dem Pool, nicht seinem Inhalt.
            return (RAD_Unit_t *)&pool->units[i];
        }
    }
    return NULL;
}

RAD_Unit_t* RAD_UnitPoolUnitAt(const RAD_UnitPool_t *pool, int32_t index)
{
    if(pool == NULL || index < 0 || index >= pool->number_of_units)
    {
        return NULL;
    }

    int32_t seen = 0;
    for(int32_t i = 0; i < RAD_MAX_UNITS; ++i)
    {
        if(pool->units[i].id == RAD_UNIT_NONE)
        {
            continue;
        }
        if(seen == index)
        {
            return (RAD_Unit_t *)&pool->units[i];
        }
        seen++;
    }
    return NULL;
}

///
/// Wie ein freier Platz aussieht -- derselbe Stand wie in der Welt
/// (RAD_ResetWorldToSize), damit ein frei gewordener Platz nichts von seiner
/// Einheit behaelt.
///
static RAD_Unit_t RAD_UnitPoolFreeSlot(void)
{
    return (RAD_Unit_t){
        .id = RAD_UNIT_NONE,
        .type = RAD_UNIT_TYPE_NONE,
        .owner = RAD_USER_NONE,
        .x = -1,
        .y = -1
    };
}
