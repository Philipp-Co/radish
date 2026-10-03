#include <string.h>
#include <unity.h>
#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand: er setzt Figuren
// ueber die Welt, um zu pruefen, was die Fassade davon herausgibt. Die Begruendung
// fuer diesen Suchpfad steht in test/CMakeLists.txt.
#include <radish/game/model/game.h>

///
/// Die Leseseite (view.c) gegen den Zustand geprueft, den sie beschreibt.
///
/// Zwei Fragen ziehen sich durch alle Tests: gibt sie *heraus*, was drinsteht, und
/// gibt sie eine *Kopie*? Das zweite ist die eigentliche Zusage -- ein Stand, der
/// sich nicht in die Welt zurueckschreibt.
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


void test_view_zaehlt_alle_tiles(void)
{
    aufbauen();

    // Das Raster ist vollstaendig besetzt: die Anzahl ist seine Groesse.
    TEST_ASSERT_EQUAL_INT(RAD_WORLD_WIDTH * RAD_WORLD_HEIGHT, RAD_GameNumberOfTiles(game));

    abbauen();
}

void test_view_liefert_jedes_tile_mit_seiner_position(void)
{
    aufbauen();

    // Das ganze Raster durchgefragt, zeilenweise und Zeile 0 zuerst. Geprueft
    // wird am Tile selbst, denn es traegt seine Position: was zurueckkommt, muss
    // das Feld sein, nach dem gefragt wurde, und nicht sein Nachbar.
    for(int16_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int16_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            RAD_Tile_t tile;
            TEST_ASSERT_TRUE(RAD_GameTileAt(game, x, y, &tile));
            TEST_ASSERT_EQUAL_INT(x, tile.x);
            TEST_ASSERT_EQUAL_INT(y, tile.y);
        }
    }

    abbauen();
}

void test_view_weist_ungueltige_tile_zugriffe_ab(void)
{
    aufbauen();

    RAD_Tile_t tile;

    // Abgewiesen wird am Zeiger, und nur dort: seit die Fassade nach x und y
    // fragt statt nach einem Index, ist eine Stelle ausserhalb der Welt kein
    // Fall mehr, den view.c beantwortet, sondern einer, den es per assert stellt
    // (tile.h). Ein Test dafuer muesste den Abbruch abfangen -- das kann Unity
    // nicht, und ein Kontrakt, der beim Aufrufer liegt, hat in der Rueckgabe
    // auch nichts verloren.
    TEST_ASSERT_FALSE(RAD_GameTileAt(game, 0, 0, NULL));
    TEST_ASSERT_FALSE(RAD_GameTileAt(NULL, 0, 0, &tile));

    // Ein NULL-Spiel hat nichts, und zwar ohne zu stuerzen.
    TEST_ASSERT_EQUAL_INT(0, RAD_GameNumberOfTiles(NULL));

    abbauen();
}

void test_view_laesst_output_bei_ablehnung_unberuehrt(void)
{
    aufbauen();

    // Die Zusage aus game.h: ein abgelehnter Aufruf schreibt nichts halb hinein.
    // Abgelehnt wird ueber das NULL-Spiel -- der einzige Weg, den es dafuer noch
    // gibt, seit die Stelle selbst per assert geprueft wird.
    RAD_Tile_t tile = { .x = -42, .y = -42, .z = -42, .type = RAD_TILE_TYPE_WATER, .unit = 7 };
    TEST_ASSERT_FALSE(RAD_GameTileAt(NULL, 0, 0, &tile));
    TEST_ASSERT_EQUAL_INT(-42, tile.x);
    TEST_ASSERT_EQUAL_INT(-42, tile.y);
    TEST_ASSERT_EQUAL_INT(7, tile.unit);

    RAD_Unit_t unit = { .id = -42, .movement = 99 };
    TEST_ASSERT_FALSE(RAD_GameUnitAt(game, 0, &unit));
    TEST_ASSERT_EQUAL_INT(-42, unit.id);
    TEST_ASSERT_EQUAL_INT(99, unit.movement);

    abbauen();
}

