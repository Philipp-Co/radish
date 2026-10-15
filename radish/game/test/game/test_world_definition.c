#include <unity.h>
#include <string.h>

#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/serialization/world_definition.h>

///
/// Die Weltdefinition (world_definition.h): Gelaende und Hoehen aus einer Datei,
/// die Groesse aus ihren Zeilen.
///
/// Geladen wird meist aus einem String, wie in test_load.c -- die Datei ist dort
/// nicht die Frage. Nur der letzte Test geht ueber RAD_LoadWorldFromFile und liest
/// die Welt, die im Repository liegt (assets/worlds/default.json).
///

typedef struct
{
    int added;
    int removed;
    int changed;
} RAD_WeltMitschrift_t;

static void welt_added(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
    ((RAD_WeltMitschrift_t*)user_argument)->added += 1;
}

static void welt_removed(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
    ((RAD_WeltMitschrift_t*)user_argument)->removed += 1;
}

static void welt_changed(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
    ((RAD_WeltMitschrift_t*)user_argument)->changed += 1;
}

static void abonniere(RAD_EventManager_t *events, RAD_WeltMitschrift_t *mitschrift)
{
    *mitschrift = (RAD_WeltMitschrift_t){ .added = 0, .removed = 0, .changed = 0 };

    RAD_EventManagerSubscribeToTileEvents(events, (RAD_EventsTileChangedCallback_t){
        .user_argument = mitschrift,
        .added = welt_added,
        .removed = welt_removed,
        .changed = welt_changed
    });
}

static RAD_SerializeResult_t lade(RAD_Game_t *game, const char *json)
{
    return RAD_DeserializeWorldDefinitionFromJson(game, json, strlen(json));
}

///
/// Eine Welt aus 3 x 2 Feldern, mit Boden, Wasser und Leere und Hoehen. Die
/// Groesse steht nirgends in der Datei und kommt trotzdem an.
///
void test_weltdefinition_baut_gelaende_und_hoehen_auf(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game,
        "{\"version\":1,\"name\":\"Klein\","
        "\"rows\":[\"._~\",\"~..\"],"
        "\"heights\":[\"100\",\"002\"]}"));

    TEST_ASSERT_EQUAL_INT(3, RAD_GameWorldWidth(game));
    TEST_ASSERT_EQUAL_INT(2, RAD_GameWorldHeight(game));
    TEST_ASSERT_EQUAL_INT(6, RAD_GameNumberOfTiles(game));

    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 0, 0, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_GROUND, tile.type);
    TEST_ASSERT_EQUAL_INT(1, tile.z);

    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 1, 0, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_VOID, tile.type);

    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 2, 0, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER_SHALLOW, tile.type);

    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 2, 1, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_GROUND, tile.type);
    TEST_ASSERT_EQUAL_INT(2, tile.z);
    TEST_ASSERT_EQUAL_INT(2, tile.x);
    TEST_ASSERT_EQUAL_INT(1, tile.y);

    // Rechts und unterhalb der Welt ist keine Welt mehr.
    TEST_ASSERT_FALSE(RAD_WorldInBounds(&game->world, 3, 0));
    TEST_ASSERT_FALSE(RAD_WorldInBounds(&game->world, 0, 2));
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

void test_weltdefinition_ohne_hoehen_ist_flach(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game, "{\"version\":1,\"rows\":[\"..\",\"~~\"]}"));

    for(int32_t y=0;y < 2; ++y)
    {
        for(int32_t x=0;x < 2; ++x)
        {
            TEST_ASSERT_EQUAL_INT(0, game->world.tiles[y][x].z);
        }
    }

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Die Schluessel duerfen in jeder Reihenfolge stehen -- auch die Hoehen vor dem
/// Gelaende und die Version zuletzt.
///
void test_weltdefinition_reihenfolge_der_schluessel_ist_egal(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game,
        "{\"heights\":[\"30\"],\"rows\":[\".~\"],\"$schema\":\"world.schema.json\",\"version\":1}"));

    TEST_ASSERT_EQUAL_INT(2, RAD_GameWorldWidth(game));
    TEST_ASSERT_EQUAL_INT(1, RAD_GameWorldHeight(game));
    TEST_ASSERT_EQUAL_INT(3, game->world.tiles[0][0].z);
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER_SHALLOW, game->world.tiles[0][1].type);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Die groesste Welt, die das Programm halten kann, geht gerade noch.
///
void test_weltdefinition_in_voller_groesse(void)
{
    char json[512];
    strcpy(json, "{\"version\":1,\"rows\":[");
    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        strcat(json, (y == 0) ? "\"" : ",\"");
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            strcat(json, "~");
        }
        strcat(json, "\"");
    }
    strcat(json, "]}");

    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game, json));
    TEST_ASSERT_EQUAL_INT(RAD_WORLD_WIDTH, RAD_GameWorldWidth(game));
    TEST_ASSERT_EQUAL_INT(RAD_WORLD_HEIGHT, RAD_GameWorldHeight(game));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER_SHALLOW, game->world.tiles[RAD_WORLD_HEIGHT - 1][RAD_WORLD_WIDTH - 1].type);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Jede Datei hier ist in genau einer Hinsicht falsch, und jede hat ihren Fehler.
