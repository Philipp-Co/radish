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
    RAD_UnitId_t id;
    int16_t x;
    int16_t y;
    RAD_Path_t pfad;
    int32_t ergebnis;
} RAD_ZugMitschrift_t;

static void notiere_zug(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    // Ohne Pfad waere das Ereignis keine Bewegung.
    TEST_ASSERT_NOT_NULL(path);

    RAD_ZugMitschrift_t *mitschrift = user_argument;
    mitschrift->moved += 1;
    mitschrift->pfad = *path;
    mitschrift->ergebnis = result;
    mitschrift->mit_figur = (unit != NULL);

    if(unit != NULL)
    {
        mitschrift->id = unit->id;
        mitschrift->x = unit->x;
        mitschrift->y = unit->y;
    }
}

static void notiere_nichts(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)unit;
    (void)x;
    (void)y;
}

static void notiere_keinen_angriff(void *user_argument, const RAD_AttackResult_t *result)
{
    (void)user_argument;
    (void)result;
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

    RAD_EventManagerSubscribeToUnitEvents(events, (RAD_EventsUnitChangedCallback_t){
        .user_argument = mitschrift,
        .spawned = notiere_nichts,
        .destroyed = notiere_nichts,
        .moved = notiere_zug,
        .attacked = notiere_keinen_angriff
    });
}

///
/// Steht die Figur dort, und traegt das Feld sie? Beide Seiten der
/// Doppelbuchfuehrung, nicht nur eine: die Figur kennt ihre Position, das Tile
/// seine Figur, und auseinanderlaufen duerfen sie nie (world.h).
///
static void pruefe_figur_steht_auf(RAD_Game_t *game, RAD_UnitId_t id, int32_t x, int32_t y)
{
    const RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, id);
    TEST_ASSERT_NOT_NULL(unit);
    TEST_ASSERT_EQUAL_INT(x, unit->x);
    TEST_ASSERT_EQUAL_INT(y, unit->y);

    const RAD_Tile_t *tile = RAD_WorldTileAt(&game->world, x, y);
    TEST_ASSERT_NOT_NULL(tile);
    TEST_ASSERT_EQUAL_INT(id, tile->unit);
}

///
/// Eine Figur direkt auf dem Feld, mit Bewegung: eine gespawnte hat keine
/// (RAD_WorldSpawnUnit), und ohne waere jeder Weg zu weit (RAD_GameCheckMovePath).
///
#define BEWEGUNG 5

static RAD_UnitId_t spawne(RAD_Game_t *game, int32_t x, int32_t y)
{
    const RAD_UnitId_t id = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, x, y);
    if(id != RAD_UNIT_NONE)
    {
        RAD_WorldUnitById(&game->world, id)->movement = BEWEGUNG;
    }
    return id;
}

/// Ist das Feld frei? Nur die Tile-Seite -- die Figur, die es nicht traegt, gibt
/// es nicht zu befragen.
static void pruefe_feld_ist_frei(RAD_Game_t *game, int32_t x, int32_t y)
{
    const RAD_Tile_t *tile = RAD_WorldTileAt(&game->world, x, y);
    TEST_ASSERT_NOT_NULL(tile);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, tile->unit);
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

    const RAD_UnitId_t figur = spawne(game, 2, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, figur);

    RAD_ZugMitschrift_t mitschrift;
    abonniere_zug(events, &mitschrift);

    // (2,2) -> (3,2) -> (3,3). Das erste Feld ist der Standort und wird nicht auf
    // Belegung geprueft -- dort steht die Figur. Beide folgenden sind leer.
    const RAD_Path_t weg = {
        .steps_to = { { .x = 2, .y = 2 }, { .x = 3, .y = 2 }, { .x = 3, .y = 3 } },
        .number_of_steps = 3
    };

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveUnit(game, figur, &weg, &command));

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

    // Drei Felder: der Standort, der Zwischenschritt und das Ziel.
    TEST_ASSERT_EQUAL_INT(3, mitschrift.pfad.number_of_steps);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.pfad.steps_to[0].x);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.pfad.steps_to[0].y);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.pfad.steps_to[1].x);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.pfad.steps_to[1].y);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.pfad.steps_to[2].x);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.pfad.steps_to[2].y);

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

    const RAD_UnitId_t zieher = spawne(game, 2, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, zieher);

    // Direkt daneben, auf dem Feld, auf das der Zug gehen soll.
    const RAD_UnitId_t im_weg = spawne(game, 3, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, im_weg);
    TEST_ASSERT_NOT_EQUAL(zieher, im_weg);

    RAD_ZugMitschrift_t mitschrift;
    abonniere_zug(events, &mitschrift);

    const RAD_Path_t weg = {
        .steps_to = { { .x = 2, .y = 2 }, { .x = 3, .y = 2 } },
        .number_of_steps = 2
    };

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveUnit(game, zieher, &weg, &command));

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

    const RAD_UnitId_t figur = spawne(game, rand_x, 3);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, figur);

    RAD_ZugMitschrift_t mitschrift;
    abonniere_zug(events, &mitschrift);

    const RAD_Path_t weg = {
        .steps_to = { { .x = rand_x, .y = 3 }, { .x = (int16_t)(rand_x + 1), .y = 3 } },
        .number_of_steps = 2
    };
    // Geprueft wird der Schritt und nicht der Standort: das zweite Feld liegt
    // draussen, das erste ist, wo die Figur steht.
    TEST_ASSERT_TRUE(RAD_WorldInBounds(&game->world, weg.steps_to[0].x, weg.steps_to[0].y));
    TEST_ASSERT_FALSE(RAD_WorldInBounds(&game->world, weg.steps_to[1].x, weg.steps_to[1].y));

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveUnit(game, figur, &weg, &command));

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

