#include <radish/model/unit_repository.h>

#include <stddef.h>
#include <string.h>

///
/// Der Platz der Einheit mit der Id "id", oder -1.
///
static int32_t RAD_ClientUnitRepositorySlotOf(const RAD_ClientUnitRepository_t *repository, RAD_ClientUnitId_t id)
{
    for(uint32_t i = 0; i < RAD_CLIENT_UNIT_REPOSITORY_UNITS_MAX; ++i)
    {
        if(repository->present[i] && (repository->units[i].id == id))
        {
            return (int32_t)i;
        }
    }
    return -1;
}

///
/// Der erste freie Platz, oder -1.
///
static int32_t RAD_ClientUnitRepositoryFreeSlot(const RAD_ClientUnitRepository_t *repository)
{
    for(uint32_t i = 0; i < RAD_CLIENT_UNIT_REPOSITORY_UNITS_MAX; ++i)
    {
        if(!repository->present[i])
        {
            return (int32_t)i;
        }
    }
    return -1;
}

void RAD_ClientUnitRepositoryInit(RAD_ClientUnitRepository_t *repository)
{
    memset(repository, 0, sizeof(*repository));
}

RAD_ClientUnit_t* RAD_ClientUnitRepositoryCreate(RAD_ClientUnitRepository_t *repository,
                                                 RAD_ClientUnitId_t id,
                                                 RAD_ClientPlayerId_t owner,
                                                 const char *name)
{
    if((id < 0) || (RAD_ClientUnitRepositorySlotOf(repository, id) >= 0))
    {
        return NULL;
    }

    const int32_t slot = RAD_ClientUnitRepositoryFreeSlot(repository);
    if(slot < 0)
    {
        return NULL;
    }

    // Ein freier Platz hat keine Beobachter mehr (RAD_ClientUnitRemove) -- Init
    // nimmt niemandem etwas weg.
    RAD_ClientUnit_t *created = &repository->units[slot];
    RAD_ClientUnitInit(created, id, owner, name);
    repository->present[slot] = true;
    ++repository->number_of_units;

    return created;
}

bool RAD_ClientUnitRepositoryRemove(RAD_ClientUnitRepository_t *repository, RAD_ClientUnitId_t id)
{
    const int32_t slot = RAD_ClientUnitRepositorySlotOf(repository, id);
    if(slot < 0)
    {
        return false;
    }

    // Erst freigeben, dann melden: wer im "removed" nach der Id sucht, findet sie
    // schon nicht mehr.
    repository->present[slot] = false;
    --repository->number_of_units;

    RAD_ClientUnitRemove(&repository->units[slot]);
    return true;
}

RAD_ClientUnit_t* RAD_ClientUnitRepositoryFind(RAD_ClientUnitRepository_t *repository, RAD_ClientUnitId_t id)
{
    const int32_t slot = RAD_ClientUnitRepositorySlotOf(repository, id);
    if(slot < 0)
    {
        return NULL;
    }
    return &repository->units[slot];
}

const RAD_ClientUnit_t* RAD_ClientUnitRepositoryFindConst(const RAD_ClientUnitRepository_t *repository, RAD_ClientUnitId_t id)
{
    const int32_t slot = RAD_ClientUnitRepositorySlotOf(repository, id);
    if(slot < 0)
    {
        return NULL;
    }
    return &repository->units[slot];
}