/// Das Spiel bleibt jedes Mal, wie es war, und gemeldet wird nichts.
///
void test_weltdefinition_lehnt_fehlerhafte_dateien_ab(void)
{
    const struct
    {
        const char *json;
        RAD_SerializeResult_t erwartet;
    } faelle[] = {
        // Die Groesse.
        { "{\"version\":1,\"rows\":[]}",                                  RAD_SERIALIZE_ERROR_SIZE_MISMATCH },
        { "{\"version\":1,\"rows\":[\"\"]}",                              RAD_SERIALIZE_ERROR_SIZE_MISMATCH },
        { "{\"version\":1,\"rows\":[\"...\",\"..\"]}",                    RAD_SERIALIZE_ERROR_SIZE_MISMATCH },
        // Eine Zeile laenger als RAD_WORLD_WIDTH, eine Zeile mehr als RAD_WORLD_HEIGHT.
        { "{\"version\":1,\"rows\":[\".................\"]}",             RAD_SERIALIZE_ERROR_SIZE_MISMATCH },
        { "{\"version\":1,\"rows\":[\".\",\".\",\".\",\".\",\".\",\".\",\".\",\".\",\".\","
          "\".\",\".\",\".\",\".\",\".\",\".\",\".\",\".\"]}",            RAD_SERIALIZE_ERROR_SIZE_MISMATCH },
        { "{\"version\":1,\"rows\":[\"..\"],\"heights\":[\"0\"]}",        RAD_SERIALIZE_ERROR_SIZE_MISMATCH },
        { "{\"version\":1,\"rows\":[\"..\"],\"heights\":[\"00\",\"00\"]}", RAD_SERIALIZE_ERROR_SIZE_MISMATCH },

        // Der Inhalt der Zeilen.
        { "{\"version\":1,\"rows\":[\".#\"]}",                            RAD_SERIALIZE_ERROR_TILE_TYPE },
        { "{\"version\":1,\"rows\":[\"..\"],\"heights\":[\"0a\"]}",       RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[\"._\"],\"heights\":[\"01\"]}",       RAD_SERIALIZE_ERROR_INCONSISTENT },

        // Die Siegfelder.
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":[{\"x\":2,\"y\":0}]}",    RAD_SERIALIZE_ERROR_VICTORY_OUT_OF_BOUNDS },
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":[{\"x\":0,\"y\":-1}]}",   RAD_SERIALIZE_ERROR_VICTORY_OUT_OF_BOUNDS },
        { "{\"version\":1,\"rows\":[\"._\"],\"victory_tiles\":[{\"x\":1,\"y\":0}]}",    RAD_SERIALIZE_ERROR_VICTORY_NO_GROUND },
        { "{\"version\":1,\"rows\":[\".~\"],\"victory_tiles\":[{\"x\":1,\"y\":0}]}",    RAD_SERIALIZE_ERROR_VICTORY_NO_GROUND },
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":[{\"x\":0,\"y\":0},{\"y\":0,\"x\":0}]}",
                                                                          RAD_SERIALIZE_ERROR_VICTORY_DUPLICATE },
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":[{\"x\":0}]}",              RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":[{\"x\":0,\"y\":0,\"z\":0}]}", RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":[{\"x\":0,\"x\":1}]}",    RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":[[0,0]]}",                    RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":{\"x\":0,\"y\":0}}",      RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[\"..\"],\"victory_tiles\":[{\"x\":\"0\",\"y\":0}]}", RAD_SERIALIZE_ERROR_SCHEMA },

        // Der Rahmen.
        { "{\"rows\":[\"..\"]}",                                          RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":2,\"rows\":[\"..\"]}",                            RAD_SERIALIZE_ERROR_VERSION },
        { "{\"version\":2,\"rows\":[\"#\"]}",                             RAD_SERIALIZE_ERROR_VERSION },
        { "{\"version\":1}",                                              RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[\"..\"],\"heigths\":[\"00\"]}",       RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[1,2]}",                               RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":\"..\"}",                              RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"name\":7,\"rows\":[\"..\"]}",                 RAD_SERIALIZE_ERROR_SCHEMA },
        { "[\"..\"]",                                                     RAD_SERIALIZE_ERROR_SCHEMA },
        { "{\"version\":1,\"rows\":[\"..\"",                              RAD_SERIALIZE_ERROR_SYNTAX },
        { "",                                                             RAD_SERIALIZE_ERROR_SYNTAX },
    };

    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&game->world, 3, 3, 2, RAD_TILE_TYPE_WATER_SHALLOW));

    RAD_WeltMitschrift_t mitschrift;
    abonniere(events, &mitschrift);

    for(size_t i=0;i < sizeof(faelle) / sizeof(faelle[0]); ++i)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(faelle[i].erwartet, lade(game, faelle[i].json), faelle[i].json);

        TEST_ASSERT_EQUAL_INT_MESSAGE(RAD_WORLD_WIDTH, RAD_GameWorldWidth(game), faelle[i].json);
        TEST_ASSERT_EQUAL_INT_MESSAGE(RAD_WORLD_HEIGHT, RAD_GameWorldHeight(game), faelle[i].json);
        TEST_ASSERT_EQUAL_INT_MESSAGE(RAD_TILE_TYPE_WATER_SHALLOW, game->world.tiles[3][3].type, faelle[i].json);
        TEST_ASSERT_EQUAL_INT_MESSAGE(2, game->world.tiles[3][3].z, faelle[i].json);
    }

    TEST_ASSERT_EQUAL_INT(0, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.changed);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Steht schon eine Figur im Spiel, wird die Welt nicht ersetzt -- auch nicht mit
