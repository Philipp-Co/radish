#include <unity.h>
#include <radish/game/model/world/world.h>

///
/// Eine frische Welt ist leer und stimmig. RAD_InitWorld veroeffentlicht dabei
/// ein Tile-Ereignis je Feld, deshalb braucht schon dieser Test einen Manager.
///
void test_world_faengt_leer_und_stimmig_an(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_World_t world = RAD_CreateWorld(events);
    RAD_InitWorld(&world);

    TEST_ASSERT_EQUAL_INT(0, world.number_of_entities);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&world));

    RAD_DestroyEventManager(&events);
}

///
/// Die Schreibseite der Tiles (RAD_WorldAddTile, RAD_WorldRemoveTile).
///
/// Zwei Zusagen ziehen sich durch alle Tests: was im Raster steht, und *welches*
/// der drei Ereignisse dazu herausgegangen ist -- es haengt am Uebergang und nicht
/// am Namen der gerufenen Funktion (world.h).
///
typedef struct
{
    int added;
    int removed;
    int changed;
} RAD_TileEreignisse_t;

static void zaehle_added(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
    ((RAD_TileEreignisse_t*)user_argument)->added += 1;
}

static void zaehle_removed(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
    ((RAD_TileEreignisse_t*)user_argument)->removed += 1;
}

static void zaehle_changed(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
    ((RAD_TileEreignisse_t*)user_argument)->changed += 1;
}

///
/// Abonniert wird immer *nach* RAD_InitWorld: der Init veroeffentlicht ein
/// added-Ereignis je Feld, und die 64 gehen keinen Test hier etwas an.
///
static void abonniere_tile_ereignisse(RAD_EventManager_t *events, RAD_TileEreignisse_t *gezaehlt)
{
    *gezaehlt = (RAD_TileEreignisse_t){ .added = 0, .removed = 0, .changed = 0 };

    RAD_EventManagerSubscribeToTileEvents(events, (RAD_EventsTileChangedCallback_t){
        .user_argument = gezaehlt,
        .added = zaehle_added,
        .removed = zaehle_removed,
        .changed = zaehle_changed
    });
}

void test_world_tile_hinzufuegen_setzt_typ_und_hoehe(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_World_t world = RAD_CreateWorld(events);
    RAD_InitWorld(&world);

    RAD_TileEreignisse_t gezaehlt;
    abonniere_tile_ereignisse(events, &gezaehlt);

    TEST_ASSERT_TRUE(RAD_WorldAddTile(&world, 3, 5, 2, RAD_TILE_TYPE_WATER));

    const RAD_Tile_t *tile = RAD_WorldTileAt(&world, 3, 5);
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER, tile->type);
    TEST_ASSERT_EQUAL_INT(2, tile->z);

    // Die Stelle bleibt die Stelle.
    TEST_ASSERT_EQUAL_INT(3, tile->x);
    TEST_ASSERT_EQUAL_INT(5, tile->y);

    // Aus RAD_InitWorld steht dort GROUND, es ist also eine Aenderung und kein
    // Zuwachs -- auch wenn die Funktion "Add" heisst.
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.changed);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.added);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&world));

    RAD_DestroyEventManager(&events);
}

void test_world_tile_entfernen_macht_void_und_laesst_die_stelle_stehen(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_World_t world = RAD_CreateWorld(events);
    RAD_InitWorld(&world);

    TEST_ASSERT_TRUE(RAD_WorldAddTile(&world, 1, 1, 4, RAD_TILE_TYPE_GROUND));

    RAD_TileEreignisse_t gezaehlt;
    abonniere_tile_ereignisse(events, &gezaehlt);

    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&world, 1, 1));

    const RAD_Tile_t *tile = RAD_WorldTileAt(&world, 1, 1);
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_VOID, tile->type);

    // Weggenommen wird das Gelaende, nicht das Feld: Position, Hoehe und der Platz
    // fuer eine Figur bleiben stehen.
    TEST_ASSERT_EQUAL_INT(1, tile->x);
    TEST_ASSERT_EQUAL_INT(1, tile->y);
    TEST_ASSERT_EQUAL_INT(4, tile->z);
    TEST_ASSERT_EQUAL_INT(RAD_ENTITY_NONE, tile->entity);

    TEST_ASSERT_EQUAL_INT(1, gezaehlt.removed);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.added);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.changed);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&world));

    RAD_DestroyEventManager(&events);
}

