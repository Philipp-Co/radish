#include <unity.h>
#include <string.h>

#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/serialization/serialization.h>

///
/// Die Tile-Ereignisse beim Laden eines Spielstands (serialization.c).
///
/// Ein Abonnent kennt die Welt aus RAD_CreateGame. Ein Ladevorgang ersetzt sie als
/// Ganzes, und er muss danach wieder richtig liegen: gemeldet werden genau die
/// Felder, die anders sind, mit ihrem neuen Stand -- und ein misslungener
/// Ladevorgang meldet gar nichts, denn am Spiel hat sich nichts geaendert.
///
/// Geladen wird aus einem String und nicht aus einer Datei: die Datei ist hier
/// nicht die Frage, und RAD_LoadGameFromFile reicht nur an
/// RAD_DeserializeGameFromJson weiter.
///

typedef struct
{
    int added;
    int removed;
    int changed;

    // Ob jeder Zeiger in die Welt des Spiels zeigte und nicht in den
    // Zwischenstand, der nach dem Laden freigegeben wird.
    bool alle_im_spiel;
    RAD_Game_t *game;
} RAD_LadeMitschrift_t;

static bool zeigt_ins_spiel(RAD_LadeMitschrift_t *mitschrift, const RAD_Tile_t *tile)
{
    const RAD_Tile_t *anfang = &mitschrift->game->world.tiles[0][0];
    const RAD_Tile_t *ende = anfang + (RAD_WORLD_WIDTH * RAD_WORLD_HEIGHT);
    return (tile >= anfang) && (tile < ende);
}

static void lade_added(void *user_argument, const RAD_Tile_t *tile)
{
    RAD_LadeMitschrift_t *mitschrift = user_argument;
    mitschrift->added += 1;
    mitschrift->alle_im_spiel = mitschrift->alle_im_spiel && zeigt_ins_spiel(mitschrift, tile);
}

static void lade_removed(void *user_argument, const RAD_Tile_t *tile)
{
    RAD_LadeMitschrift_t *mitschrift = user_argument;
    mitschrift->removed += 1;
    mitschrift->alle_im_spiel = mitschrift->alle_im_spiel && zeigt_ins_spiel(mitschrift, tile);
}

static void lade_changed(void *user_argument, const RAD_Tile_t *tile)
{
    RAD_LadeMitschrift_t *mitschrift = user_argument;
    mitschrift->changed += 1;
    mitschrift->alle_im_spiel = mitschrift->alle_im_spiel && zeigt_ins_spiel(mitschrift, tile);

    // Gemeldet wird der neue Stand, nicht der Boden aus dem Zuruecksetzen.
    if(tile->x == 2 && tile->y == 2)
    {
        TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER, tile->type);
    }
}

static void abonniere(RAD_EventManager_t *events, RAD_Game_t *game, RAD_LadeMitschrift_t *mitschrift)
{
    *mitschrift = (RAD_LadeMitschrift_t){
        .added = 0,
        .removed = 0,
        .changed = 0,
        .alle_im_spiel = true,
        .game = game
    };

    RAD_EventManagerSubscribeToTileEvents(events, (RAD_EventsTileChangedCallback_t){
        .user_argument = mitschrift,
        .added = lade_added,
        .removed = lade_removed,
        .changed = lade_changed
    });
}

static char spielstand[RAD_SAVE_JSON_MAX];

///
/// Ein Spielstand mit einem leeren Feld (1,1) und Wasser auf (2,2), geladen in ein
/// frisches Spiel aus lauter Boden: ein removed und ein changed, sonst nichts --
/// und nicht 64-mal added fuer Boden, den es danach gar nicht gibt.
///
void test_laden_meldet_nur_die_geaenderten_tiles(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *quelle = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(quelle);
    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&quelle->world, 1, 1));
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&quelle->world, 2, 2, 0, RAD_TILE_TYPE_WATER));

    size_t laenge = 0;
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, RAD_SerializeGameToJson(quelle, spielstand, sizeof(spielstand), false, &laenge));
    RAD_DestroyGame(&quelle);

    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);

    RAD_LadeMitschrift_t mitschrift;
    abonniere(events, game, &mitschrift);

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, RAD_DeserializeGameFromJson(game, spielstand, laenge));

    TEST_ASSERT_EQUAL_INT(0, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.changed);
    TEST_ASSERT_TRUE(mitschrift.alle_im_spiel);

    // Und derselbe Stand ein zweites Mal geladen meldet nichts mehr.
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, RAD_DeserializeGameFromJson(game, spielstand, laenge));
    TEST_ASSERT_EQUAL_INT(0, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.changed);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Ein Spielstand, der erst am Ende abgelehnt wird -- die Welt ist dann schon
