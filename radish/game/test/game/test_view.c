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
    RAD_Tile_t tile = { .x = -42, .y = -42, .z = -42, .type = RAD_TILE_TYPE_WATER, .entity = 7 };
    TEST_ASSERT_FALSE(RAD_GameTileAt(NULL, 0, 0, &tile));
    TEST_ASSERT_EQUAL_INT(-42, tile.x);
    TEST_ASSERT_EQUAL_INT(-42, tile.y);
    TEST_ASSERT_EQUAL_INT(7, tile.entity);

    RAD_Entity_t entity = { .id = -42, .health = 99 };
    TEST_ASSERT_FALSE(RAD_GameEntityAt(game, 0, &entity));
    TEST_ASSERT_EQUAL_INT(-42, entity.id);
    TEST_ASSERT_EQUAL_INT(99, entity.health);

    abbauen();
}

void test_view_zaehlt_nur_vorhandene_entitaeten(void)
{
    aufbauen();

    // Ein frisches Spiel hat keine Figuren -- der Pool ist leer, nicht luecklenhaft.
    TEST_ASSERT_EQUAL_INT(0, RAD_GameNumberOfEntities(game));

    RAD_Entity_t entity;
    TEST_ASSERT_FALSE(RAD_GameEntityAt(game, 0, &entity));

    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_NPC, 1, 1));
    TEST_ASSERT_EQUAL_INT(1, RAD_GameNumberOfEntities(game));

    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_PLAYER, 2, 3));
    TEST_ASSERT_EQUAL_INT(2, RAD_GameNumberOfEntities(game));

    abbauen();
}

void test_view_index_ist_dicht_auch_bei_luecken_im_pool(void)
{
    aufbauen();

    // Drei Figuren setzen, die mittlere entfernen: der Pool hat danach eine Luecke,
    // der Index der Fassade darf keine haben.
    const RAD_EntityId_t first  = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_NPC, 0, 0);
    const RAD_EntityId_t middle = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_NPC, 1, 0);
    const RAD_EntityId_t last   = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_NPC, 2, 0);
    TEST_ASSERT_EQUAL_INT(3, RAD_GameNumberOfEntities(game));

    RAD_WorldRemoveEntity(&game->world, middle);
    TEST_ASSERT_EQUAL_INT(2, RAD_GameNumberOfEntities(game));

    // Index 0 und 1 treffen die zwei uebrigen, aufsteigend nach Id -- die
    // entfernte kommt nicht mehr vor, und hinter Index 1 ist Schluss.
    RAD_Entity_t at_zero;
    RAD_Entity_t at_one;
    TEST_ASSERT_TRUE(RAD_GameEntityAt(game, 0, &at_zero));
    TEST_ASSERT_TRUE(RAD_GameEntityAt(game, 1, &at_one));
    TEST_ASSERT_EQUAL_INT(first, at_zero.id);
    TEST_ASSERT_EQUAL_INT(last, at_one.id);

    RAD_Entity_t beyond;
    TEST_ASSERT_FALSE(RAD_GameEntityAt(game, 2, &beyond));

    abbauen();
}

void test_view_gibt_eine_kopie_heraus(void)
{
    aufbauen();

    const RAD_EntityId_t id = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_NPC, 4, 5);
    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, id);

    // Die eigentliche Zusage: was der Aufrufer bekommt, haengt nicht an der Welt.
    // In seinen Stand geschrieben zu haben, aendert dort nichts.
    RAD_Entity_t mine;
    TEST_ASSERT_TRUE(RAD_GameEntityAt(game, 0, &mine));
    TEST_ASSERT_EQUAL_INT(id, mine.id);

    mine.id = RAD_ENTITY_NONE;
    mine.health = 12345;

    RAD_Entity_t again;
    TEST_ASSERT_TRUE(RAD_GameEntityAt(game, 0, &again));
    TEST_ASSERT_EQUAL_INT(id, again.id);
    TEST_ASSERT_NOT_EQUAL(12345, again.health);

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
    const RAD_EntityId_t id = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_PLAYER, 3, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, id);

    RAD_Entity_t entity;
    TEST_ASSERT_TRUE(RAD_GameEntityAt(game, 0, &entity));
    TEST_ASSERT_EQUAL_INT(3, entity.x);
    TEST_ASSERT_EQUAL_INT(2, entity.y);

    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_GameTileAt(game, entity.x, entity.y, &tile));
    TEST_ASSERT_EQUAL_INT(id, tile.entity);

    abbauen();
}
