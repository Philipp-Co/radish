#include <unity.h>
#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand: er baut das
// Spielfeld ueber die Welt auf und liest danach in ihr nach, was das Kommando
// wirklich hinterlassen hat. Die Begruendung fuer diesen Suchpfad steht in
// test/CMakeLists.txt.
#include <radish/game/model/game.h>
#include <radish/game/model/world/world.h>

///
/// Was ein Move-Kommando in der Welt anrichtet -- drei Spielfelder, drei Ausgaenge.
///
///     eine Figur, freier Weg          sie geht
///     eine Figur auf dem Zielfeld     sie bleibt stehen
///     Ziel ausserhalb der Welt        sie bleibt stehen
///
/// Geprueft wird beides, das Ergebnis und das Ereignis: die Welt sagt, wo die
/// Figur danach steht, das Ereignis, was ein Abonnent davon erfaehrt. Ein
/// abgelehnter Zug muss an beiden Stellen erkennbar sein -- der Client zeichnet
/// auf das Ereignis hin und sieht in die Welt nicht hinein.
///
/// Die Nachbardatei test_game.c prueft dieselbe Strecke fuer die Faelle, in denen
/// es die Figur gar nicht gibt; hier steht das Spielfeld im Mittelpunkt.
///

///
/// Mitschrift des moved-Ereignisses. Der Zeiger auf die Figur wird geprueft und
/// nicht blind dereferenziert: ein abgewiesenes Kommando kann eines *ohne* Figur
/// veroeffentlichen, und ein Test, der daran abstuerzt, sagt nichts mehr.
///
typedef struct
{
    int moved;
    bool mit_figur;
    RAD_EntityId_t id;
    int16_t x;
    int16_t y;
    RAD_EntityPath_t pfad;
    int32_t ergebnis;
} RAD_ZugMitschrift_t;

static void notiere_zug(void *user_argument, const RAD_Entity_t *entity, const RAD_EntityPath_t *path, int32_t result)
{
    // Ohne Pfad waere das Ereignis keine Bewegung.
    TEST_ASSERT_NOT_NULL(path);

    RAD_ZugMitschrift_t *mitschrift = user_argument;
    mitschrift->moved += 1;
    mitschrift->pfad = *path;
    mitschrift->ergebnis = result;
    mitschrift->mit_figur = (entity != NULL);

    if(entity != NULL)
    {
        mitschrift->id = entity->id;
        mitschrift->x = entity->x;
        mitschrift->y = entity->y;
    }
}

static void notiere_nichts(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)entity;
    (void)x;
    (void)y;
}

///
/// Eine Gruppe wird als Ganzes gesetzt, also stehen hier auch die zwei Callbacks,
/// die diese Datei nicht braucht (event_manager.h). Abonniert wird immer erst,
/// wenn das Spielfeld steht -- die spawned-Ereignisse der aufgestellten Figuren
/// gehen diese Tests nichts an.
///
static void abonniere_zug(RAD_EventManager_t *events, RAD_ZugMitschrift_t *mitschrift)
{
    *mitschrift = (RAD_ZugMitschrift_t){ .moved = 0, .mit_figur = false };

    RAD_EventManagerSubscribeToEntityEvents(events, (RAD_EventsEntityChangedCallback_t){
        .user_argument = mitschrift,
        .spawned = notiere_nichts,
        .destroyed = notiere_nichts,
        .moved = notiere_zug
    });
}

///
/// Steht die Figur dort, und traegt das Feld sie? Beide Seiten der
/// Doppelbuchfuehrung, nicht nur eine: die Figur kennt ihre Position, das Tile
/// seine Figur, und auseinanderlaufen duerfen sie nie (world.h).
///
static void pruefe_figur_steht_auf(RAD_Game_t *game, RAD_EntityId_t id, int32_t x, int32_t y)
{
    const RAD_Entity_t *entity = RAD_WorldEntityById(&game->world, id);
    TEST_ASSERT_NOT_NULL(entity);
    TEST_ASSERT_EQUAL_INT(x, entity->x);
    TEST_ASSERT_EQUAL_INT(y, entity->y);

    const RAD_Tile_t *tile = RAD_WorldTileAt(&game->world, x, y);
    TEST_ASSERT_NOT_NULL(tile);
    TEST_ASSERT_EQUAL_INT(id, tile->entity);
}

