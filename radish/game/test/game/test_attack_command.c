#include <unity.h>
#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand: er liest an der
// Einheit nach, was der Angriff hinterlassen hat. Die Begruendung fuer diesen
// Suchpfad steht in test/CMakeLists.txt.
#include <radish/game/model/game.h>
#include <radish/game/model/world/world.h>

///
/// Der Angriff auf ein Feld (RAD_COMMAND_TYPE_ATTACK): die Fabrik, die Regel
/// (RAD_GameCheckAttack) und was er an der Einheit hinterlaesst
/// (RAD_Unit_t.turn). Einen Kampf gibt es noch nicht -- gezaehlt wird nur, dass
/// sie angegriffen hat.
///
/// Ein Spieler genuegt: gibt er ab, ist er wieder dran, und es ist ein neuer Zug
/// (RAD_GameEndTurn).
///

static const RAD_UserId_t angreifer = (RAD_UserId_t)0x4711;

static RAD_EventManager_t *events;
static RAD_Game_t *game;

static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    game = RAD_CreateGame(events, angreifer);
    TEST_ASSERT_NOT_NULL(game);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, angreifer));
}

static void abbauen(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

static void greife_an(RAD_UnitId_t figur, int16_t x, int16_t y)
{
    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameAttack(game, figur, x, y, &command));
    RAD_GameExecuteCommand(game, &command);
}

static bool hat_angegriffen(RAD_UnitId_t figur)
{
    return RAD_WorldUnitById(&game->world, figur)->turn.attacked;
}

/// Gibt der Einheit ein Mitglied mit einer Waffe von "min_range" bis "max_range".
static void bewaffne(RAD_UnitId_t figur, int16_t min_range, int16_t max_range)
{
    RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, figur);
    TEST_ASSERT_NOT_NULL(unit);
    unit->number_of_members = 1;
    unit->members[0].number_of_weapons = 1;
    unit->members[0].weapons[0].min_range = min_range;
    unit->members[0].weapons[0].max_range = max_range;
}

/// Eine Einheit auf (x, y), deren Waffe "max_range" Felder weit reicht.
static RAD_UnitId_t bewaffnete_figur(int16_t x, int16_t y, int16_t max_range)
{
    const RAD_UnitId_t figur = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, x, y);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, figur);
    bewaffne(figur, 0, max_range);
    return figur;
}

///
/// Die Fabrik fuellt Kopf und Ziel; eine Id unter 0 weist sie ab, ohne "output"
/// anzufassen und ohne eine Nummer zu verbrauchen.
///
void test_attack_fabrik_fuellt_das_kommando(void)
{
    aufbauen();

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameAttack(game, 3, 5, 6, &command));
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_ATTACK, command.header.type);
    TEST_ASSERT_EQUAL_UINT64(angreifer, command.header.user);
    TEST_ASSERT_EQUAL_UINT64(1, command.header.sequence);
    TEST_ASSERT_EQUAL_INT(3, command.command.attack.unit);
    TEST_ASSERT_EQUAL_INT(5, command.command.attack.x);
    TEST_ASSERT_EQUAL_INT(6, command.command.attack.y);

    TEST_ASSERT_FALSE(RAD_GameAttack(game, RAD_UNIT_NONE, 1, 1, &command));
    TEST_ASSERT_EQUAL_INT(3, command.command.attack.unit);
    TEST_ASSERT_FALSE(RAD_GameAttack(NULL, 3, 1, 1, &command));
    TEST_ASSERT_FALSE(RAD_GameAttack(game, 3, 1, 1, NULL));

    TEST_ASSERT_TRUE(RAD_GameAttack(game, 3, 5, 6, &command));
    TEST_ASSERT_EQUAL_UINT64(2, command.header.sequence);

    abbauen();
}

///
/// **Eine gerade aufgestellte Einheit greift nicht an** -- erst im naechsten Zug.
///
void test_attack_frisch_aufgestellt_greift_nicht_an(void)
{
    aufbauen();

    RAD_Unit_t values = { .number_of_members = 1 };
    RAD_UnitId_t figur = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, angreifer, &values, &figur));

    bewaffne(figur, 0, 3);

    RAD_Command_t deploy = {0};
    TEST_ASSERT_TRUE(RAD_GameDeployUnit(game, figur, 3, 4, &deploy));
    RAD_GameExecuteCommand(game, &deploy);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_UNIT_JUST_DEPLOYED, RAD_GameCheckAttack(game, figur, 5, 4));
    TEST_ASSERT_FALSE(RAD_GameUnitCanAttack(game, figur));

    greife_an(figur, 5, 4);
    TEST_ASSERT_FALSE(hat_angegriffen(figur));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, angreifer));
    TEST_ASSERT_TRUE(RAD_GameUnitCanAttack(game, figur));

    greife_an(figur, 5, 4);
    TEST_ASSERT_TRUE(hat_angegriffen(figur));

    abbauen();
}

