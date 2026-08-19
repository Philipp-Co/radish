#include <unity.h>
#include <stdlib.h>

#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand -- hier sogar
// zwingend: RAD_GameAddTile und RAD_GameRemoveTile stehen im privaten Header und
// waeren von aussen nicht zu rufen. Die Begruendung fuer diesen Suchpfad steht in
// test/CMakeLists.txt.
#include <radish/game/model/game.h>

///
/// Die Schreibseite der Tiles am Spiel (tile.c) gegen die Leseseite geprueft: was
/// hineingeschrieben wird, muss ueber RAD_GameTileAt wieder herauskommen.
///
/// Die Regeln selbst -- welcher Uebergang welches Ereignis meldet, warum eine Figur
/// ihr Gelaende haelt -- stehen in der Welt und sind in test/model/test_world.c
/// geprueft. Hier steht nur, dass die Fassade wirklich dorthin durchreicht.
///

static RAD_EventManager_t *events;
static RAD_Game_t *game;

static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
}

static void abbauen(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Zeilenweise, Zeile 0 zuerst -- derselbe Index, den RAD_GameTileAt erwartet.
///
static int32_t index_von(int32_t x, int32_t y)
{
    return (y * RAD_WORLD_WIDTH) + x;
}

///
/// Was beim Abonnenten ankommt: welches der drei Ereignisse, und mit welchem Tile.
///
/// Das Tile wird **kopiert** und nicht als Zeiger festgehalten. Ein Ereignis reicht
/// einen const RAD_Tile_t* in die Welt, und der gilt nur, solange der Callback
/// laeuft -- danach dreht sich die Welt weiter. Dieselbe Ueberlegung, aus der die
/// Leseseite Kopien herausgibt (tile.h).
///
typedef struct
{
    int added;
    int removed;
    int changed;
    RAD_Tile_t letztes;
} RAD_TileMitschrift_t;

static void notiere_added(void *user_argument, const RAD_Tile_t *tile)
{
    // Ein Ereignis ohne Tile waere keines: wer darauf hin zeichnen soll, muss
    // lesen koennen, wo das Feld liegt.
    TEST_ASSERT_NOT_NULL(tile);

    RAD_TileMitschrift_t *mitschrift = user_argument;
    mitschrift->added += 1;
    mitschrift->letztes = *tile;
}

static void notiere_removed(void *user_argument, const RAD_Tile_t *tile)
{
    TEST_ASSERT_NOT_NULL(tile);

    RAD_TileMitschrift_t *mitschrift = user_argument;
    mitschrift->removed += 1;
    mitschrift->letztes = *tile;
}

static void notiere_changed(void *user_argument, const RAD_Tile_t *tile)
{
    TEST_ASSERT_NOT_NULL(tile);

    RAD_TileMitschrift_t *mitschrift = user_argument;
    mitschrift->changed += 1;
    mitschrift->letztes = *tile;
}

///
/// Abonniert wird immer *nach* RAD_CreateGame: der Aufbau der Welt veroeffentlicht
/// ein added-Ereignis je Feld, und die 64 gehen diesen Test nichts an.
///
static void abonnieren(RAD_TileMitschrift_t *mitschrift)
{
    *mitschrift = (RAD_TileMitschrift_t){ .added = 0, .removed = 0, .changed = 0 };

    RAD_EventManagerSubscribeToTileEvents(events, (RAD_EventsTileChangedCallback_t){
        .user_argument = mitschrift,
        .added = notiere_added,
        .removed = notiere_removed,
        .changed = notiere_changed
    });
}


void test_tile_hinzufuegen_geht_durch_bis_in_die_welt(void)
{
    aufbauen();

    TEST_ASSERT_TRUE(RAD_GameAddTile(game, 4, 2, 1, RAD_TILE_TYPE_WATER));

    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, index_von(4, 2), &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER, tile.type);
    TEST_ASSERT_EQUAL_INT(1, tile.z);
    TEST_ASSERT_EQUAL_INT(4, tile.x);
    TEST_ASSERT_EQUAL_INT(2, tile.y);

    abbauen();
}