///
/// **Eine Einheit in der Reserve: sie geht nicht.**
///
/// Sie ist dem Spiel bekannt, steht aber auf keinem Feld -- ihre Position ist
/// (-1,-1), und ein Weg von dort aus waere ein Griff neben das Raster. Abgelehnt
/// wird wie bei einer Figur, die es nicht gibt: mit einem Fehlschlag im Ereignis
/// und ohne Spur in der Welt.
///
void test_move_kommando_fuer_einheit_in_reserve_bewegt_nicht(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, (RAD_UserId_t)0x4711);
    TEST_ASSERT_NOT_NULL(game);

    RAD_Unit_t values = { .number_of_members = 1, .movement = BEWEGUNG };
    const RAD_UnitId_t reserve = RAD_WorldAddReserveUnit(&game->world, &values, (RAD_UserId_t)0x4711);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, reserve);

    RAD_ZugMitschrift_t mitschrift;
    abonniere_zug(events, &mitschrift);

    const RAD_Path_t weg = {
        .steps_to = { { .x = 0, .y = 0 }, { .x = 1, .y = 0 } },
        .number_of_steps = 2
    };

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveUnit(game, reserve, &weg, &command));

    RAD_GameExecuteCommand(game, &command);

    const RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, reserve);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, unit->state);
    TEST_ASSERT_EQUAL_INT(-1, unit->x);
    TEST_ASSERT_EQUAL_INT(-1, unit->y);
    pruefe_feld_ist_frei(game, 0, 0);
    pruefe_feld_ist_frei(game, 1, 0);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    TEST_ASSERT_EQUAL_INT(1, mitschrift.moved);
    TEST_ASSERT_NOT_EQUAL(0, mitschrift.ergebnis);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.pfad.number_of_steps);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// **Was die Einheit im Zug schon getan hat** (RAD_Unit_t.turn): je Zug zieht
/// sie einmal, im Zug ihres Aufstellens gar nicht, und mit dem Zugwechsel ist
/// es vergessen. Ein Spieler genuegt -- gibt er ab, ist er wieder dran, und es
/// ist ein neuer Zug (RAD_GameEndTurn).
///

static const RAD_UserId_t ziehender = (RAD_UserId_t)0x4711;

/// Ein Weg aus zwei Feldern, von (x, y) einen Schritt nach rechts.
static RAD_Path_t ein_schritt_nach_rechts(int16_t x, int16_t y)
{
    return (RAD_Path_t){
        .steps_to = { { .x = x, .y = y }, { .x = (int16_t)(x + 1), .y = y } },
        .number_of_steps = 2
    };
}

static void ziehe(RAD_Game_t *game, RAD_UnitId_t figur, const RAD_Path_t *weg)
{
    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveUnit(game, figur, weg, &command));
    RAD_GameExecuteCommand(game, &command);
}