/// Ist das Feld frei? Nur die Tile-Seite -- die Figur, die es nicht traegt, gibt
/// es nicht zu befragen.
static void pruefe_feld_ist_frei(RAD_Game_t *game, int32_t x, int32_t y)
{
    const RAD_Tile_t *tile = RAD_WorldTileAt(&game->world, x, y);
    TEST_ASSERT_NOT_NULL(tile);
    TEST_ASSERT_EQUAL_INT(RAD_ENTITY_NONE, tile->entity);
}


///
/// **Ein Spielfeld, eine Figur, freier Weg: sie geht.**
///
/// Der Weg hat zwei Schritte, damit er einer ist und nicht bloss ein Ziel: das
/// Kommando benennt jedes betretene Feld einzeln (path.h), und die Figur steht
/// danach auf dem letzten. Die Felder dazwischen laesst sie hinter sich -- auch
/// das wird geprueft, sonst bliebe offen, ob der Zwischenschritt sie festhaelt.
///
void test_move_kommando_bewegt_die_figur(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, (RAD_UserId_t)0x4711);
    TEST_ASSERT_NOT_NULL(game);

    const RAD_EntityId_t figur = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_PLAYER, 2, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, figur);

    RAD_ZugMitschrift_t mitschrift;
    abonniere_zug(events, &mitschrift);

    // (2,2) -> (3,2) -> (3,3). Beide Zielfelder sind leer, dazwischen liegt
    // nichts im Weg.
    const RAD_EntityPath_t weg = {
        .steps_to = { { .x = 3, .y = 2 }, { .x = 3, .y = 3 } },
        .number_of_steps = 2
    };

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveEntity(game, figur, &weg, &command));

    RAD_GameExecuteCommand(game, &command);

    // Die Welt: die Figur steht am Ende des Weges, das Startfeld und der
    // Zwischenschritt sind frei.
    pruefe_figur_steht_auf(game, figur, 3, 3);
    pruefe_feld_ist_frei(game, 2, 2);
    pruefe_feld_ist_frei(game, 3, 2);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    // Das Ereignis: genau eines, mit Erfolg und mit dem ganzen gelaufenen Weg.
    TEST_ASSERT_EQUAL_INT(1, mitschrift.moved);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.ergebnis);
    TEST_ASSERT_TRUE(mitschrift.mit_figur);
    TEST_ASSERT_EQUAL_INT(figur, mitschrift.id);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.x);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.y);

    TEST_ASSERT_EQUAL_INT(2, mitschrift.pfad.number_of_steps);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.pfad.steps_to[0].x);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.pfad.steps_to[0].y);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.pfad.steps_to[1].x);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.pfad.steps_to[1].y);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// **Zwei Figuren, und die eine will auf das Feld der anderen: das geht nicht.**