void test_tile_entfernen_macht_void(void)
{
    aufbauen();

    TEST_ASSERT_TRUE(RAD_GameRemoveTile(game, 4, 2));

    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, index_von(4, 2), &tile));
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_VOID, tile.type);

    // Das Raster bleibt vollstaendig besetzt: ein leeres Feld ist ein Feld.
    TEST_ASSERT_EQUAL_INT(RAD_WORLD_WIDTH * RAD_WORLD_HEIGHT, RAD_GameNumberOfTiles(game));

    abbauen();
}

void test_tile_ohne_spiel_ist_kein_absturz(void)
{
    TEST_ASSERT_FALSE(RAD_GameAddTile(NULL, 0, 0, 0, RAD_TILE_TYPE_GROUND));
    TEST_ASSERT_FALSE(RAD_GameRemoveTile(NULL, 0, 0));
}

///
/// Die ganze Strecke: RAD_GameRemoveTile und RAD_GameAddTile bis zum Abonnenten.
///
/// Geprueft wird nicht nur, *dass* etwas ankommt, sondern was: das Ereignis passt
/// zum Uebergang, und das mitgereichte Tile traegt den Stand von *nach* der
/// Aenderung. Ein Abonnent zeichnet daraufhin -- er muss sehen, was jetzt dasteht,
/// und nicht, was dort stand.
///
void test_tile_ereignisse_kommen_beim_abonnenten_an(void)
{
    aufbauen();

    RAD_TileMitschrift_t mitschrift;
    abonnieren(&mitschrift);

    // Aus RAD_InitWorld steht auf (2,3) GROUND. Wegnehmen ist damit ein Entfernen.
    TEST_ASSERT_TRUE(RAD_GameRemoveTile(game, 2, 3));

    TEST_ASSERT_EQUAL_INT(1, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.changed);

    // Und zwar von dem Feld, das gemeint war, mit dem Typ, den es jetzt hat.
    TEST_ASSERT_EQUAL_INT(2, mitschrift.letztes.x);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.letztes.y);
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_VOID, mitschrift.letztes.type);

    // Auf das leere Feld Gelaende stellen: ein Zuwachs, kein Wechsel.
    TEST_ASSERT_TRUE(RAD_GameAddTile(game, 2, 3, 5, RAD_TILE_TYPE_WATER));

    TEST_ASSERT_EQUAL_INT(1, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.changed);

    TEST_ASSERT_EQUAL_INT(2, mitschrift.letztes.x);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.letztes.y);
    TEST_ASSERT_EQUAL_INT(RAD_TILE_TYPE_WATER, mitschrift.letztes.type);
    TEST_ASSERT_EQUAL_INT(5, mitschrift.letztes.z);

    abbauen();
}

///
/// Kein Ereignis, wo nichts geschieht -- die Kehrseite des Tests darueber.
///
/// Ein Abonnent, der auf jedes Ereignis hin neu zeichnet, zeichnet sonst umsonst;
/// schlimmer, er kann ein "removed" fuer ein Feld bekommen, das schon leer war, und
/// haelt es fuer eine Aenderung.
///
void test_tile_ohne_aenderung_kommt_kein_ereignis_an(void)
{
    aufbauen();

    RAD_TileMitschrift_t mitschrift;
    abonnieren(&mitschrift);

    // Derselbe Typ und dieselbe Hoehe, die RAD_InitWorld gesetzt hat.
    TEST_ASSERT_TRUE(RAD_GameAddTile(game, 0, 0, 0, RAD_TILE_TYPE_GROUND));

    // Zweimal entfernen: die zweite Runde findet nichts mehr vor.
    TEST_ASSERT_TRUE(RAD_GameRemoveTile(game, 0, 0));
    TEST_ASSERT_TRUE(RAD_GameRemoveTile(game, 0, 0));

    // Ausserhalb der Welt gibt es kein Feld, ueber das sich etwas melden liesse.
    TEST_ASSERT_FALSE(RAD_GameRemoveTile(game, RAD_WORLD_WIDTH, 0));
    TEST_ASSERT_FALSE(RAD_GameAddTile(game, -1, 0, 0, RAD_TILE_TYPE_WATER));

    TEST_ASSERT_EQUAL_INT(1, mitschrift.removed);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.added);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.changed);

    abbauen();
}
