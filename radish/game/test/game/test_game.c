#include <unity.h>
#include <stdlib.h>
#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand: er setzt die Figur
// ueber die Welt, die das Kommando danach bewegt. Die Begruendung fuer diesen
// Suchpfad steht in test/CMakeLists.txt.
#include <radish/game/model/game.h>

///
/// Erster Test der Fassade: ein frisches Spiel gibt es, und es spielt niemand
/// mit. Mehr steht hier noch nicht -- die Tests zu Mitspielern, Zug und Besitz
/// kommen aus den bisherigen Pruefprogrammen herueber.
///
void test_game_wird_leer_angelegt(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);

    TEST_ASSERT_EQUAL_INT(0, RAD_GameNumberOfPlayers(game));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_GameCurrentUser(game));

    // Beide nullen den Zeiger des Aufrufers.
    RAD_DestroyGame(&game);
    TEST_ASSERT_NULL(game);

    RAD_DestroyEventManager(&events);
    TEST_ASSERT_NULL(events);
}


///
/// Was beim Abonnenten von einer Bewegung ankommt.
///
/// Der Zeiger auf die Figur wird geprueft und nicht blind dereferenziert: ein
/// abgewiesenes Kommando veroeffentlicht heute ein Ereignis *ohne* Figur (unten),
/// und ein Test, der daran abstuerzt, sagt nichts mehr.
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
} RAD_BewegungMitschrift_t;

static void notiere_moved(void *user_argument, const RAD_Entity_t *entity, const RAD_EntityPath_t *path, int32_t result)
{
    // Ohne Pfad waere das Ereignis keine Bewegung.
    TEST_ASSERT_NOT_NULL(path);

    RAD_BewegungMitschrift_t *mitschrift = user_argument;
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
/// die dieser Test nicht braucht (event_manager.h). Abonniert wird *nach* dem
/// Setzen der Figur -- das spawned-Ereignis geht ihn nichts an.
///
static void abonniere_bewegung(RAD_EventManager_t *events, RAD_BewegungMitschrift_t *mitschrift)
{
    *mitschrift = (RAD_BewegungMitschrift_t){ .moved = 0, .mit_figur = false };

    RAD_EventManagerSubscribeToEntityEvents(events, (RAD_EventsEntityChangedCallback_t){
        .user_argument = mitschrift,
        .spawned = notiere_nichts,
        .destroyed = notiere_nichts,
        .moved = notiere_moved
    });
}

///
/// Aus einem Move-Kommando entsteht ein Ereignis.
///
/// Das ist die Strecke, um die es geht: eine Absicht in Datenform kommt herein
/// (control/command/), RAD_GameExecuteCommand fuehrt sie aus, und heraus kommt eine
/// Meldung an die Abonnenten (control/events/). Der Client zeichnet auf dieses
/// Ereignis hin -- kommt es nicht, hat sich fuer ihn nichts bewegt, gleich was in
/// der Welt steht.
///
/// Das Kommando kommt aus der Fabrik und nicht von Hand: sie schreibt Art,
/// Sequenznummer und Absender hinein, und genau so trifft ein Kommando im Ernstfall
/// ein.
///
void test_game_move_kommando_veroeffentlicht_ein_ereignis(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, (RAD_UserId_t)0x4711);
    TEST_ASSERT_NOT_NULL(game);

    const RAD_EntityId_t id = RAD_WorldSpawnEntity(&game->world, RAD_ENTITY_TYPE_PLAYER, 2, 2);
    TEST_ASSERT_NOT_EQUAL(RAD_ENTITY_NONE, id);

    RAD_BewegungMitschrift_t mitschrift;
    abonniere_bewegung(events, &mitschrift);

    const RAD_EntityPath_t weg = {
        .steps_to = { { .x = 3, .y = 2 } },
        .number_of_steps = 1
    };

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveEntity(game, id, &weg, &command));

    // Bis hierher ist nichts geschehen: die Fabrik fuellt ein Kommando aus und
    // fuehrt es nicht aus.
    TEST_ASSERT_EQUAL_INT(0, mitschrift.moved);

    RAD_GameExecuteCommand(game, &command);

    // Genau eines, nicht keines und nicht zwei.
    TEST_ASSERT_EQUAL_INT(1, mitschrift.moved);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.ergebnis);

    // Es traegt die Figur, die das Kommando benannt hat.
    TEST_ASSERT_TRUE(mitschrift.mit_figur);
    TEST_ASSERT_EQUAL_INT(id, mitschrift.id);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.x);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.y);

    // Und den gelaufenen Weg: die betretenen Felder, nicht das verlassene (path.h).
    TEST_ASSERT_EQUAL_INT(1, mitschrift.pfad.number_of_steps);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.pfad.steps_to[0].x);
    TEST_ASSERT_EQUAL_INT(2, mitschrift.pfad.steps_to[0].y);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

///
/// Auch ein Kommando, das nicht ausgefuehrt werden kann, meldet sich.
///
/// Ein Absender wartet auf Antwort; bliebe eine abgewiesene Bewegung still, waere
/// sie von einer verlorenen nicht zu unterscheiden. Der Weg im Ereignis ist dann
/// leer und das Ergebnis nicht 0 -- daran erkennt ein Abonnent, dass nichts
/// geschehen ist.
///
/// **Befund, keine Zusage:** heute reicht dieses Ereignis *keine* Figur heraus,
/// sondern NULL -- es gibt sie ja nicht. Wer abonniert, darf den Zeiger deshalb
/// nicht ungeprueft dereferenzieren. Der Test haelt das fest, ohne es zu segnen.
///
void test_game_move_kommando_ohne_figur_meldet_ein_ergebnis(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Game_t *game = RAD_CreateGame(events, (RAD_UserId_t)0x4711);
    TEST_ASSERT_NOT_NULL(game);

    RAD_BewegungMitschrift_t mitschrift;
    abonniere_bewegung(events, &mitschrift);

    // Auf 7 steht keine Figur -- es steht ueberhaupt keine in dieser Welt.
    const RAD_EntityPath_t weg = {
        .steps_to = { { .x = 1, .y = 1 } },
        .number_of_steps = 1
    };

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveEntity(game, 7, &weg, &command));

    RAD_GameExecuteCommand(game, &command);

    TEST_ASSERT_EQUAL_INT(1, mitschrift.moved);
    TEST_ASSERT_NOT_EQUAL(0, mitschrift.ergebnis);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.pfad.number_of_steps);

    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}
