#include <unity.h>
#include <string.h>

#include <radish/game/game.h>
#include <radish/game/model/unit/unit.h>
#include <radish/game/model/tile/tile.h>
#include <radish/game/control/start_game.h>
#include <radish/server/control/execute.h>
#include <radish/server/control/loader.h>

///
/// Das Zuruecksetzen des Spiels (RAD_ControlResetGame): vor jedem neuen Start
/// baut der Server Steuerung und Spiel ab und neu auf. Ohne das nimmt ein Server
/// nach seinem ersten Spiel keines mehr an (RAD_GAME_ERROR_STARTED).
///

static RAD_ControlGame_t loaded;
static RAD_Control_t control;
static RAD_EventCallbacks_t callbacks;

/// Wie oft das Spiel eine aufgestellte Einheit gemeldet hat.
static int32_t aufgestellt;

static void zaehle_aufstellen(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument; (void)unit; (void)x; (void)y;
    aufgestellt++;
}

static void einheit_ohne_belang(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument; (void)unit; (void)x; (void)y;
}

static void zug_ohne_belang(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)user_argument; (void)unit; (void)path; (void)result;
}

static void angriff_ohne_belang(void *user_argument, const RAD_AttackResult_t *result)
{
    (void)user_argument; (void)result;
}

static void zugwechsel_ohne_belang(void *user_argument, RAD_UserId_t current_user)
{
    (void)user_argument; (void)current_user;
}

static void feld_ohne_belang(void *user_argument, const RAD_Tile_t *tile)
{
    (void)user_argument; (void)tile;
}

static void ende_ohne_belang(void *user_argument, RAD_UserId_t winner, RAD_GameEndReason_t reason)
{
    (void)user_argument; (void)winner; (void)reason;
}

static void aufbauen(void)
{
    callbacks = (RAD_EventCallbacks_t){
        .tile_changed = {
            .added = feld_ohne_belang,
            .removed = feld_ohne_belang,
            .changed = feld_ohne_belang
        },
        .unit_changed = {
            .spawned = zaehle_aufstellen,
            .destroyed = einheit_ohne_belang,
            .moved = zug_ohne_belang,
            .attacked = angriff_ohne_belang
        },
        .turn_changed = { .changed = zugwechsel_ohne_belang },
        .game_changed = { .finished = ende_ohne_belang }
    };
    loaded = RAD_ControlCreateGame(NULL, &callbacks);
    TEST_ASSERT_NOT_NULL(loaded.game);
    control = RAD_CreateControl(loaded.game);
    TEST_ASSERT_NOT_NULL(control);
    aufgestellt = 0;
}

static void abbauen(void)
{
    RAD_DestroyControl(&control);
    RAD_ControlDestroyGame(&loaded);
}

/// Zwei Spieler mit je einer Einheit, ueber die Steuerung wie im Server.
static RAD_GameResult_t starte(const char *host, const char *gast)
{
    RAD_GameStart_t *start = RAD_CreateGameStart();
    TEST_ASSERT_NOT_NULL(start);
    strcpy(start->players[0].identifier, host);
    strcpy(start->players[1].identifier, gast);
    for(int32_t p=0;p < RAD_GAME_START_NUMBER_OF_PLAYERS; ++p)
    {
        RAD_Unit_t *unit = &start->players[p].units[0];
        strcpy(unit->name, "Trupp");
        unit->number_of_members = 1;
        strcpy(unit->members[0].profile, "Soldat");
        unit->members[0].health = 1;
        start->players[p].number_of_units = 1;
    }
    const RAD_GameResult_t result = RAD_ControlStartGame(control, start);
    RAD_DestroyGameStart(&start);
    return result;
}

/// Spielt alle Runden durch -- danach ist das Spiel nach Punkten vorbei.
static void spiele_zu_ende(RAD_UserId_t host, RAD_UserId_t gast)
{
    for(int32_t r = 0; r < RAD_ControlNumberOfRounds(control); ++r)
    {
        TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(loaded.game, host));
        TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(loaded.game, gast));
    }
    TEST_ASSERT_TRUE(RAD_ControlIsFinished(control));
}

