#include <unity.h>
#include <string.h>

#include <radish/game/game.h>
#include <radish/game/control/start_game.h>

// Der Test sieht den Spielzustand: was der Start eingerichtet hat, steht in Pool,
// Spielern und Reserven, und die sind von aussen nicht zu sehen
// (test/CMakeLists.txt).
#include <radish/game/model/game.h>
#include <radish/game/model/unit_pool/unit_pool.h>
#include <radish/game/model/player/player.h>
#include <radish/game/model/reserve/reserve.h>

///
/// Wie aus einem Spielstart ein Spiel wird (RAD_StartGame, RAD_StartGameFromFile):
/// Spieler treten bei, ihre Einheiten entstehen im Pool und stehen in
/// Einheitenliste und Reserve -- genau einmal, und bei einer Ablehnung aendert
/// sich nichts.
///
/// Was der Leser kann, prueft test_parse.c; hier geht es um das, was danach im
/// Spiel steht.
///

#define HOST "aB3xK9pQ"
#define GAST "Zz1Yy2Xx"

static RAD_EventManager_t *events;
static RAD_Game_t *game;
static RAD_GameStart_t *start;

static void einheit(RAD_GameStartPlayer_t *player, const char *name)
{
    RAD_Unit_t *unit = &player->units[player->number_of_units];
    strcpy(unit->name, name);
    unit->movement = 6;
    unit->number_of_members = 1;
    strcpy(unit->members[0].profile, "Soldat");
    player->number_of_units++;
}

static void lege_an(void)
{
    events = RAD_CreateEventManager();
    game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);

    start = RAD_CreateGameStart();
    TEST_ASSERT_NOT_NULL(start);
    strcpy(start->players[0].identifier, HOST);
    strcpy(start->players[1].identifier, GAST);
    einheit(&start->players[0], "Trupp");
    einheit(&start->players[0], "Panzer");
    einheit(&start->players[1], "Spaeher");
}

static void baue_ab(void)
{
    RAD_DestroyGameStart(&start);
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

static void pruefe_unberuehrt(void)
{
    TEST_ASSERT_EQUAL_INT32(0, game->number_of_players);
    TEST_ASSERT_EQUAL_INT32(0, RAD_GameNumberOfPlayers(game));
    TEST_ASSERT_EQUAL_INT32(0, RAD_UnitPoolNumberOfUnits(game->unit_pool));
}

void test_start_richtet_spieler_pool_und_reserven_ein(void)
{
    lege_an();
    const RAD_UserId_t host = RAD_UserIdFromIdentifier(HOST);
    const RAD_UserId_t gast = RAD_UserIdFromIdentifier(GAST);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_StartGame(game, start));

    // Beide spielen mit, der Host zuerst -- er ist dran.
    TEST_ASSERT_EQUAL_INT32(2, RAD_GameNumberOfPlayers(game));
    TEST_ASSERT_EQUAL_UINT64(host, RAD_GamePlayerAt(game, 0));
    TEST_ASSERT_EQUAL_UINT64(host, RAD_GameCurrentUser(game));
    TEST_ASSERT_EQUAL_INT32(3, RAD_UnitPoolNumberOfUnits(game->unit_pool));

    RAD_Player_t *erster = RAD_GameFindPlayer(game, host);
    RAD_Player_t *zweiter = RAD_GameFindPlayer(game, gast);
    TEST_ASSERT_NOT_NULL(erster);
    TEST_ASSERT_NOT_NULL(zweiter);
    TEST_ASSERT_EQUAL_INT32(2, RAD_PlayerNumberOfUnits(erster));
    TEST_ASSERT_EQUAL_INT32(1, RAD_PlayerNumberOfUnits(zweiter));
    TEST_ASSERT_EQUAL_INT32(2, RAD_ReserveNumberOfUnits(RAD_PlayerReserve(erster)));
    TEST_ASSERT_EQUAL_INT32(1, RAD_ReserveNumberOfUnits(RAD_PlayerReserve(zweiter)));

    // In der Reihenfolge der Armee, mit ihren Werten -- und mit dem, was der
    // Pool vergibt.
    const RAD_Unit_t *trupp = RAD_PlayerUnitAt(erster, 0);
    const RAD_Unit_t *panzer = RAD_PlayerUnitAt(erster, 1);
    TEST_ASSERT_EQUAL_STRING("Trupp", trupp->name);
    TEST_ASSERT_EQUAL_STRING("Panzer", panzer->name);
    TEST_ASSERT_EQUAL_INT16(6, trupp->movement);
    TEST_ASSERT_EQUAL_STRING("Soldat", trupp->members[0].profile);
    TEST_ASSERT_EQUAL_INT32(0, trupp->id);
    TEST_ASSERT_EQUAL_INT32(1, panzer->id);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, trupp->state);
    TEST_ASSERT_EQUAL_UINT64(host, trupp->owner);
    TEST_ASSERT_EQUAL_INT16(-1, trupp->x);
    TEST_ASSERT_EQUAL_INT16(-1, trupp->y);

    // Liste und Reserve zeigen auf dieselbe Einheit.
    TEST_ASSERT_TRUE(RAD_ReserveUnitAt(RAD_PlayerReserve(erster), 0) == trupp);

    const RAD_Unit_t *spaeher = RAD_PlayerUnitAt(zweiter, 0);
    TEST_ASSERT_EQUAL_STRING("Spaeher", spaeher->name);
    TEST_ASSERT_EQUAL_UINT64(gast, spaeher->owner);
    TEST_ASSERT_FALSE(RAD_PlayerOwnsUnit(erster, spaeher));

    baue_ab();
}

