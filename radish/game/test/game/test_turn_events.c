#include <unity.h>
#include <string.h>
#include <radish/game/game.h>

///
/// Der Zugwechsel als Ereignis (RAD_EventManagerPublishTurnChanged): das Spiel
/// meldet jede Aenderung daran, wer dran ist, und jedes erfolgreiche Abgeben.
///

typedef struct
{
    int gemeldet;
    RAD_UserId_t zuletzt;
} RAD_ZugwechselMitschrift_t;

static RAD_EventManager_t *events;
static RAD_Game_t *game;
static RAD_ZugwechselMitschrift_t mitschrift;

static void notiere_zugwechsel(void *user_argument, RAD_UserId_t current_user)
{
    RAD_ZugwechselMitschrift_t *m = user_argument;
    m->gemeldet += 1;
    m->zuletzt = current_user;
}

static void aufbauen(void)
{
    memset(&mitschrift, 0, sizeof(mitschrift));
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    RAD_EventManagerSubscribeToTurnEvents(events, (RAD_EventsTurnCallback_t){
        .user_argument = &mitschrift,
        .changed = notiere_zugwechsel
    });
    game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
}

static void abbauen(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

void test_zugwechsel_erster_beitritt_meldet_ihn(void)
{
    aufbauen();

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, 0x10));
    TEST_ASSERT_EQUAL_INT(1, mitschrift.gemeldet);
    TEST_ASSERT_EQUAL_UINT64(0x10, mitschrift.zuletzt);

    // Der zweite haengt sich hinten an, wer dran ist, bleibt -- und derselbe noch
    // einmal aendert gar nichts.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, 0x20));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, 0x10));
    TEST_ASSERT_EQUAL_INT(1, mitschrift.gemeldet);

    abbauen();
}

void test_zugwechsel_abgeben_meldet_den_naechsten(void)
{
    aufbauen();
    RAD_GameAddPlayer(game, 0x10);
    RAD_GameAddPlayer(game, 0x20);
    mitschrift.gemeldet = 0;

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, 0x10));
    TEST_ASSERT_EQUAL_INT(1, mitschrift.gemeldet);
    TEST_ASSERT_EQUAL_UINT64(0x20, mitschrift.zuletzt);

    // Abgelehnt: wer nicht dran ist, gibt nichts ab, und gemeldet wird nichts.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NOT_YOUR_TURN, RAD_GameEndTurn(game, 0x10));
    TEST_ASSERT_EQUAL_INT(1, mitschrift.gemeldet);

    abbauen();
}

void test_zugwechsel_allein_abgeben_meldet_ihn_erneut(void)
{
    aufbauen();
    RAD_GameAddPlayer(game, 0x10);
    mitschrift.gemeldet = 0;

    // Derselbe ist wieder dran -- aber es ist ein neuer Zug.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, 0x10));
    TEST_ASSERT_EQUAL_INT(1, mitschrift.gemeldet);
    TEST_ASSERT_EQUAL_UINT64(0x10, mitschrift.zuletzt);

    abbauen();
}

void test_zugwechsel_austritt_meldet_nur_eine_aenderung(void)
{
    aufbauen();
    RAD_GameAddPlayer(game, 0x10);
    RAD_GameAddPlayer(game, 0x20);
    RAD_GameAddPlayer(game, 0x30);
    mitschrift.gemeldet = 0;

    // Wer nicht dran ist, geht still.
    RAD_GameRemovePlayer(game, 0x30);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.gemeldet);

    // Geht der, der dran ist, ist sein Nachfolger dran.
    RAD_GameRemovePlayer(game, 0x10);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.gemeldet);
    TEST_ASSERT_EQUAL_UINT64(0x20, mitschrift.zuletzt);

    // Geht der letzte, ist niemand mehr dran.
    RAD_GameRemovePlayer(game, 0x20);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.gemeldet);
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, mitschrift.zuletzt);

    abbauen();
}