///
/// **Eine gerade aufgestellte Einheit zieht nicht** -- erst im naechsten Zug.
///
void test_move_kommando_frisch_aufgestellt_zieht_nicht(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, ziehender);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, ziehender));

    RAD_Unit_t values = { .number_of_members = 1, .movement = BEWEGUNG };
    RAD_UnitId_t figur = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, ziehender, &values, &figur));

    RAD_Command_t deploy = {0};
    TEST_ASSERT_TRUE(RAD_GameDeployUnit(game, figur, 3, 4, &deploy));
    RAD_GameExecuteCommand(game, &deploy);
    pruefe_figur_steht_auf(game, figur, 3, 4);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_UNIT_JUST_DEPLOYED, RAD_GameCheckMoveUnit(game, figur));
    TEST_ASSERT_FALSE(RAD_GameUnitCanMove(game, figur));

    const RAD_Path_t weg = ein_schritt_nach_rechts(3, 4);
    ziehe(game, figur, &weg);
    pruefe_figur_steht_auf(game, figur, 3, 4);

    // Im naechsten Zug darf sie.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, ziehender));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckMoveUnit(game, figur));

    ziehe(game, figur, &weg);
    pruefe_figur_steht_auf(game, figur, 4, 4);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// **Einmal je Zug:** der zweite Zug im selben Zug wird abgelehnt, nach dem
/// Zugwechsel geht es wieder.
///
void test_move_kommando_nur_einmal_je_zug(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, ziehender);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, ziehender));

    const RAD_UnitId_t figur = spawne(game, 2, 2);
    TEST_ASSERT_TRUE(RAD_GameUnitCanMove(game, figur));

    const RAD_Path_t erster = ein_schritt_nach_rechts(2, 2);
    ziehe(game, figur, &erster);
    pruefe_figur_steht_auf(game, figur, 3, 2);
    TEST_ASSERT_TRUE(RAD_WorldUnitById(&game->world, figur)->turn.moved);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_UNIT_ALREADY_MOVED, RAD_GameCheckMoveUnit(game, figur));

    const RAD_Path_t zweiter = ein_schritt_nach_rechts(3, 2);
    ziehe(game, figur, &zweiter);
    pruefe_figur_steht_auf(game, figur, 3, 2);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, ziehender));
    TEST_ASSERT_FALSE(RAD_WorldUnitById(&game->world, figur)->turn.moved);

    ziehe(game, figur, &zweiter);
    pruefe_figur_steht_auf(game, figur, 4, 2);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// **Kam sie nicht los, hat sie nicht gezogen:** ist schon das erste Feld
/// besetzt, darf sie es noch einmal versuchen.
///
void test_move_kommando_am_ersten_feld_blockiert_zaehlt_nicht(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, ziehender);

    const RAD_UnitId_t figur = spawne(game, 2, 2);
    spawne(game, 3, 2);

    const RAD_Path_t weg = ein_schritt_nach_rechts(2, 2);
    ziehe(game, figur, &weg);

    pruefe_figur_steht_auf(game, figur, 2, 2);
    TEST_ASSERT_FALSE(RAD_WorldUnitById(&game->world, figur)->turn.moved);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckMoveUnit(game, figur));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// **Kam sie ein Stueck weit, hat sie gezogen** -- auch wenn der Weg danach
/// besetzt war.
///
void test_move_kommando_teilweise_blockiert_zaehlt(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, ziehender);

    const RAD_UnitId_t figur = spawne(game, 2, 2);
    spawne(game, 4, 2);

    const RAD_Path_t weg = {
        .steps_to = { { .x = 2, .y = 2 }, { .x = 3, .y = 2 }, { .x = 4, .y = 2 } },
        .number_of_steps = 3
    };
    ziehe(game, figur, &weg);

    pruefe_figur_steht_auf(game, figur, 3, 2);
    TEST_ASSERT_TRUE(RAD_WorldUnitById(&game->world, figur)->turn.moved);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_UNIT_ALREADY_MOVED, RAD_GameCheckMoveUnit(game, figur));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// **Die Regel des Weges** (RAD_GameCheckMovePath): von Nachbar zu Nachbar, nur