void test_start_geht_nur_einmal(void)
{
    lege_an();
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_StartGame(game, start));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_STARTED, RAD_StartGame(game, start));
    TEST_ASSERT_EQUAL_INT32(2, game->number_of_players);
    TEST_ASSERT_EQUAL_INT32(3, RAD_UnitPoolNumberOfUnits(game->unit_pool));

    baue_ab();
}

///
/// Im Server tritt bei, wer sein erstes Kommando schickt -- auch vor dem Start.
/// Wer so schon mitspielt, behaelt seine Stelle und bekommt seine Armee; wer
/// nicht in der Datei steht, bleibt einfach dabei.
///
void test_start_nimmt_vorab_beigetretene_mit(void)
{
    lege_an();
    const RAD_UserId_t host = RAD_UserIdFromIdentifier(HOST);
    const RAD_UserId_t gast = RAD_UserIdFromIdentifier(GAST);
    RAD_GameAddPlayer(game, gast);
    RAD_GameAddPlayer(game, 0x42);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_StartGame(game, start));
    TEST_ASSERT_EQUAL_INT32(3, RAD_GameNumberOfPlayers(game));
    TEST_ASSERT_EQUAL_UINT64(gast, RAD_GamePlayerAt(game, 0));
    TEST_ASSERT_EQUAL_UINT64(host, RAD_GamePlayerAt(game, 2));
    TEST_ASSERT_EQUAL_INT32(1, RAD_PlayerNumberOfUnits(RAD_GameFindPlayer(game, gast)));
    TEST_ASSERT_EQUAL_INT32(2, RAD_PlayerNumberOfUnits(RAD_GameFindPlayer(game, host)));
    TEST_ASSERT_EQUAL_INT32(0, RAD_PlayerNumberOfUnits(RAD_GameFindPlayer(game, 0x42)));

    baue_ab();
}

///
/// Passt der zweite Spieler nicht mehr hinein, tritt der erste wieder aus -- aber
/// nur, weil er erst mit diesem Start kam. Wer vorher schon da war, bleibt.
///
void test_start_nimmt_nur_die_eigenen_beitritte_zurueck(void)
{
    lege_an();
    for(int32_t i = 0; i < RAD_MAX_PLAYERS - 1; ++i)
    {
        RAD_GameAddPlayer(game, (RAD_UserId_t)(0x100 + i));
    }

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_FULL, RAD_StartGame(game, start));
    TEST_ASSERT_EQUAL_INT32(RAD_MAX_PLAYERS - 1, RAD_GameNumberOfPlayers(game));
    TEST_ASSERT_FALSE(RAD_GameIsPlaying(game, RAD_UserIdFromIdentifier(HOST)));
    TEST_ASSERT_TRUE(RAD_GameIsPlaying(game, 0x100));
    TEST_ASSERT_EQUAL_INT32(0, RAD_UnitPoolNumberOfUnits(game->unit_pool));

    baue_ab();
}

void test_start_mit_ungueltiger_kennung_aendert_nichts(void)
{
    lege_an();
    strcpy(start->players[1].identifier, "kurz");

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_USER, RAD_StartGame(game, start));
    pruefe_unberuehrt();

    baue_ab();
}

void test_start_lehnt_ab_was_nicht_in_den_pool_passt(void)
{
    lege_an();

    // Mehr, als eine gelesene Armee haben kann: gezaehlt wird vor dem ersten
    // Zugriff auf die Einheiten, deshalb steht dahinter nichts.
    start->players[0].number_of_units = RAD_MAX_UNITS;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_FULL, RAD_StartGame(game, start));
    pruefe_unberuehrt();

    baue_ab();
}

void test_start_ohne_spiel_oder_daten(void)
{
    lege_an();

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_UNIT, RAD_StartGame(NULL, start));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_UNIT, RAD_StartGame(game, NULL));
    pruefe_unberuehrt();

    baue_ab();
}

void test_start_aus_datei(void)
{
    lege_an();
    RAD_GameResult_t grund = RAD_GAME_ERROR_FULL;

    TEST_ASSERT_EQUAL_INT(RAD_GAME_START_OK,
        RAD_StartGameFromFile(game, RAD_SCHEMA_EXAMPLES_DIR "/spielstart/valid/zwei_spieler.json", &grund));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, grund);
    TEST_ASSERT_EQUAL_INT32(2, RAD_GameNumberOfPlayers(game));

    RAD_Player_t *host = RAD_GameFindPlayer(game, RAD_GamePlayerAt(game, 0));
    TEST_ASSERT_EQUAL_INT32(2, RAD_PlayerNumberOfUnits(host));
    TEST_ASSERT_EQUAL_STRING("Trupp", RAD_PlayerUnitAt(host, 0)->name);
    TEST_ASSERT_EQUAL_STRING("Transporter", RAD_PlayerUnitAt(host, 1)->name);

    // Ein zweites Mal: die Datei ist in Ordnung, das Spiel lehnt ab.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_START_ERROR_REJECTED,
        RAD_StartGameFromFile(game, RAD_SCHEMA_EXAMPLES_DIR "/spielstart/valid/zwei_spieler.json", &grund));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_STARTED, grund);

    baue_ab();
}

void test_start_aus_fehlender_datei_aendert_nichts(void)
{
    lege_an();
    RAD_GameResult_t grund = RAD_GAME_ERROR_FULL;

    TEST_ASSERT_EQUAL_INT(RAD_GAME_START_ERROR_NOT_FOUND,
        RAD_StartGameFromFile(game, "/gibt/es/nicht/spielstart.json", &grund));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, grund);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_START_ERROR_NOT_FOUND,
        RAD_StartGameFromFile(game, "/gibt/es/nicht/spielstart.json", NULL));
    pruefe_unberuehrt();

    baue_ab();
}