///
/// Genau das, was ohne Zuruecksetzen scheitert: nach einem Spiel ein zweites
/// auf derselben Steuerung. Erst ohne -- abgelehnt --, dann mit.
///
void test_nach_dem_zuruecksetzen_geht_ein_zweiter_start(void)
{
    aufbauen();
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, starte("aB3xK9pQ", "Zz1Yy2Xx"));
    spiele_zu_ende(RAD_UserIdFromIdentifier("aB3xK9pQ"), RAD_UserIdFromIdentifier("Zz1Yy2Xx"));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_STARTED, starte("Cc3Dd4Ee", "Ff5Gg6Hh"));

    TEST_ASSERT_TRUE(RAD_ControlResetGame(&loaded, &control, NULL, &callbacks));

    // Leer wie nach dem Prozessstart.
    TEST_ASSERT_NOT_NULL(loaded.game);
    TEST_ASSERT_NOT_NULL(control);
    TEST_ASSERT_EQUAL_INT32(0, RAD_ControlNumberOfPlayers(control));
    TEST_ASSERT_EQUAL_INT32(0, RAD_ControlNumberOfUnits(control));
    TEST_ASSERT_FALSE(RAD_ControlIsFinished(control));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_ControlWinner(control));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, starte("Cc3Dd4Ee", "Ff5Gg6Hh"));
    TEST_ASSERT_EQUAL_INT32(2, RAD_ControlNumberOfPlayers(control));
    TEST_ASSERT_EQUAL_UINT64(RAD_UserIdFromIdentifier("Cc3Dd4Ee"), RAD_ControlPlayerAt(control, 0));
    TEST_ASSERT_EQUAL_INT32(2, RAD_ControlNumberOfUnits(control));

    abbauen();
}

///
/// Die Callbacks bleiben: was das neue Spiel meldet, kommt beim selben
/// Abonnenten an wie vorher.
///
void test_nach_dem_zuruecksetzen_kommen_die_ereignisse_an(void)
{
    aufbauen();
    TEST_ASSERT_TRUE(RAD_ControlResetGame(&loaded, &control, NULL, &callbacks));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, starte("aB3xK9pQ", "Zz1Yy2Xx"));

    const RAD_UserId_t host = RAD_UserIdFromIdentifier("aB3xK9pQ");
    RAD_Command_t command;
    memset(&command, 0, sizeof(command));
    command.header.type = RAD_COMMAND_TYPE_DEPLOY_UNIT;
    command.header.sequence = 1;
    command.header.user = host;
    command.command.deploy_unit.unit = RAD_GameUserUnitAt(loaded.game, host, 0);
    command.command.deploy_unit.x = 2;
    command.command.deploy_unit.y = 3;
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, RAD_ControlExecuteCommand(control, &command).value);

    TEST_ASSERT_EQUAL_INT32(1, aufgestellt);

    abbauen();
}

///
/// Ohne Neustart bleibt ein beendetes Spiel beendet -- mit Grund und Punkten,
/// bis das naechste kommt.
///
void test_ein_beendetes_spiel_bleibt_ohne_neustart_stehen(void)
{
    aufbauen();
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, starte("aB3xK9pQ", "Zz1Yy2Xx"));
    spiele_zu_ende(RAD_UserIdFromIdentifier("aB3xK9pQ"), RAD_UserIdFromIdentifier("Zz1Yy2Xx"));

    TEST_ASSERT_TRUE(RAD_ControlIsFinished(control));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_END_ROUNDS, RAD_ControlEndReason(control));
    TEST_ASSERT_EQUAL_INT32(2, RAD_ControlNumberOfPlayers(control));
    TEST_ASSERT_EQUAL_INT32(2, RAD_ControlNumberOfUnits(control));

    abbauen();
}

///
/// Scheitert das Neuanlegen -- hier an einer Welt, die es nicht gibt --, ist
/// nichts mehr da, und beides ist NULL statt halb.
///
void test_scheitert_das_neuanlegen_ist_nichts_mehr_da(void)
{
    aufbauen();

    TEST_ASSERT_FALSE(RAD_ControlResetGame(&loaded, &control, "/gibt/es/nicht.json", &callbacks));
    TEST_ASSERT_NULL(loaded.game);
    TEST_ASSERT_NULL(loaded.event_manager);
    TEST_ASSERT_NULL(control);

    abbauen();
}
