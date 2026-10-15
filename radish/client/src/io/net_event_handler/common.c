#include "common.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void RAD_IoNetPrintDiscoverAnswerPrefix(const RAD_IoNetEventHandlerContext_t *context)
{
    const RAD_IoNetDiscoverRequest_t *request = &context->session->last_discover_request;

    if(!request->sent)
    {
        printf("<- Discover-Antwort (ohne eigene Anfrage): ");
        return;
    }

    printf("<- Discover-Antwort auf x=%u y=%u w=%u h=%u: ",
        (unsigned)request->x, (unsigned)request->y,
        (unsigned)request->w, (unsigned)request->h);
}

void RAD_IoNetPrintTile(const RAD_NetTile_t *tile)
{
    printf("Feld (%u, %u) Typ=%s z=%u MV=%d CV=%u%s Figur=",
        (unsigned)tile->x, (unsigned)tile->y, RAD_NetTileTypeText(tile->type), (unsigned)tile->z,
        (int)tile->movement_modifier, (unsigned)tile->cover, tile->impassable ? " unbegehbar" : "");

    // -1 ist RAD_NET_ENTITY_NONE; jede andere Zahl ist eine Figur, auch die 0.
    if(tile->entity_id == RAD_NET_ENTITY_NONE)
    {
        printf("keine");
    }
    else
    {
        printf("%d", (int)tile->entity_id);
    }
}

///
/// Der Typ eines Feldes im Modell zu dem auf der Verbindung; was das Modell
/// nicht kennt, ist UNKNOWN.
///
static RAD_ClientTileType_t RAD_IoNetTileTypeToClient(RAD_NetTileType_t type)
{
    switch(type)
    {
        case RAD_NET_TILE_TYPE_GROUND:        return RAD_CLIENT_TILE_TYPE_GROUND;
        case RAD_NET_TILE_TYPE_WATER:         return RAD_CLIENT_TILE_TYPE_WATER_SHALLOW;
        case RAD_NET_TILE_TYPE_VOID:          return RAD_CLIENT_TILE_TYPE_VOID;
        case RAD_NET_TILE_TYPE_MUD:           return RAD_CLIENT_TILE_TYPE_MUD;
        case RAD_NET_TILE_TYPE_MEADOW:        return RAD_CLIENT_TILE_TYPE_MEADOW;
        case RAD_NET_TILE_TYPE_FOREST:        return RAD_CLIENT_TILE_TYPE_FOREST;
        case RAD_NET_TILE_TYPE_WATER_DEEP:    return RAD_CLIENT_TILE_TYPE_WATER_DEEP;
        case RAD_NET_TILE_TYPE_CITY:          return RAD_CLIENT_TILE_TYPE_CITY;
        case RAD_NET_TILE_TYPE_ROAD:          return RAD_CLIENT_TILE_TYPE_ROAD;
        case RAD_NET_TILE_TYPE_FORTIFICATION: return RAD_CLIENT_TILE_TYPE_FORTIFICATION;
        case RAD_NET_TILE_TYPE_UNKNOWN:       return RAD_CLIENT_TILE_TYPE_UNKNOWN;
    }
    return RAD_CLIENT_TILE_TYPE_UNKNOWN;
}

void RAD_IoNetApplyTile(RAD_IoNetEventHandlerContext_t *context, const RAD_NetTile_t *tile)
{
    // RAD_NET_ENTITY_NONE und RAD_CLIENT_UNIT_ID_NONE sind beide -1.
    const RAD_ClientPosition_t position = { .x = (int32_t)tile->x, .y = (int32_t)tile->y };
    // CV kommt als uint32; was nicht in int32 passt, ist eine unerreichbare Deckung.
    const RAD_ClientTerrain_t terrain = {
        .movement_modifier = tile->movement_modifier,
        .cover = (tile->cover > (uint32_t)INT32_MAX) ? INT32_MAX : (int32_t)tile->cover,
        .impassable = tile->impassable
    };
    RAD_ClientWorldApplyTile(context->world, position, RAD_IoNetTileTypeToClient(tile->type), tile->entity_id, tile->victory, terrain);
}

///
/// Ein Wert der Nachricht fuer ein int16-Feld des Modells; was groesser ist,
/// wird INT16_MAX statt negativ.
///
static int16_t RAD_IoNetValue(uint32_t value)
{
    return (value > (uint32_t)INT16_MAX) ? INT16_MAX : (int16_t)value;
}

void RAD_IoNetUnitToClient(const RAD_NetUnit_t *unit, RAD_ClientUnit_t *out)
{
    RAD_ClientUnitInit(out, unit->unit, unit->owner, unit->name);
    out->movement = RAD_IoNetValue(unit->movement);
    out->deployed = unit->deployed;
    out->moved = unit->moved;
    out->attacked = unit->attacked;

    // RAD_ClientUnitAddMember meldet "changed" -- an einer Einheit ohne
    // Beobachter erreicht das niemanden. Mehr Mitglieder oder Waffen, als das
    // Modell fasst, laesst der Codec nicht durch (RAD_NetFillUnit).
    for(uint32_t m = 0; m < unit->number_of_members; ++m)
    {
        const RAD_NetUnitMember_t *net_member = &unit->members[m];

        RAD_ClientMember_t member;
        memset(&member, 0, sizeof(member));
        strncpy(member.profile, net_member->profile, sizeof(member.profile) - 1);
        member.health = RAD_IoNetValue(net_member->health);
        // Der Hoechstwert kommt vom Server; schickt er keinen, ist es der jetzige.
        member.health_max = RAD_IoNetValue(net_member->health_max);
        if(member.health_max < member.health)
        {
            member.health_max = member.health;
        }
        member.armor = RAD_IoNetValue(net_member->armor);
        member.toughness = RAD_IoNetValue(net_member->toughness);
        member.strength = RAD_IoNetValue(net_member->strength);
        member.accuracy = RAD_IoNetValue(net_member->accuracy);

        for(uint32_t w = 0; w < net_member->number_of_weapons; ++w)
        {
            const RAD_NetWeapon_t *net_weapon = &net_member->weapons[w];
            RAD_ClientWeapon_t *weapon = &member.weapons[w];
            strncpy(weapon->name, net_weapon->name, sizeof(weapon->name) - 1);
            weapon->shots = RAD_IoNetValue(net_weapon->shots);
            weapon->strength = RAD_IoNetValue(net_weapon->strength);
            weapon->min_range = RAD_IoNetValue(net_weapon->min_range);
            weapon->max_range = RAD_IoNetValue(net_weapon->max_range);
            weapon->penetration = RAD_IoNetValue(net_weapon->penetration);
        }
        member.number_of_weapons = net_member->number_of_weapons;

        RAD_ClientUnitAddMember(out, &member);
    }
}