/// einer Welt, auf der ihr Feld noch da waere.
///
void test_weltdefinition_mit_figur_im_spiel_wird_abgelehnt(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, 0, 0));

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_ERROR_WORLD_OCCUPIED, lade(game, "{\"version\":1,\"rows\":[\"~~\"]}"));

    TEST_ASSERT_EQUAL_INT(RAD_WORLD_WIDTH, RAD_GameWorldWidth(game));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_GROUND, game->world.tiles[0][0].type);
    TEST_ASSERT_EQUAL_INT(1, RAD_GameNumberOfUnits(game));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Ein Abonnent kennt die Welt aus RAD_CreateGame: RAD_WORLD_WIDTH x RAD_WORLD_HEIGHT Boden. Eine kleinere Welt
/// meldet ihm die Felder, die anders sind, und die, die wegfallen. Eine wieder
/// groessere meldet die neuen als added.
///
void test_weltdefinition_meldet_geaenderte_und_weggefallene_felder(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);

    RAD_WeltMitschrift_t mitschrift;
    abonniere(events, &mitschrift);

    // (0,1) wird Wasser, (1,1) verliert sein Gelaende, (0,0) und (1,0) bleiben.
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game, "{\"version\":1,\"rows\":[\"..\",\"~_\"]}"));

    const int weggefallen = (RAD_WORLD_WIDTH * RAD_WORLD_HEIGHT) - 4;
    TEST_ASSERT_EQUAL_INT(0, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(1 + weggefallen, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.changed);

    // Zurueck auf volle Groesse, ueberall Boden: alles, was eben verschwand, kommt
    // als added zurueck, und das Wasser wird wieder Boden.
    char json[512];
    strcpy(json, "{\"version\":1,\"rows\":[");
    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        strcat(json, (y == 0) ? "\"" : ",\"");
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            strcat(json, ".");
        }
        strcat(json, "\"");
    }
    strcat(json, "]}");

    mitschrift = (RAD_WeltMitschrift_t){ .added = 0, .removed = 0, .changed = 0 };
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game, json));

    TEST_ASSERT_EQUAL_INT(1 + weggefallen, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.changed);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Der oeffentliche Weg, mit der Welt aus dem Repository -- dieselbe Datei, die
/// die Schema-Tests gegen world.schema.json pruefen.
///
void test_weltdefinition_aus_datei(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_LOAD_OK, RAD_LoadWorldFromFile(game, RAD_ASSETS_DIR "/worlds/default.json"));

    TEST_ASSERT_EQUAL_INT(16, RAD_GameWorldWidth(game));
    TEST_ASSERT_EQUAL_INT(16, RAD_GameWorldHeight(game));

    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 5, 5, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER_SHALLOW, tile.type);
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 7, 7, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER_DEEP, tile.type);
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 2, 1, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_FORTIFICATION, tile.type);
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 4, 4, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_ROAD, tile.type);

    // Ohne "heights": alles auf 0.
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 0, 0, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_FOREST, tile.type);
    TEST_ASSERT_EQUAL_INT(0, tile.z);
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 0, 15, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_VOID, tile.type);

    // Die beiden Siegfelder, punktsymmetrisch, je eine Stadt.
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 7, 1, &tile));
    TEST_ASSERT_TRUE(tile.victory);
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_CITY, tile.type);
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 8, 14, &tile));
    TEST_ASSERT_TRUE(tile.victory);
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_CITY, tile.type);
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 8, 1, &tile));
    TEST_ASSERT_FALSE(tile.victory);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_LOAD_ERROR_NOT_FOUND, RAD_LoadWorldFromFile(game, RAD_ASSETS_DIR "/worlds/gibt-es-nicht.json"));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_LOAD_ERROR_NOT_FOUND, RAD_LoadWorldFromFile(game, NULL));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_LOAD_ERROR_NOT_FOUND, RAD_LoadWorldFromFile(NULL, RAD_ASSETS_DIR "/worlds/default.json"));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Die Siegfelder (RAD_Tile_t.victory): gesetzt, wo die Datei sie nennt, sonst
