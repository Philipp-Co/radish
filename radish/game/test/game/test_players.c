#include <unity.h>
#include <radish/game/game.h>
#include <radish/game/game_definitions.h>

// Der Test sieht den Spielzustand, um an den Spieler eines Benutzers zu kommen
// (RAD_GameFindPlayer). Die Begruendung fuer diesen Suchpfad steht in
// test/CMakeLists.txt.
#include <radish/game/model/game.h>
#include <radish/game/model/player/player.h>
#include <radish/game/model/reserve/reserve.h>
#include <radish/game/model/world/world.h>
#include <string.h>

///
/// Die Spieler im Spiel: angelegt beim Beitritt, zerstoert beim Austritt, und
/// immer dieselben Benutzer wie in der Reihe.
///

static RAD_EventManager_t *events;
static RAD_Game_t *game;

static void lege_spiel_an(void)
{
    events = RAD_CreateEventManager();
    game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
}

static void baue_spiel_ab(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

void test_players_beitritt_legt_einen_spieler_an(void)
{
    lege_spiel_an();
    TEST_ASSERT_NULL(RAD_GameFindPlayer(game, 0x10));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, 0x10));
    RAD_Player_t *player = RAD_GameFindPlayer(game, 0x10);
    TEST_ASSERT_NOT_NULL(player);
    TEST_ASSERT_EQUAL_UINT64(0x10, RAD_PlayerId(player));
    TEST_ASSERT_EQUAL_INT32(0, RAD_PlayerNumberOfUnits(player));
    TEST_ASSERT_EQUAL_INT32(0, RAD_ReserveNumberOfUnits(RAD_PlayerReserve(player)));

    // Zweimal derselbe: derselbe Spieler, kein zweiter.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, 0x10));
    TEST_ASSERT_TRUE(RAD_GameFindPlayer(game, 0x10) == player);
    TEST_ASSERT_EQUAL_INT32(1, game->number_of_players);
    TEST_ASSERT_EQUAL_INT32(1, RAD_GameNumberOfPlayers(game));

    baue_spiel_ab();
}

void test_players_ohne_benutzer_kein_spieler(void)
{
    lege_spiel_an();

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_USER, RAD_GameAddPlayer(game, RAD_USER_NONE));
    TEST_ASSERT_EQUAL_INT32(0, game->number_of_players);
    TEST_ASSERT_NULL(RAD_GameFindPlayer(game, RAD_USER_NONE));
    TEST_ASSERT_NULL(RAD_GameFindPlayer(NULL, 0x10));

    baue_spiel_ab();
}

void test_players_austritt_zerstoert_nur_seinen_spieler(void)
{
    lege_spiel_an();
    RAD_GameAddPlayer(game, 0x10);
    RAD_GameAddPlayer(game, 0x20);
    RAD_GameAddPlayer(game, 0x30);
    RAD_Player_t *third = RAD_GameFindPlayer(game, 0x30);

    RAD_GameRemovePlayer(game, 0x20);
    TEST_ASSERT_NULL(RAD_GameFindPlayer(game, 0x20));
    TEST_ASSERT_FALSE(RAD_GameIsPlaying(game, 0x20));
    TEST_ASSERT_EQUAL_INT32(2, game->number_of_players);
    TEST_ASSERT_NOT_NULL(RAD_GameFindPlayer(game, 0x10));
    TEST_ASSERT_TRUE(RAD_GameFindPlayer(game, 0x30) == third);

    // Wer nicht mitspielt, tritt ohne Folgen aus.
    RAD_GameRemovePlayer(game, 0x20);
    TEST_ASSERT_EQUAL_INT32(2, game->number_of_players);

    // Ein neuer Beitritt legt einen neuen Spieler an.
    RAD_GameAddPlayer(game, 0x20);
    TEST_ASSERT_NOT_NULL(RAD_GameFindPlayer(game, 0x20));
    TEST_ASSERT_EQUAL_INT32(3, game->number_of_players);

    baue_spiel_ab();
}

void test_players_volles_spiel_legt_keinen_an(void)
{
    lege_spiel_an();

    for(int32_t i = 0; i < RAD_MAX_PLAYERS; ++i)
    {
        TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, (RAD_UserId_t)(0x100 + i)));
    }
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_FULL, RAD_GameAddPlayer(game, 0x999));
    TEST_ASSERT_NULL(RAD_GameFindPlayer(game, 0x999));
    TEST_ASSERT_EQUAL_INT32(RAD_MAX_PLAYERS, game->number_of_players);
    TEST_ASSERT_EQUAL_INT32(RAD_MAX_PLAYERS, RAD_GameNumberOfPlayers(game));

    // Abbauen mit vollen Plaetzen gibt jeden Spieler wieder her.
    baue_spiel_ab();
    TEST_ASSERT_NULL(game);
}

///
/// Einheitenliste und Reserve folgen dem Spiel: was ein Spieler bekommt, steht in
/// beiden; was er aufstellt, verlaesst die Reserve und bleibt in der Liste.
///
static RAD_Unit_t trupp_werte(void)
{
    RAD_Unit_t values;
    memset(&values, 0, sizeof(values));
    strcpy(values.name, "Trupp");
    values.number_of_members = 1;
    strcpy(values.members[0].profile, "Soldat");
    return values;
}