///
/// **Einmal je Zug:** der zweite Angriff im selben Zug wird abgelehnt, nach dem
/// Zugwechsel geht es wieder. Ein leeres Feld ist ein gueltiges Ziel.
///
void test_attack_nur_einmal_je_zug(void)
{
    aufbauen();

    const RAD_UnitId_t figur = bewaffnete_figur(2, 2, 3);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckAttack(game, figur, 4, 2));

    greife_an(figur, 4, 2);
    TEST_ASSERT_TRUE(hat_angegriffen(figur));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_UNIT_ALREADY_ATTACKED, RAD_GameCheckAttack(game, figur, 4, 2));
    TEST_ASSERT_FALSE(RAD_GameUnitCanAttack(game, figur));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, angreifer));
    TEST_ASSERT_FALSE(hat_angegriffen(figur));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckAttack(game, figur, 4, 2));

    abbauen();
}

///
/// **Ziehen und Angreifen zaehlen getrennt**, in beiden Reihenfolgen.
///
void test_attack_und_move_sind_unabhaengig(void)
{
    aufbauen();

    const RAD_UnitId_t erst_zug = bewaffnete_figur(1, 1, 3);
    const RAD_UnitId_t erst_angriff = bewaffnete_figur(1, 5, 3);

    // Erst ziehen, dann angreifen.
    const RAD_Path_t weg = {
        .steps_to = { { .x = 1, .y = 1 }, { .x = 2, .y = 1 } },
        .number_of_steps = 2
    };
    RAD_Command_t move = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveUnit(game, erst_zug, &weg, &move));
    RAD_GameExecuteCommand(game, &move);
    TEST_ASSERT_TRUE(RAD_WorldUnitById(&game->world, erst_zug)->turn.moved);
    TEST_ASSERT_TRUE(RAD_GameUnitCanAttack(game, erst_zug));

    greife_an(erst_zug, 4, 1);
    TEST_ASSERT_TRUE(hat_angegriffen(erst_zug));

    // Erst angreifen, dann ziehen.
    greife_an(erst_angriff, 4, 5);
    TEST_ASSERT_TRUE(hat_angegriffen(erst_angriff));
    TEST_ASSERT_TRUE(RAD_GameUnitCanMove(game, erst_angriff));

    const RAD_Path_t danach = {
        .steps_to = { { .x = 1, .y = 5 }, { .x = 2, .y = 5 } },
        .number_of_steps = 2
    };
    TEST_ASSERT_TRUE(RAD_GameMoveUnit(game, erst_angriff, &danach, &move));
    RAD_GameExecuteCommand(game, &move);
    TEST_ASSERT_EQUAL_INT(2, RAD_WorldUnitById(&game->world, erst_angriff)->x);
    TEST_ASSERT_TRUE(RAD_WorldUnitById(&game->world, erst_angriff)->turn.moved);

    abbauen();
}

///
/// **Ein Ziel ausserhalb der Welt ist keins:** abgelehnt, und es zaehlt nicht.
/// Ebenso wenig greift an, was nicht auf dem Feld steht.
///
void test_attack_ohne_gueltiges_ziel_oder_figur_zaehlt_nicht(void)
{
    aufbauen();

    const RAD_UnitId_t figur = bewaffnete_figur(2, 2, 20);
    const int16_t draussen = (int16_t)RAD_WORLD_WIDTH;

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_OUT_OF_BOUNDS, RAD_GameCheckAttack(game, figur, draussen, 2));
    greife_an(figur, draussen, 2);
    TEST_ASSERT_FALSE(hat_angegriffen(figur));

    RAD_Unit_t values = { .number_of_members = 1 };
    RAD_UnitId_t reserve = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, angreifer, &values, &reserve));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_UNIT_NOT_DEPLOYED, RAD_GameCheckAttack(game, reserve, 2, 3));
    greife_an(reserve, 2, 3);
    TEST_ASSERT_FALSE(hat_angegriffen(reserve));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_UNIT, RAD_GameCheckAttack(game, 99, 2, 3));

    abbauen();
}

///
/// **Die Reichweite:** wenigstens eine Waffe muss die Entfernung abdecken, in
/// Feldern waagerecht plus senkrecht, von min_range bis max_range. Das eigene
/// Feld ist nie ein Ziel. Was nicht reicht, zaehlt nicht als Angriff.
///
void test_attack_prueft_die_reichweite(void)
{
    aufbauen();

    // Eine Waffe von 2 bis 3 Feldern.
    const RAD_UnitId_t figur = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, 2, 2);
    bewaffne(figur, 2, 3);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_TARGET_OUT_OF_RANGE, RAD_GameCheckAttack(game, figur, 2, 2));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_TARGET_OUT_OF_RANGE, RAD_GameCheckAttack(game, figur, 3, 2));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckAttack(game, figur, 4, 2));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckAttack(game, figur, 3, 4));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_TARGET_OUT_OF_RANGE, RAD_GameCheckAttack(game, figur, 4, 4));

    greife_an(figur, 4, 4);
    TEST_ASSERT_FALSE(hat_angegriffen(figur));

    // Eine zweite Waffe ohne Mindestweite deckt das Nachbarfeld ab.
    RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, figur);
    unit->members[0].number_of_weapons = 2;
    unit->members[0].weapons[1].min_range = 0;
    unit->members[0].weapons[1].max_range = 1;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameCheckAttack(game, figur, 3, 2));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_TARGET_OUT_OF_RANGE, RAD_GameCheckAttack(game, figur, 2, 2));

    // Ohne Waffen reicht nichts.
    const RAD_UnitId_t unbewaffnet = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, 6, 6);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_TARGET_OUT_OF_RANGE, RAD_GameCheckAttack(game, unbewaffnet, 6, 5));

    abbauen();
}