/// ueber begehbares Gelaende, und nicht weiter, als die Bewegung reicht. Ein Weg,
/// der eine davon verletzt, wird als Ganzes abgelehnt -- die Figur bleibt stehen
/// und darf es noch einmal versuchen.
///
/// Das Gelaende kommt wie im Spiel als Tabelle (RAD_WorldSetTerrain): Wald kostet
/// zwei, Strasse wie Boden eins, tiefes Wasser ist unbegehbar.
///
static void setze_gelaende(RAD_Game_t *game)
{
    RAD_Terrain_t terrain[RAD_NUMBER_OF_TILE_TYPES] = { { 0 } };
    terrain[RAD_TILE_TYPE_FOREST] = (RAD_Terrain_t){ .movement_modifier = -1, .cover = 2 };
    terrain[RAD_TILE_TYPE_ROAD] = (RAD_Terrain_t){ .movement_modifier = 1 };
    terrain[RAD_TILE_TYPE_WATER_DEEP] = (RAD_Terrain_t){ .impassable = true };
    RAD_WorldSetTerrain(&game->world, terrain);
}

static RAD_Path_t weg_aus(const int16_t felder[][2], int32_t anzahl)
{
    RAD_Path_t weg = { .number_of_steps = anzahl };
    for(int32_t i = 0; i < anzahl; ++i)
    {
        weg.steps_to[i].x = felder[i][0];
        weg.steps_to[i].y = felder[i][1];
    }
    return weg;
}

void test_move_regel_kosten_des_gelaendes(void)
{
    const RAD_Tile_t wald = { .terrain = { .movement_modifier = -1 } };
    const RAD_Tile_t sumpf = { .terrain = { .movement_modifier = -2 } };
    const RAD_Tile_t strasse = { .terrain = { .movement_modifier = 1 } };
    const RAD_Tile_t boden = { .terrain = { .movement_modifier = 0 } };

    TEST_ASSERT_EQUAL_INT(2, RAD_TileMovementCost(&wald));
    TEST_ASSERT_EQUAL_INT(3, RAD_TileMovementCost(&sumpf));
    TEST_ASSERT_EQUAL_INT(1, RAD_TileMovementCost(&strasse));
    TEST_ASSERT_EQUAL_INT(1, RAD_TileMovementCost(&boden));
}

void test_move_regel_nennt_jeden_grund(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, ziehender);
    setze_gelaende(game);

    const RAD_UnitId_t figur = spawne(game, 2, 2);
    RAD_WorldUnitById(&game->world, figur)->movement = 3;

    TEST_ASSERT_TRUE(RAD_GameAddTile(game, 3, 2, 0, RAD_TILE_TYPE_FOREST));
    TEST_ASSERT_TRUE(RAD_GameAddTile(game, 2, 3, 0, RAD_TILE_TYPE_WATER_DEEP));
    TEST_ASSERT_TRUE(RAD_GameRemoveTile(game, 1, 2));

    // Wald (2) und Boden (1): genau die Bewegung.
    const int16_t passt[][2] = { { 2, 2 }, { 3, 2 }, { 4, 2 } };
    RAD_Path_t weg = weg_aus(passt, 3);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckMovePath(game, figur, &weg));

    // Ein Feld mehr ist eines zu viel.
    const int16_t zu_weit[][2] = { { 2, 2 }, { 3, 2 }, { 4, 2 }, { 5, 2 } };
    weg = weg_aus(zu_weit, 4);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_OUT_OF_MOVEMENT, RAD_GameCheckMovePath(game, figur, &weg));

    // Schraeg ist kein Nachbar, ein Sprung auch nicht -- und der erste Schritt
    // muss an die Figur grenzen, gleich was in steps_to[0] steht.
    const int16_t schraeg[][2] = { { 2, 2 }, { 3, 3 } };
    weg = weg_aus(schraeg, 2);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_PATH, RAD_GameCheckMovePath(game, figur, &weg));
    const int16_t sprung[][2] = { { 2, 2 }, { 4, 2 } };
    weg = weg_aus(sprung, 2);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_PATH, RAD_GameCheckMovePath(game, figur, &weg));
    const int16_t fremder_start[][2] = { { 5, 5 }, { 5, 6 } };
    weg = weg_aus(fremder_start, 2);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_PATH, RAD_GameCheckMovePath(game, figur, &weg));

    // Unbegehbar, ohne Gelaende, ausserhalb der Welt.
    const int16_t wasser[][2] = { { 2, 2 }, { 2, 3 } };
    weg = weg_aus(wasser, 2);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_IMPASSABLE, RAD_GameCheckMovePath(game, figur, &weg));
    const int16_t leere[][2] = { { 2, 2 }, { 1, 2 } };
    weg = weg_aus(leere, 2);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_GROUND, RAD_GameCheckMovePath(game, figur, &weg));

    // Kein Weg: nur der Standort.
    const int16_t standort[][2] = { { 2, 2 } };
    weg = weg_aus(standort, 1);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_PATH, RAD_GameCheckMovePath(game, figur, &weg));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_PATH, RAD_GameCheckMovePath(game, figur, NULL));

    // Erst die Einheit, dann der Weg.
    weg = weg_aus(passt, 3);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_UNIT, RAD_GameCheckMovePath(game, 99, &weg));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Die Strasse ist nicht umsonst: drei Felder Strasse kosten drei, nicht null.