/// nicht -- auch nicht von einer Welt davor. Ein Feld, das nur zum Siegfeld wird,
/// meldet "changed".
///
void test_weltdefinition_setzt_die_siegfelder(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK,
        lade(game, "{\"version\":1,\"rows\":[\"...\",\"...\"],"
                   "\"victory_tiles\":[{\"x\":2,\"y\":0},{\"y\":1,\"x\":0}]}"));
    TEST_ASSERT_TRUE(game->world.tiles[0][2].victory);
    TEST_ASSERT_TRUE(game->world.tiles[1][0].victory);
    TEST_ASSERT_FALSE(game->world.tiles[0][0].victory);
    TEST_ASSERT_FALSE(game->world.tiles[1][2].victory);

    // Dieselbe Welt, ein Siegfeld weniger und eins mehr: zwei Felder aendern sich.
    RAD_WeltMitschrift_t mitschrift;
    abonniere(events, &mitschrift);
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK,
        lade(game, "{\"version\":1,\"rows\":[\"...\",\"...\"],\"victory_tiles\":[{\"x\":2,\"y\":0},{\"x\":1,\"y\":1}]}"));
    TEST_ASSERT_FALSE(game->world.tiles[1][0].victory);
    TEST_ASSERT_TRUE(game->world.tiles[1][1].victory);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.changed);

    // Ohne "victory_tiles": keine, und auch eine leere Liste ist gueltig.
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game, "{\"version\":1,\"rows\":[\"...\",\"...\"]}"));
    TEST_ASSERT_FALSE(game->world.tiles[0][2].victory);
    TEST_ASSERT_FALSE(game->world.tiles[1][1].victory);
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game, "{\"version\":1,\"rows\":[\".\"],\"victory_tiles\":[]}"));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Jedes Zeichen aus world.schema.json wird zu seinem Tile-Typ.
///
void test_weltdefinition_kennt_alle_gelaendearten(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);

    // Zwei Zeilen zu fuenf: alle zehn passen nicht in RAD_WORLD_WIDTH.
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, lade(game, "{\"version\":1,\"rows\":[\"_.~,:\",\"^=H+*\"]}"));

    const RAD_TileType_t erwartet[2][5] = {
        { RAD_TILE_TYPE_VOID, RAD_TILE_TYPE_GROUND, RAD_TILE_TYPE_WATER_SHALLOW, RAD_TILE_TYPE_MUD, RAD_TILE_TYPE_MEADOW },
        { RAD_TILE_TYPE_FOREST, RAD_TILE_TYPE_WATER_DEEP, RAD_TILE_TYPE_CITY, RAD_TILE_TYPE_ROAD, RAD_TILE_TYPE_FORTIFICATION }
    };
    for(int32_t y=0;y < 2; ++y)
    {
        for(int32_t x=0;x < 5; ++x)
        {
            TEST_ASSERT_EQUAL_INT(erwartet[y][x], game->world.tiles[y][x].type);
        }
    }

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Ein Siegfeld darf auf jedem Land liegen, aber nicht im Wasser -- seicht oder
/// tief -- und nicht auf einem Feld ohne Gelaende.
///
void test_weltdefinition_siegfelder_nur_auf_land(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK,
        lade(game, "{\"version\":1,\"rows\":[\".,:^H+*\"],"
                   "\"victory_tiles\":[{\"x\":0,\"y\":0},{\"x\":1,\"y\":0},{\"x\":2,\"y\":0},{\"x\":3,\"y\":0},"
                   "{\"x\":4,\"y\":0},{\"x\":5,\"y\":0},{\"x\":6,\"y\":0}]}"));
    for(int32_t x=0;x < 7; ++x)
    {
        TEST_ASSERT_TRUE(game->world.tiles[0][x].victory);
    }

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_ERROR_VICTORY_NO_GROUND,
        lade(game, "{\"version\":1,\"rows\":[\".=\"],\"victory_tiles\":[{\"x\":1,\"y\":0}]}"));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}
