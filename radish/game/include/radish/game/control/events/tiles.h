#ifndef __RAD_CONTROL_EVENT_TILES_H__
#define __RAD_CONTROL_EVENT_TILES_H__

#include <stdint.h>

typedef struct
{
    uint8_t x;
    uint8_t y;
    uint16_t type;
} RAD_TileSpawnedEvent_t;

typedef struct 
{
    uint8_t x;
    uint8_t y;
} RAD_TileRemovedEvent_t;

typedef struct
{

} RAD_TileEvent_t;


#endif
