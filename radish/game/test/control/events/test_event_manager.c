#include <unity.h>
#include <radish/game/control/events/event_manager.h>

///
/// Ein Abonnent bekommt sein user_argument zurueck -- daran haengt alles Weitere,
/// weil ein Callback sonst seinen eigenen Zustand nicht wiederfindet.
///
static void on_tile(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
    *(int*)user_argument += 1;
}

void test_manager_reicht_ein_ereignis_durch(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    int gezaehlt = 0;
    RAD_EventManagerSubscribeToTileEvents(events, (RAD_EventsTileChangedCallback_t){
        .user_argument = &gezaehlt,
        .added = on_tile,
        .removed = on_tile,
        .changed = on_tile
    });

    RAD_EventManagerPublishTileAddedToGameEvent(events, NULL);
    RAD_EventManagerPublishTileStateChangeEvent(events, NULL);

    TEST_ASSERT_EQUAL_INT(2, gezaehlt);

    RAD_DestroyEventManager(&events);
}
