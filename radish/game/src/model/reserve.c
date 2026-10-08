#include <radish/game/model/reserve/reserve.h>
#include <radish/game/game_definitions.h>
#include <stdlib.h>
#include <stddef.h>

///
/// Die Struktur steht hier und nicht im Header, wie beim Einheitenpool.
/// "units" ist vorne mit "number_of_units" Zeigern belegt, dahinter steht nichts
/// Gueltiges.
///
struct RAD_Reserve
{
    RAD_Unit_t *units[RAD_MAX_UNITS];
    int32_t number_of_units;
};

static int32_t RAD_ReserveIndexOf(const RAD_Reserve_t *reserve, const RAD_Unit_t *unit);

RAD_Reserve_t* RAD_CreateReserve(void)
{
    RAD_Reserve_t *reserve = malloc(sizeof(struct RAD_Reserve));
    if(reserve == NULL)
    {
        return NULL;
    }

    reserve->number_of_units = 0;
    return reserve;
}

void RAD_DestroyReserve(RAD_Reserve_t **reserve)
{
    if(reserve == NULL)
    {
        return;
    }
    free(*reserve);
    *reserve = NULL;
}

bool RAD_ReserveAddUnit(RAD_Reserve_t *reserve, RAD_Unit_t *unit)
{
    if(reserve == NULL || unit == NULL)
    {
        return false;
    }
    if(RAD_ReserveIndexOf(reserve, unit) >= 0)
    {
        return true;
    }
    if(reserve->number_of_units >= RAD_MAX_UNITS)
    {
        return false;
    }

    reserve->units[reserve->number_of_units] = unit;
    reserve->number_of_units++;
    return true;
}

bool RAD_ReserveRemoveUnit(RAD_Reserve_t *reserve, const RAD_Unit_t *unit)
{
    if(reserve == NULL || unit == NULL)
    {
        return false;
    }

    const int32_t index = RAD_ReserveIndexOf(reserve, unit);
    if(index < 0)
    {
        return false;
    }

    for(int32_t i = index + 1; i < reserve->number_of_units; ++i)
    {
        reserve->units[i - 1] = reserve->units[i];
    }
    reserve->number_of_units--;
    return true;
}

bool RAD_ReserveContains(const RAD_Reserve_t *reserve, const RAD_Unit_t *unit)
{
    if(reserve == NULL || unit == NULL)
    {
        return false;
    }
    return RAD_ReserveIndexOf(reserve, unit) >= 0;
}

int32_t RAD_ReserveNumberOfUnits(const RAD_Reserve_t *reserve)
{
    return reserve == NULL ? 0 : reserve->number_of_units;
}

RAD_Unit_t* RAD_ReserveUnitAt(const RAD_Reserve_t *reserve, int32_t index)
{
    if(reserve == NULL || index < 0 || index >= reserve->number_of_units)
    {
        return NULL;
    }
    return reserve->units[index];
}

///
/// Die Stelle von "unit" in der Liste, oder -1. Verglichen wird auf Gleichheit
/// der Zeiger -- dieselbe Einheit ist derselbe Platz im Pool.
///
static int32_t RAD_ReserveIndexOf(const RAD_Reserve_t *reserve, const RAD_Unit_t *unit)
{
    for(int32_t i = 0; i < reserve->number_of_units; ++i)
    {
        if(reserve->units[i] == unit)
        {
            return i;
        }
    }
    return -1;
}
