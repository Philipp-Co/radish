#include <unity.h>
#include <string.h>
#include <radish/game/game.h>
#include <radish/game/game_definitions.h>

// Der Test sieht den Spielzustand: er setzt Siegfelder und stellt Einheiten
// direkt in die Welt. Die Begruendung fuer diesen Suchpfad steht in
// test/CMakeLists.txt.
#include <radish/game/model/game.h>
#include <radish/game/model/world/world.h>

///
/// Die Siegbedingungen (control/victory.c): Punkte auf den Siegfeldern, die Wertung
/// nach der letzten Runde und die Niederlage dessen, der keine Einheit mehr hat.
///

static const RAD_UserId_t host = (RAD_UserId_t)0x4711;
static const RAD_UserId_t gast = (RAD_UserId_t)0x0815;

static RAD_EventManager_t *events;
static RAD_Game_t *game;

typedef struct
{
    int32_t gemeldet;
    RAD_UserId_t sieger;
    RAD_GameEndReason_t grund;
} RAD_EndeMitschrift_t;

static RAD_EndeMitschrift_t ende;

static void notiere_ende(void *user_argument, RAD_UserId_t winner, RAD_GameEndReason_t reason)
{
    RAD_EndeMitschrift_t *m = user_argument;
    m->gemeldet++;
    m->sieger = winner;
    m->grund = reason;
}

///
/// Zwei Spieler, ein Siegfeld auf (3, 3). Abonniert wird das Ende.
///
static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, host));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, gast));
    game->world.tiles[3][3].victory = true;

    memset(&ende, 0, sizeof(ende));
    RAD_EventManagerSubscribeToGameEvents(events, (RAD_EventsGameCallback_t){
        .user_argument = &ende,
        .finished = notiere_ende
    });
}

static void abbauen(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Eine Einheit von "owner" in die Reserve, und steht (x, y) in der Welt, gleich
/// dorthin -- an der Aufstellregel vorbei, um die geht es hier nicht.
///
static RAD_UnitId_t einheit(RAD_UserId_t owner, bool can_capture, int32_t x, int32_t y)
{
    RAD_Unit_t values;
    memset(&values, 0, sizeof(values));
    strcpy(values.name, "Trupp");
    values.number_of_members = 1;
    strcpy(values.members[0].profile, "Soldat");
    values.can_capture = can_capture;

    RAD_UnitId_t id = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, owner, &values, &id));
    if(x >= 0)
    {
        TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&game->world, id, x, y));
    }
    return id;
}

/// Beide geben einmal ab: eine Runde.
static void runde(void)
{
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, host));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, gast));
}

void test_victory_auf_dem_feld_zaehlt_zwanzig(void)
{
    aufbauen();
    TEST_ASSERT_EQUAL_INT32(0, RAD_GameScore(game, host));

    einheit(host, true, 3, 3);
    TEST_ASSERT_EQUAL_INT32(RAD_VICTORY_TILE_POINTS, RAD_GameScore(game, host));
    TEST_ASSERT_EQUAL_INT32(0, RAD_GameScore(game, gast));

    TEST_ASSERT_EQUAL_INT32(0, RAD_GameScore(game, RAD_USER_NONE));
    TEST_ASSERT_EQUAL_INT32(0, RAD_GameScore(NULL, host));

    abbauen();
}

void test_victory_ohne_can_capture_zaehlt_nichts(void)
{
    aufbauen();

    einheit(host, false, 3, 3);
    einheit(host, false, 2, 3);
    TEST_ASSERT_EQUAL_INT32(0, RAD_GameScore(game, host));

    abbauen();
}

///
/// Die vier Nachbarn zaehlen je fuer sich, diagonal und weiter weg nicht.
///
void test_victory_nachbarn_zaehlen_je_fuenf(void)
{
    aufbauen();

    einheit(host, true, 2, 3);
    einheit(host, true, 4, 3);
    einheit(gast, true, 3, 2);
    einheit(gast, true, 3, 4);
    einheit(gast, true, 2, 2);  // diagonal
    einheit(host, true, 3, 5);  // zwei weit
    einheit(host, true, 5, 3);  // zwei weit

    TEST_ASSERT_EQUAL_INT32(2 * RAD_VICTORY_NEIGHBOUR_POINTS, RAD_GameScore(game, host));
    TEST_ASSERT_EQUAL_INT32(2 * RAD_VICTORY_NEIGHBOUR_POINTS, RAD_GameScore(game, gast));

    abbauen();
}

///
/// Mehrere Siegfelder addieren sich, und eine Einheit neben zweien zaehlt bei
/// beiden. Was in der Reserve steht, zaehlt nicht.
///
void test_victory_mehrere_felder_und_reserve(void)
{
    aufbauen();
    game->world.tiles[3][5].victory = true;

    einheit(host, true, 3, 3);   // auf (3, 3)
    einheit(host, true, 4, 3);   // neben (3, 3) und (5, 3)
    einheit(host, true, -1, -1); // in der Reserve

    TEST_ASSERT_EQUAL_INT32(RAD_VICTORY_TILE_POINTS + 2 * RAD_VICTORY_NEIGHBOUR_POINTS,
                            RAD_GameScore(game, host));

    abbauen();
}