void test_view_zaehlt_nur_vorhandene_einheiten(void)
{
    aufbauen();

    // Ein frisches Spiel hat keine Figuren -- der Pool ist leer, nicht luecklenhaft.
    TEST_ASSERT_EQUAL_INT(0, RAD_GameNumberOfUnits(game));

    RAD_Unit_t unit;
    TEST_ASSERT_FALSE(RAD_GameUnitAt(game, 0, &unit));

    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_NPC, 1, 1));
    TEST_ASSERT_EQUAL_INT(1, RAD_GameNumberOfUnits(game));

    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, 2, 3));
    TEST_ASSERT_EQUAL_INT(2, RAD_GameNumberOfUnits(game));

    abbauen();
}

void test_view_zaehlt_einheiten_in_jedem_zustand(void)
{
    aufbauen();

    // Drei Einheiten, eine in jedem Zustand: auf dem Feld, in der Reserve und
    // zerstoert. Gezaehlt werden alle drei -- die Fassade gibt die Einheitenliste
    // heraus und nicht nur das Feld.
    RAD_Unit_t values = { .number_of_members = 1 };
    strcpy(values.name, "Trupp");
    strcpy(values.members[0].profile, "Soldat");

    const RAD_UnitId_t deployed  = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_NPC, 0, 0);
    const RAD_UnitId_t reserve   = RAD_WorldAddReserveUnit(&game->world, &values, RAD_USER_NONE);
    const RAD_UnitId_t destroyed = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_NPC, 2, 0);
    RAD_WorldRemoveUnit(&game->world, destroyed);

    TEST_ASSERT_EQUAL_INT(3, RAD_GameNumberOfUnits(game));

    // Aufsteigend nach Id, und die zerstoerte behaelt ihren Platz -- ihre Id wird
    // nicht neu vergeben, also faellt sie auch aus dem Index nicht heraus.
    RAD_Unit_t at[3];
    for(int32_t i=0;i < 3; ++i)
    {
        TEST_ASSERT_TRUE(RAD_GameUnitAt(game, i, &at[i]));
    }
    TEST_ASSERT_EQUAL_INT(deployed, at[0].id);
    TEST_ASSERT_EQUAL_INT(reserve, at[1].id);
    TEST_ASSERT_EQUAL_INT(destroyed, at[2].id);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, at[0].state);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, at[1].state);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DESTROYED, at[2].state);

    RAD_Unit_t beyond;
    TEST_ASSERT_FALSE(RAD_GameUnitAt(game, 3, &beyond));

    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    abbauen();
}

void test_view_gibt_eine_kopie_heraus(void)
{
    aufbauen();

    const RAD_UnitId_t id = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_NPC, 4, 5);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, id);

    // Die eigentliche Zusage: was der Aufrufer bekommt, haengt nicht an der Welt.
    // In seinen Stand geschrieben zu haben, aendert dort nichts.
    RAD_Unit_t mine;
    TEST_ASSERT_TRUE(RAD_GameUnitAt(game, 0, &mine));
    TEST_ASSERT_EQUAL_INT(id, mine.id);

    mine.id = RAD_UNIT_NONE;
    mine.movement = 12345;

    RAD_Unit_t again;
    TEST_ASSERT_TRUE(RAD_GameUnitAt(game, 0, &again));
    TEST_ASSERT_EQUAL_INT(id, again.id);
    TEST_ASSERT_NOT_EQUAL(12345, again.movement);

    // Dasselbe fuer ein Tile, ueber seine Position gegengeprueft.
    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 0, 0, &tile));
    tile.x = 999;

    RAD_Tile_t tile_again;
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, 0, 0, &tile_again));
    TEST_ASSERT_EQUAL_INT(0, tile_again.x);

    // Und die Welt selbst ist unversehrt: die Doppelbuchfuehrung stimmt noch.
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    abbauen();
}

void test_view_findet_die_gesetzte_figur_auf_ihrem_tile(void)
{
    aufbauen();

    // Die zwei Seiten gegeneinander: die Figur kennt ihr Feld, das Feld die Figur.
    // Beides muss durch die Fassade gleich herauskommen.
    const RAD_UnitId_t id = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, 3, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, id);

    RAD_Unit_t unit;
    TEST_ASSERT_TRUE(RAD_GameUnitAt(game, 0, &unit));
    TEST_ASSERT_EQUAL_INT(3, unit.x);
    TEST_ASSERT_EQUAL_INT(2, unit.y);

    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, unit.x, unit.y, &tile));
    TEST_ASSERT_EQUAL_INT(id, tile.unit);

    abbauen();
}