void test_world_tile_entfernen_scheitert_unter_einer_figur(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_World_t world = RAD_CreateWorld(events);
    RAD_InitWorld(&world);

    const RAD_EntityId_t id = RAD_WorldSpawnEntity(&world, RAD_ENTITY_TYPE_PLAYER, 2, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, id);

    RAD_TileEreignisse_t gezaehlt;
    abonniere_tile_ereignisse(events, &gezaehlt);

    TEST_ASSERT_FALSE(RAD_WorldRemoveTile(&world, 2, 2));

    // Abgelehnt heisst: nichts geschrieben und nichts gemeldet.
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_GROUND, RAD_WorldTileAt(&world, 2, 2)->type);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.removed);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.changed);

    // VOID hinzuzufuegen ist derselbe Weg und damit derselbe Ausgang.
    TEST_ASSERT_FALSE(RAD_WorldAddTile(&world, 2, 2, 0, RAD_TILE_TYPE_VOID));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_GROUND, RAD_WorldTileAt(&world, 2, 2)->type);

    // Ohne Figur geht es: die Pruefung haengt an ihr und nicht am Feld.
    RAD_WorldRemoveEntity(&world, id);
    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&world, 2, 2));
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.removed);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&world));

    RAD_DestroyEventManager(&events);
}

void test_world_tile_ausserhalb_der_welt_ist_kein_tile(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_World_t world = RAD_CreateWorld(events);
    RAD_InitWorld(&world);

    RAD_TileEreignisse_t gezaehlt;
    abonniere_tile_ereignisse(events, &gezaehlt);

    TEST_ASSERT_FALSE(RAD_WorldAddTile(&world, -1, 0, 0, RAD_TILE_TYPE_WATER));
    TEST_ASSERT_FALSE(RAD_WorldAddTile(&world, 0, RAD_WORLD_HEIGHT, 0, RAD_TILE_TYPE_WATER));
    TEST_ASSERT_FALSE(RAD_WorldRemoveTile(&world, RAD_WORLD_WIDTH, 0));
    TEST_ASSERT_FALSE(RAD_WorldRemoveTile(&world, 0, -1));

    TEST_ASSERT_EQUAL_INT(0, gezaehlt.added);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.removed);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.changed);

    RAD_DestroyEventManager(&events);
}

void test_world_tile_ereignis_folgt_dem_uebergang(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_World_t world = RAD_CreateWorld(events);
    RAD_InitWorld(&world);

    RAD_TileEreignisse_t gezaehlt;
    abonniere_tile_ereignisse(events, &gezaehlt);

    // X -> VOID
    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&world, 0, 0));
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.removed);

    // VOID -> X
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&world, 0, 0, 0, RAD_TILE_TYPE_GROUND));
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.added);

    // X -> Y
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&world, 0, 0, 0, RAD_TILE_TYPE_WATER));
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.changed);

    // Derselbe Typ, andere Hoehe: auch eine Aenderung -- die Hoehe gehoert zum
    // Zustand des Feldes.
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&world, 0, 0, 3, RAD_TILE_TYPE_WATER));
    TEST_ASSERT_EQUAL_INT(2, gezaehlt.changed);

    // Und derselbe Stand meldet nichts.
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&world, 0, 0, 3, RAD_TILE_TYPE_WATER));
    TEST_ASSERT_EQUAL_INT(2, gezaehlt.changed);
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.added);
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.removed);

    RAD_DestroyEventManager(&events);
}

void test_world_tile_entfernen_ist_idempotent(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_World_t world = RAD_CreateWorld(events);
    RAD_InitWorld(&world);

    RAD_TileEreignisse_t gezaehlt;
    abonniere_tile_ereignisse(events, &gezaehlt);

    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&world, 7, 7));
    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&world, 7, 7));

    // Zweimal true, aber nur eine Aenderung -- und nur ein Ereignis.
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.removed);

    TEST_ASSERT_TRUE(RAD_WorldAddTile(&world, 7, 7, 0, RAD_TILE_TYPE_VOID));
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.removed);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.added);

    RAD_DestroyEventManager(&events);
}