///
/// Die Runden: weitergezaehlt, wenn der letzte in der Reihe abgibt. Nach der
/// letzten ist Schluss -- mit mehr Punkten gewinnt man.
///
void test_victory_nach_der_letzten_runde_gewinnt_wer_mehr_hat(void)
{
    aufbauen();
    einheit(host, true, 3, 3);
    einheit(gast, true, 3, 4);
    TEST_ASSERT_EQUAL_INT32(1, RAD_GameRound(game));
    TEST_ASSERT_EQUAL_INT32(RAD_GAME_NUMBER_OF_ROUNDS, RAD_GameNumberOfRounds(game));

    // Abgeben des ersten ist noch keine Runde.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, host));
    TEST_ASSERT_EQUAL_INT32(1, RAD_GameRound(game));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, gast));
    TEST_ASSERT_EQUAL_INT32(2, RAD_GameRound(game));

    for(int32_t r = 2; r < RAD_GAME_NUMBER_OF_ROUNDS; ++r)
    {
        runde();
    }
    TEST_ASSERT_EQUAL_INT32(RAD_GAME_NUMBER_OF_ROUNDS, RAD_GameRound(game));
    TEST_ASSERT_FALSE(RAD_GameIsFinished(game));
    TEST_ASSERT_EQUAL_INT(0, ende.gemeldet);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, host));
    TEST_ASSERT_FALSE(RAD_GameIsFinished(game));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, gast));

    TEST_ASSERT_TRUE(RAD_GameIsFinished(game));
    TEST_ASSERT_EQUAL_UINT64(host, RAD_GameWinner(game));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_END_ROUNDS, RAD_GameEndReason(game));
    TEST_ASSERT_EQUAL_INT32(RAD_GAME_NUMBER_OF_ROUNDS, RAD_GameRound(game));
    TEST_ASSERT_EQUAL_INT(1, ende.gemeldet);
    TEST_ASSERT_EQUAL_UINT64(host, ende.sieger);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_END_ROUNDS, ende.grund);

    abbauen();
}

void test_victory_gleichstand_ist_unentschieden(void)
{
    aufbauen();
    einheit(host, true, 2, 3);
    einheit(gast, true, 4, 3);

    for(int32_t r = 0; r < RAD_GAME_NUMBER_OF_ROUNDS; ++r)
    {
        runde();
    }

    TEST_ASSERT_TRUE(RAD_GameIsFinished(game));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_GameWinner(game));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_END_ROUNDS, RAD_GameEndReason(game));
    TEST_ASSERT_EQUAL_INT(1, ende.gemeldet);
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, ende.sieger);

    abbauen();
}

///
/// Nach dem Ende nimmt das Spiel nichts mehr an: die Regeln sagen es, und ein
/// Kommando aendert nichts.
///
void test_victory_nach_dem_ende_wird_alles_abgelehnt(void)
{
    aufbauen();
    const RAD_UnitId_t stehend = einheit(host, true, 1, 1);
    const RAD_UnitId_t reserve = einheit(host, true, -1, -1);
    for(int32_t r = 0; r < RAD_GAME_NUMBER_OF_ROUNDS; ++r)
    {
        runde();
    }
    TEST_ASSERT_TRUE(RAD_GameIsFinished(game));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_FINISHED, RAD_GameEndTurn(game, host));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_FINISHED, RAD_GameCheckDeployUnit(game, host, reserve, 0, 0));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_FINISHED, RAD_GameCheckMoveUnit(game, stehend));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_FINISHED, RAD_GameCheckAttack(game, stehend, 2, 2));

    RAD_Command_t command;
    memset(&command, 0, sizeof(command));
    command.header.type = RAD_COMMAND_TYPE_DEPLOY_UNIT;
    command.header.user = host;
    command.command.deploy_unit.unit = reserve;
    command.command.deploy_unit.x = 0;
    command.command.deploy_unit.y = 0;
    RAD_GameExecuteCommand(game, &command);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, RAD_WorldUnitById(&game->world, reserve)->state);

    TEST_ASSERT_EQUAL_INT(1, ende.gemeldet);

    abbauen();
}

///
/// Wer keine Einheit mehr hat, verliert -- erst, wenn auch die Reserve leer ist.
///
void test_victory_ohne_einheiten_verliert_man(void)
{
    aufbauen();
    const RAD_UnitId_t erste = einheit(gast, false, 6, 6);
    const RAD_UnitId_t zweite = einheit(gast, false, -1, -1);
    einheit(host, false, 1, 1);

    RAD_GameDestroyUnit(game, erste);
    TEST_ASSERT_FALSE(RAD_GameIsFinished(game));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DESTROYED, RAD_WorldUnitById(&game->world, erste)->state);

    // Die Reserve zaehlt: erst aufstellen, dann zerstoeren.
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&game->world, zweite, 6, 5));
    RAD_GameDestroyUnit(game, zweite);

    TEST_ASSERT_TRUE(RAD_GameIsFinished(game));
    TEST_ASSERT_EQUAL_UINT64(host, RAD_GameWinner(game));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_END_ELIMINATED, RAD_GameEndReason(game));
    TEST_ASSERT_EQUAL_INT(1, ende.gemeldet);
    TEST_ASSERT_EQUAL_UINT64(host, ende.sieger);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_END_ELIMINATED, ende.grund);

    // Zweimal zerstoert, oder eine unbekannte: nichts weiter.
    RAD_GameDestroyUnit(game, zweite);
    RAD_GameDestroyUnit(game, RAD_UNIT_NONE);
    TEST_ASSERT_EQUAL_INT(1, ende.gemeldet);

    abbauen();
}

///
/// Wer noch nie eine Armee hatte, verliert nicht: nur das Zerstoeren loest die
/// Pruefung aus, und nur fuer den Besitzer.
///
void test_victory_ohne_armee_verliert_man_nicht(void)
{
    aufbauen();
    const RAD_UnitId_t a = einheit(host, false, 1, 1);
    einheit(host, false, 1, 2);

    RAD_GameDestroyUnit(game, a);
    TEST_ASSERT_FALSE(RAD_GameIsFinished(game));
    TEST_ASSERT_EQUAL_INT(0, ende.gemeldet);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_END_NONE, RAD_GameEndReason(game));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_GameWinner(game));

    abbauen();
}