///
void test_move_regel_strasse_kostet_eins(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, ziehender);
    setze_gelaende(game);

    const RAD_UnitId_t figur = spawne(game, 0, 0);
    RAD_WorldUnitById(&game->world, figur)->movement = 2;
    for(int32_t x = 1; x <= 3; ++x)
    {
        TEST_ASSERT_TRUE(RAD_GameAddTile(game, x, 0, 0, RAD_TILE_TYPE_ROAD));
    }

    const int16_t zwei[][2] = { { 0, 0 }, { 1, 0 }, { 2, 0 } };
    RAD_Path_t weg = weg_aus(zwei, 3);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckMovePath(game, figur, &weg));

    const int16_t drei[][2] = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 } };
    weg = weg_aus(drei, 4);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_OUT_OF_MOVEMENT, RAD_GameCheckMovePath(game, figur, &weg));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// **Abgelehnt wird als Ganzes:** ein Weg, der zu weit fuehrt, bewegt die Figur
/// keinen Schritt -- nicht bis dorthin, wo die Bewegung noch reichte. Sie hat
/// danach nicht gezogen und darf es mit einem kuerzeren Weg versuchen.
///
void test_move_kommando_zu_weit_bewegt_nicht(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, ziehender);
    setze_gelaende(game);

    const RAD_UnitId_t figur = spawne(game, 2, 2);
    RAD_WorldUnitById(&game->world, figur)->movement = 2;
    TEST_ASSERT_TRUE(RAD_GameAddTile(game, 4, 2, 0, RAD_TILE_TYPE_FOREST));

    RAD_ZugMitschrift_t mitschrift;
    abonniere_zug(events, &mitschrift);

    // Boden (1) und Wald (2): drei, eine Bewegung von zwei reicht nicht.
    const int16_t zu_weit[][2] = { { 2, 2 }, { 3, 2 }, { 4, 2 } };
    RAD_Path_t weg = weg_aus(zu_weit, 3);
    ziehe(game, figur, &weg);

    pruefe_figur_steht_auf(game, figur, 2, 2);
    TEST_ASSERT_FALSE(RAD_WorldUnitById(&game->world, figur)->turn.moved);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.moved);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.pfad.number_of_steps);
    TEST_ASSERT_NOT_EQUAL(0, mitschrift.ergebnis);

    const int16_t kuerzer[][2] = { { 2, 2 }, { 3, 2 } };
    weg = weg_aus(kuerzer, 2);
    ziehe(game, figur, &weg);
    pruefe_figur_steht_auf(game, figur, 3, 2);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Unbegehbar heisst auch: nicht darauf aufstellen.
///
void test_deploy_nicht_auf_unbegehbares_feld(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, ziehender);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, ziehender));
    setze_gelaende(game);

    RAD_Unit_t values = { .number_of_members = 1, .movement = BEWEGUNG };
    RAD_UnitId_t figur = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, ziehender, &values, &figur));

    TEST_ASSERT_TRUE(RAD_GameAddTile(game, 3, 4, 0, RAD_TILE_TYPE_WATER_DEEP));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_IMPASSABLE, RAD_GameCheckDeployUnit(game, ziehender, figur, 3, 4));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckDeployUnit(game, ziehender, figur, 2, 4));

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}