/// gelesen und aufgebaut. Das Spiel bleibt, wie es war, und gemeldet wird nichts.
///
void test_misslungenes_laden_meldet_nichts(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&game->world, 4, 4));

    size_t laenge = 0;
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, RAD_SerializeGameToJson(game, spielstand, sizeof(spielstand), false, &laenge));

    // Die Figur steht auf einem Feld, das im Spielstand keine Figur fuehrt: die
    // Gegenprobe in RAD_DeserializeWorld lehnt das nach dem Aufbau ab.
    const char *vorher = "\"entities\":[]";
    const char *nachher = "\"entities\":[{\"id\":0,\"type\":\"player\",\"owner\":null,\"x\":0,\"y\":0}]";
    char *stelle = strstr(spielstand, vorher);
    TEST_ASSERT_NOT_NULL(stelle);
    char rest[RAD_SAVE_JSON_MAX];
    strcpy(rest, stelle + strlen(vorher));
    strcpy(stelle, nachher);
    strcat(stelle, rest);
    laenge = strlen(spielstand);

    RAD_LadeMitschrift_t mitschrift;
    abonniere(events, game, &mitschrift);

    TEST_ASSERT_NOT_EQUAL(RAD_SERIALIZE_OK, RAD_DeserializeGameFromJson(game, spielstand, laenge));

    TEST_ASSERT_EQUAL_INT(0, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.changed);
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_VOID, game->world.tiles[4][4].type);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Seit Version 2 traegt der Spielstand die Hoehe jedes Feldes und eine Welt, die
/// kleiner ist als die groesste. Beides kommt aus der Datei so zurueck, wie es
/// hineinging.
///
void test_laden_behaelt_hoehen_und_groesse(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *quelle = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(quelle);
    TEST_ASSERT_TRUE(RAD_ResetWorldToSize(&quelle->world, 3, 2));
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&quelle->world, 2, 1, 4, RAD_TILE_TYPE_WATER));
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&quelle->world, 0, 0, 1, RAD_TILE_TYPE_GROUND));

    size_t laenge = 0;
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, RAD_SerializeGameToJson(quelle, spielstand, sizeof(spielstand), false, &laenge));
    RAD_DestroyGame(&quelle);

    TEST_ASSERT_NOT_NULL(strstr(spielstand, "\"version\":2"));

    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, RAD_DeserializeGameFromJson(game, spielstand, laenge));

    TEST_ASSERT_EQUAL_INT(3, RAD_GameWorldWidth(game));
    TEST_ASSERT_EQUAL_INT(2, RAD_GameWorldHeight(game));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER, game->world.tiles[1][2].type);
    TEST_ASSERT_EQUAL_INT(4, game->world.tiles[1][2].z);
    TEST_ASSERT_EQUAL_INT(1, game->world.tiles[0][0].z);
    TEST_ASSERT_EQUAL_INT(0, game->world.tiles[0][1].z);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Ein Spielstand aus Version 1 kennt kein z. Er wird weiter gelesen, und seine
/// Felder stehen auf Hoehe 0.
///
void test_laden_liest_version_1_ohne_hoehen(void)
{
    const char *version_1 =
        "{\"format\":\"radish-save\",\"version\":1,\"game\":{\"world\":{"
        "\"width\":2,\"height\":1,"
        "\"tiles\":[[{\"x\":0,\"y\":0,\"type\":\"water\",\"entity\":null},"
                    "{\"x\":1,\"y\":0,\"type\":\"ground\",\"entity\":null}]],"
        "\"entities\":[]}}}";

    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
    TEST_ASSERT_TRUE(RAD_WorldAddTile(&game->world, 0, 0, 3, RAD_TILE_TYPE_GROUND));

    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_OK, RAD_DeserializeGameFromJson(game, version_1, strlen(version_1)));

    TEST_ASSERT_EQUAL_INT(2, RAD_GameWorldWidth(game));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER, game->world.tiles[0][0].type);
    TEST_ASSERT_EQUAL_INT(0, game->world.tiles[0][0].z);

    // Und eine Version, die es noch nicht gibt, bleibt abgelehnt.
    char version_3[512];
    strcpy(version_3, version_1);
    char *stelle = strstr(version_3, "\"version\":1");
    TEST_ASSERT_NOT_NULL(stelle);
    stelle[strlen("\"version\":")] = '3';
    TEST_ASSERT_EQUAL_INT(RAD_SERIALIZE_ERROR_VERSION, RAD_DeserializeGameFromJson(game, version_3, strlen(version_3)));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}