///
/// Pro Tile hoechstens eine Figur -- die Invariante, die die Welt haelt
/// (world.h). Der Zug endet deshalb vor dem besetzten Feld, und weil es schon das
/// erste ist, bewegt sich gar nichts: beide Figuren stehen danach, wo sie standen.
///
/// Geprueft wird ausdruecklich auch die Figur, die im Weg stand. Sie wird nicht
/// angefasst, verschoben oder ueberschrieben -- ein Zug, der die Belegung des
/// Zielfeldes still ueberschreibt, waere an der Figur, die ihn macht, nicht zu
/// erkennen.
///
void test_move_kommando_auf_besetztes_feld_bewegt_nicht(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, (RAD_UserId_t)0x4711);
    TEST_ASSERT_NOT_NULL(game);

    const RAD_EntityId_t zieher = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_PLAYER, 2, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, zieher);

    // Direkt daneben, auf dem Feld, auf das der Zug gehen soll.
    const RAD_EntityId_t im_weg = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_PLAYER, 3, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, im_weg);
    TEST_ASSERT_NOT_EQUAL(zieher, im_weg);

    RAD_ZugMitschrift_t mitschrift;
    abonniere_zug(events, &mitschrift);

    const RAD_EntityPath_t weg = {
        .steps_to = { { .x = 3, .y = 2 } },
        .number_of_steps = 1
    };

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveEntity(game, zieher, &weg, &command));

    RAD_GameExecuteCommand(game, &command);

    // Die Welt: keine der beiden hat sich bewegt.
    pruefe_figur_steht_auf(game, zieher, 2, 2);
    pruefe_figur_steht_auf(game, im_weg, 3, 2);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    // Das Ereignis: gemeldet wird trotzdem -- ein Absender wartet auf Antwort,
    // und ein abgewiesener Zug darf von einem verlorenen nicht zu unterscheiden
    // sein. Der leere Weg ist, woran ein Abonnent erkennt, dass nichts geschah.
    TEST_ASSERT_EQUAL_INT(1, mitschrift.moved);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.pfad.number_of_steps);
    TEST_ASSERT_TRUE(mitschrift.mit_figur);
    TEST_ASSERT_EQUAL_INT(zieher, mitschrift.id);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.x);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.y);

    // **Befund, keine Zusage:** das Ergebnis im Ereignis ist heute 0 -- dasselbe,
    // was ein gelungener Zug meldet, obwohl die Figur keinen Schritt getan hat.
    // Auseinanderhalten laesst sich beides derzeit nur an der Anzahl der
    // Schritte. Der Test haelt das fest, ohne es zu segnen: aendert sich die
    // Stelle in game.c, faellt er auf, statt still weiterzulaufen.
    TEST_ASSERT_EQUAL_INT(0, mitschrift.ergebnis);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// **Eine Figur, und der Weg fuehrt aus der Welt heraus: das geht nicht.**
///
/// Hinter dem Rand liegt kein Feld, auf das sich treten liesse -- das Raster ist
/// vollstaendig besetzt und endet dort, wo es endet (world.h). Anders als beim
/// besetzten Feld hoert die Bewegung nicht auf, sie kommt gar nicht zustande: die
/// Figur bleibt stehen, und das Ergebnis im Ereignis ist nicht 0.
///
/// Der Zug geht ueber den rechten Rand: die Welt ist RAD_WORLD_WIDTH breit, x = 8
/// liegt also ausserhalb. Geschrieben wird die Zahl trotzdem nicht hin -- die
/// Breite gehoert zu den Regeln und darf sich aendern, ohne dass dieser Test
/// nachgezogen werden muss.
///
void test_move_kommando_ueber_den_rand_bewegt_nicht(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, (RAD_UserId_t)0x4711);
    TEST_ASSERT_NOT_NULL(game);

    // Ganz an den Rand, damit der naechste Schritt schon draussen liegt.
    const int16_t rand_x = (int16_t)(RAD_WORLD_WIDTH - 1);

    const RAD_EntityId_t figur = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_PLAYER, rand_x, 3);
    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, figur);

    RAD_ZugMitschrift_t mitschrift;
    abonniere_zug(events, &mitschrift);

    const RAD_EntityPath_t weg = {
        .steps_to = { { .x = (int16_t)(rand_x + 1), .y = 3 } },
        .number_of_steps = 1
    };
    TEST_ASSERT_FALSE(RAD_WorldInBounds(&game->world, weg.steps_to[0].x, weg.steps_to[0].y));

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveEntity(game, figur, &weg, &command));

    RAD_GameExecuteCommand(game, &command);

    // Die Welt: die Figur steht unveraendert auf ihrem Feld.
    pruefe_figur_steht_auf(game, figur, rand_x, 3);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    // Das Ereignis: leerer Weg und ein Ergebnis ungleich 0 -- hier meldet das
    // Spiel den Fehlschlag auch im Ergebnis, anders als beim besetzten Feld oben.
    TEST_ASSERT_EQUAL_INT(1, mitschrift.moved);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.pfad.number_of_steps);
    TEST_ASSERT_NOT_EQUAL(0, mitschrift.ergebnis);
    TEST_ASSERT_TRUE(mitschrift.mit_figur);
    TEST_ASSERT_EQUAL_INT(figur, mitschrift.id);
    TEST_ASSERT_EQUAL_INT(rand_x, mitschrift.x);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.y);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}