void test_players_neue_einheit_steht_in_liste_und_reserve(void)
{
    lege_spiel_an();
    RAD_GameAddPlayer(game, 0x10);
    const RAD_Unit_t values = trupp_werte();

    RAD_UnitId_t id = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, 0x10, &values, &id));

    RAD_Player_t *player = RAD_GameFindPlayer(game, 0x10);
    const RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, id);
    TEST_ASSERT_TRUE(RAD_PlayerUnitAt(player, 0) == unit);
    TEST_ASSERT_TRUE(RAD_ReserveContains(RAD_PlayerReserve(player), unit));
    TEST_ASSERT_EQUAL_INT32(1, RAD_GameNumberOfUnits(game));

    baue_spiel_ab();
}

void test_players_aufstellen_verlaesst_die_reserve(void)
{
    lege_spiel_an();
    RAD_GameAddPlayer(game, 0x10);
    const RAD_Unit_t values = trupp_werte();
    RAD_UnitId_t id = RAD_UNIT_NONE;
    RAD_GameAddUnit(game, 0x10, &values, &id);

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameDeployUnit(game, id, 2, 3, &command));
    command.header.user = 0x10;
    RAD_GameExecuteCommand(game, &command);

    RAD_Player_t *player = RAD_GameFindPlayer(game, 0x10);
    const RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, id);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, unit->state);
    TEST_ASSERT_FALSE(RAD_ReserveContains(RAD_PlayerReserve(player), unit));
    TEST_ASSERT_TRUE(RAD_PlayerOwnsUnit(player, unit));
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    baue_spiel_ab();
}

void test_players_zuordnen_und_loesen(void)
{
    lege_spiel_an();
    RAD_GameAddPlayer(game, 0x10);
    const RAD_UnitId_t id = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_NPC, 1, 1);
    const RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, id);
    RAD_Player_t *player = RAD_GameFindPlayer(game, 0x10);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameBindUnit(game, 0x10, id));
    TEST_ASSERT_TRUE(RAD_PlayerOwnsUnit(player, unit));
    // Sie steht auf dem Feld, also in keiner Reserve.
    TEST_ASSERT_FALSE(RAD_ReserveContains(RAD_PlayerReserve(player), unit));

    RAD_GameUnbindUnit(game, id);
    TEST_ASSERT_FALSE(RAD_PlayerOwnsUnit(player, unit));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, unit->owner);

    baue_spiel_ab();
}

///
/// Seine Figuren bleiben seine (game.h): wer austritt und wiederkommt, findet sie
/// in seiner neuen Einheitenliste, und was noch nicht steht, in seiner Reserve.
///
void test_players_wiederkehr_findet_seine_einheiten(void)
{
    lege_spiel_an();
    RAD_GameAddPlayer(game, 0x10);
    const RAD_Unit_t values = trupp_werte();
    RAD_UnitId_t in_reserve = RAD_UNIT_NONE;
    RAD_UnitId_t auf_dem_feld = RAD_UNIT_NONE;
    RAD_GameAddUnit(game, 0x10, &values, &in_reserve);
    RAD_GameAddUnit(game, 0x10, &values, &auf_dem_feld);
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&game->world, auf_dem_feld, 0, 0));

    RAD_GameRemovePlayer(game, 0x10);
    TEST_ASSERT_NULL(RAD_GameFindPlayer(game, 0x10));

    RAD_GameAddPlayer(game, 0x10);
    RAD_Player_t *player = RAD_GameFindPlayer(game, 0x10);
    TEST_ASSERT_EQUAL_INT32(2, RAD_PlayerNumberOfUnits(player));
    TEST_ASSERT_EQUAL_INT32(1, RAD_ReserveNumberOfUnits(RAD_PlayerReserve(player)));
    TEST_ASSERT_TRUE(RAD_ReserveUnitAt(RAD_PlayerReserve(player), 0) == RAD_WorldUnitById(&game->world, in_reserve));

    baue_spiel_ab();
}

///
/// Das Kommando zum Abgeben (RAD_COMMAND_TYPE_END_TURN) gibt den Zug an den
/// naechsten in der Reihe weiter -- aber nur, wenn es der schickt, der dran ist.
///
void test_players_end_turn_kommando_gibt_den_zug_weiter(void)
{
    lege_spiel_an();
    RAD_GameAddPlayer(game, 0x10);
    RAD_GameAddPlayer(game, 0x20);
    TEST_ASSERT_EQUAL_UINT64(0x10, RAD_GameCurrentUser(game));

    RAD_Command_t command;
    memset(&command, 0, sizeof(command));
    command.header.type = RAD_COMMAND_TYPE_END_TURN;

    // Wer nicht dran ist, gibt nichts ab.
    command.header.user = 0x20;
    RAD_GameExecuteCommand(game, &command);
    TEST_ASSERT_EQUAL_UINT64(0x10, RAD_GameCurrentUser(game));

    command.header.user = 0x10;
    RAD_GameExecuteCommand(game, &command);
    TEST_ASSERT_EQUAL_UINT64(0x20, RAD_GameCurrentUser(game));

    baue_spiel_ab();
}
