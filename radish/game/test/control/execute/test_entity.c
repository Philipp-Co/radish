#include <unity.h>
#include <radish/game/game.h>

///
/// Die Fabriken fuellen den Kopf: Art, die naechste Sequenznummer und den
/// Absender aus dem Spiel. Genau dafuer gibt es sie -- eine Aufrufstelle soll den
/// Absender nicht mitschleppen und ihn nicht vergessen koennen.
///
void test_fabrik_fuellt_den_kopf(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    RAD_Game_t *game = RAD_CreateGame(events, (RAD_UserId_t)0x4711);

    // Ein Weg und keine zwei Koordinatenpaare: wo die Figur losgeht, weiss sie
    // selbst, das Kommando traegt nur die betretenen Felder (path.h).
    const RAD_EntityPath_t path = {
        .steps_to = { { .x = 1, .y = 0 } },
        .number_of_steps = 1
    };

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveEntity(game, 3, &path, &command));

    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_MOVE_ENTITY, command.header.type);
    TEST_ASSERT_EQUAL_UINT64((RAD_UserId_t)0x4711, command.header.user);
    TEST_ASSERT_EQUAL_UINT64(1, command.header.sequence);
    TEST_ASSERT_EQUAL_INT(3, command.command.move_entity.entity);

    // Kopiert wird der Pfad, nicht der Zeiger darauf.
    TEST_ASSERT_EQUAL_INT(1, command.command.move_entity.path.number_of_steps);
    TEST_ASSERT_EQUAL_INT(1, command.command.move_entity.path.steps_to[0].x);
    TEST_ASSERT_EQUAL_INT(0, command.command.move_entity.path.steps_to[0].y);

    // Die zweite bekommt die naechste Nummer.
    const RAD_EntityPath_t weiter = {
        .steps_to = { { .x = 2, .y = 0 } },
        .number_of_steps = 1
    };

    TEST_ASSERT_TRUE(RAD_GameMoveEntity(game, 3, &weiter, &command));
    TEST_ASSERT_EQUAL_UINT64(2, command.header.sequence);

    // Ein Pfad ohne Schritte ist keiner: abgewiesen, ohne eine Nummer zu
    // verbrauchen (execute/entity.c).
    const RAD_EntityPath_t leer = { .number_of_steps = 0 };
    TEST_ASSERT_FALSE(RAD_GameMoveEntity(game, 3, &leer, &command));
    TEST_ASSERT_EQUAL_UINT64(2, command.header.sequence);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}
