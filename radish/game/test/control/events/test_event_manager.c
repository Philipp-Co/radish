#include <unity.h>
#include <radish/game/control/events/event_manager.h>

///
/// Ein Abonnent bekommt sein user_argument zurueck -- daran haengt alles Weitere,
/// weil ein Callback sonst seinen eigenen Zustand nicht wiederfindet.
///
static void on_tile(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
    *(int*)user_argument += 1;
}

void test_manager_reicht_ein_ereignis_durch(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    int gezaehlt = 0;
    RAD_EventManagerSubscribeToTileEvents(events, (RAD_EventsTileChangedCallback_t){
        .user_argument = &gezaehlt,
        .added = on_tile,
        .removed = on_tile,
        .changed = on_tile
    });

    RAD_EventManagerPublishTileAddedToGameEvent(events, NULL);
    RAD_EventManagerPublishTileStateChangeEvent(events, NULL);

    TEST_ASSERT_EQUAL_INT(2, gezaehlt);

    RAD_DestroyEventManager(&events);
}

///
/// Die Einheiten je Spieler (RAD_EventManagerSubscribeToUnitEventsOf): wer
/// bekommt was, wenn eine Empfaengerliste mitkommt.
///

static const RAD_UserId_t anna = (RAD_UserId_t)0xA;
static const RAD_UserId_t bert = (RAD_UserId_t)0xB;
static const RAD_UserId_t cleo = (RAD_UserId_t)0xC;

/// Was ein Abonnent mitbekommen hat, je Art gezaehlt.
typedef struct
{
    int spawned;
    int destroyed;
    int moved;
    int attacked;
    int sighted;
    int lost;
    RAD_UnitId_t last_lost;
} RAD_Mitschrift_t;

static void mitschrift_spawned(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)unit; (void)x; (void)y;
    ((RAD_Mitschrift_t*)user_argument)->spawned++;
}

static void mitschrift_destroyed(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)unit; (void)x; (void)y;
    ((RAD_Mitschrift_t*)user_argument)->destroyed++;
}

static void mitschrift_moved(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)unit; (void)path; (void)result;
    ((RAD_Mitschrift_t*)user_argument)->moved++;
}

static void mitschrift_attacked(void *user_argument, const RAD_AttackResult_t *result)
{
    (void)result;
    ((RAD_Mitschrift_t*)user_argument)->attacked++;
}

static void mitschrift_sighted(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)unit; (void)x; (void)y;
    ((RAD_Mitschrift_t*)user_argument)->sighted++;
}

static void mitschrift_lost(void *user_argument, RAD_UnitId_t unit, int32_t x, int32_t y)
{
    (void)x; (void)y;
    ((RAD_Mitschrift_t*)user_argument)->lost++;
    ((RAD_Mitschrift_t*)user_argument)->last_lost = unit;
}

static RAD_EventsUnitChangedCallback_t schreibe_mit(RAD_Mitschrift_t *mitschrift)
{
    *mitschrift = (RAD_Mitschrift_t){ .last_lost = RAD_UNIT_NONE };
    return (RAD_EventsUnitChangedCallback_t){
        .user_argument = mitschrift,
        .spawned = mitschrift_spawned,
        .destroyed = mitschrift_destroyed,
        .moved = mitschrift_moved,
        .attacked = mitschrift_attacked,
        .sighted = mitschrift_sighted,
        .lost = mitschrift_lost
    };
}

static int summe(const RAD_Mitschrift_t *m)
{
    return m->spawned + m->destroyed + m->moved + m->attacked + m->sighted + m->lost;
}

void test_empfaengerliste_nimmt_jeden_einmal(void)
{
    RAD_Recipients_t liste = {0};

    TEST_ASSERT_TRUE(RAD_RecipientsAdd(&liste, anna));
    TEST_ASSERT_TRUE(RAD_RecipientsAdd(&liste, anna));
    TEST_ASSERT_TRUE(RAD_RecipientsAdd(&liste, bert));
    TEST_ASSERT_EQUAL_INT(2, liste.number_of_users);
    TEST_ASSERT_TRUE(RAD_RecipientsContains(&liste, bert));
    TEST_ASSERT_FALSE(RAD_RecipientsContains(&liste, cleo));

    TEST_ASSERT_FALSE(RAD_RecipientsAdd(&liste, RAD_USER_NONE));
    TEST_ASSERT_FALSE(RAD_RecipientsAdd(NULL, anna));
    TEST_ASSERT_FALSE(RAD_RecipientsContains(NULL, anna));
    TEST_ASSERT_EQUAL_INT(2, liste.number_of_users);

    // Voll ist voll -- wer schon drin steht, steht weiter drin.
    for(RAD_UserId_t user = 0x100; liste.number_of_users < RAD_EVENT_MAX_RECIPIENTS; ++user)
    {
        TEST_ASSERT_TRUE(RAD_RecipientsAdd(&liste, user));
    }
    TEST_ASSERT_FALSE(RAD_RecipientsAdd(&liste, cleo));
    TEST_ASSERT_TRUE(RAD_RecipientsAdd(&liste, anna));
    TEST_ASSERT_EQUAL_INT(RAD_EVENT_MAX_RECIPIENTS, liste.number_of_users);
}

///
/// Jedes Ereignis erreicht die Spieler der Liste, die abonniert haben, und nur
/// sie. Wer in der Liste steht, aber kein Abo hat, wird uebergangen.
///
void test_ereignis_erreicht_nur_die_empfaenger(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Mitschrift_t von_anna, von_bert;
    TEST_ASSERT_TRUE(RAD_EventManagerSubscribeToUnitEventsOf(events, anna, schreibe_mit(&von_anna)));
    TEST_ASSERT_TRUE(RAD_EventManagerSubscribeToUnitEventsOf(events, bert, schreibe_mit(&von_bert)));

    RAD_Recipients_t nur_anna = {0};
    RAD_RecipientsAdd(&nur_anna, anna);
    RAD_RecipientsAdd(&nur_anna, cleo);

    const RAD_Path_t weg = { .number_of_steps = 0 };
    const RAD_AttackResult_t angriff = { .attacker = 1, .target = RAD_UNIT_NONE };

    RAD_EventManagerPublishUnitSpawned(events, &nur_anna, NULL, 1, 2);
    RAD_EventManagerPublishUnitDestroyed(events, &nur_anna, NULL, 1, 2);
    RAD_EventManagerPublishUnitMoved(events, &nur_anna, NULL, &weg, 0);
    RAD_EventManagerPublishUnitAttacked(events, &nur_anna, &angriff);
    RAD_EventManagerPublishUnitSighted(events, &nur_anna, NULL, 1, 2);
    RAD_EventManagerPublishUnitLost(events, &nur_anna, 7, 1, 2);

    TEST_ASSERT_EQUAL_INT(1, von_anna.spawned);
    TEST_ASSERT_EQUAL_INT(1, von_anna.destroyed);
    TEST_ASSERT_EQUAL_INT(1, von_anna.moved);
    TEST_ASSERT_EQUAL_INT(1, von_anna.attacked);
    TEST_ASSERT_EQUAL_INT(1, von_anna.sighted);
    TEST_ASSERT_EQUAL_INT(1, von_anna.lost);
    TEST_ASSERT_EQUAL_INT(7, von_anna.last_lost);
    TEST_ASSERT_EQUAL_INT(0, summe(&von_bert));

    // Ohne Liste, mit einer leeren: kein Spieler.
    const RAD_Recipients_t niemand = {0};
    RAD_EventManagerPublishUnitSpawned(events, NULL, NULL, 1, 2);
    RAD_EventManagerPublishUnitSpawned(events, &niemand, NULL, 1, 2);
    TEST_ASSERT_EQUAL_INT(1, von_anna.spawned);
    TEST_ASSERT_EQUAL_INT(0, summe(&von_bert));

    RAD_DestroyEventManager(&events);
}

///
/// Der Abonnent ohne Empfaenger bekommt jedes Ereignis genau einmal -- gleich,
/// wie viele Spieler in der Liste stehen, und auch ohne Liste.
///
void test_ohne_empfaenger_abonniert_bekommt_alles_einmal(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Mitschrift_t schiedsrichter, von_anna, von_bert;
    RAD_EventManagerSubscribeToUnitEvents(events, schreibe_mit(&schiedsrichter));
    RAD_EventManagerSubscribeToUnitEventsOf(events, anna, schreibe_mit(&von_anna));
    RAD_EventManagerSubscribeToUnitEventsOf(events, bert, schreibe_mit(&von_bert));

    RAD_Recipients_t beide = {0};
    RAD_RecipientsAdd(&beide, anna);
    RAD_RecipientsAdd(&beide, bert);

    RAD_EventManagerPublishUnitSpawned(events, &beide, NULL, 0, 0);
    RAD_EventManagerPublishUnitSpawned(events, NULL, NULL, 0, 0);

    TEST_ASSERT_EQUAL_INT(2, schiedsrichter.spawned);
    TEST_ASSERT_EQUAL_INT(1, von_anna.spawned);
    TEST_ASSERT_EQUAL_INT(1, von_bert.spawned);

    RAD_DestroyEventManager(&events);
}

///
/// Ein zweites Abo desselben Spielers ersetzt das erste; abgemeldet bekommt er
/// nichts mehr, und die anderen bleiben, wie sie waren.
///
void test_abo_ersetzen_und_abmelden(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Mitschrift_t erstes, zweites, von_bert;
    TEST_ASSERT_TRUE(RAD_EventManagerSubscribeToUnitEventsOf(events, anna, schreibe_mit(&erstes)));
    TEST_ASSERT_TRUE(RAD_EventManagerSubscribeToUnitEventsOf(events, bert, schreibe_mit(&von_bert)));
    TEST_ASSERT_TRUE(RAD_EventManagerSubscribeToUnitEventsOf(events, anna, schreibe_mit(&zweites)));

    RAD_Recipients_t beide = {0};
    RAD_RecipientsAdd(&beide, anna);
    RAD_RecipientsAdd(&beide, bert);

    RAD_EventManagerPublishUnitSighted(events, &beide, NULL, 0, 0);
    TEST_ASSERT_EQUAL_INT(0, erstes.sighted);
    TEST_ASSERT_EQUAL_INT(1, zweites.sighted);
    TEST_ASSERT_EQUAL_INT(1, von_bert.sighted);

    RAD_EventManagerUnsubscribeUser(events, anna);
    RAD_EventManagerUnsubscribeUser(events, cleo);
    RAD_EventManagerPublishUnitSighted(events, &beide, NULL, 0, 0);
    TEST_ASSERT_EQUAL_INT(1, zweites.sighted);
    TEST_ASSERT_EQUAL_INT(2, von_bert.sighted);

    RAD_DestroyEventManager(&events);
}

///
/// Hoechstens RAD_EVENT_MAX_RECIPIENTS Spieler; ein weiterer wird abgewiesen, ein
/// schon abonnierter darf sein Abo trotzdem ersetzen, und RAD_USER_NONE nie.
/// Nach dem Abmelden ist wieder Platz.
///
void test_volle_abo_tabelle_weist_ab(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Mitschrift_t mitschrift;
    const RAD_EventsUnitChangedCallback_t callbacks = schreibe_mit(&mitschrift);

    TEST_ASSERT_FALSE(RAD_EventManagerSubscribeToUnitEventsOf(events, RAD_USER_NONE, callbacks));

    for(RAD_UserId_t user = 1; user <= RAD_EVENT_MAX_RECIPIENTS; ++user)
    {
        TEST_ASSERT_TRUE(RAD_EventManagerSubscribeToUnitEventsOf(events, user, callbacks));
    }
    TEST_ASSERT_FALSE(RAD_EventManagerSubscribeToUnitEventsOf(events, 0x100, callbacks));
    TEST_ASSERT_TRUE(RAD_EventManagerSubscribeToUnitEventsOf(events, 1, callbacks));

    RAD_EventManagerUnsubscribeUser(events, 3);
    TEST_ASSERT_TRUE(RAD_EventManagerSubscribeToUnitEventsOf(events, 0x100, callbacks));

    RAD_DestroyEventManager(&events);
}

///
/// Ein Abonnent, der die neuen Zeiger nicht setzt, laesst sie NULL -- der
/// Manager setzt die Voreinstellung ein, statt NULL aufzurufen.
///
void test_fehlende_callbacks_werden_voreingestellt(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Mitschrift_t mitschrift = {0};
    const RAD_EventsUnitChangedCallback_t nur_spawned = {
        .user_argument = &mitschrift,
        .spawned = mitschrift_spawned
    };
    RAD_EventManagerSubscribeToUnitEvents(events, nur_spawned);
    RAD_EventManagerSubscribeToUnitEventsOf(events, anna, nur_spawned);

    RAD_Recipients_t nur_anna = {0};
    RAD_RecipientsAdd(&nur_anna, anna);

    const RAD_Path_t weg = { .number_of_steps = 0 };
    RAD_EventManagerPublishUnitSpawned(events, &nur_anna, NULL, 0, 0);
    RAD_EventManagerPublishUnitMoved(events, &nur_anna, NULL, &weg, 0);
    RAD_EventManagerPublishUnitSighted(events, &nur_anna, NULL, 0, 0);
    RAD_EventManagerPublishUnitLost(events, &nur_anna, 1, 0, 0);

    TEST_ASSERT_EQUAL_INT(2, mitschrift.spawned);

    RAD_DestroyEventManager(&events);
}

///
/// Eine weitere Fassung (RAD_Recipients_t.variant) geht nur an ihre Liste --
/// der Abonnent ohne Empfaenger hat das Ereignis schon mit der ersten gesehen.
///
void test_weitere_fassung_nur_an_die_liste(void)
{
    RAD_EventManager_t *events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);

    RAD_Mitschrift_t schiedsrichter, von_anna, von_bert;
    RAD_EventManagerSubscribeToUnitEvents(events, schreibe_mit(&schiedsrichter));
    RAD_EventManagerSubscribeToUnitEventsOf(events, anna, schreibe_mit(&von_anna));
    RAD_EventManagerSubscribeToUnitEventsOf(events, bert, schreibe_mit(&von_bert));

    RAD_Recipients_t erste = {0};
    RAD_RecipientsAdd(&erste, anna);
    RAD_Recipients_t weitere = { .variant = true };
    RAD_RecipientsAdd(&weitere, bert);

    const RAD_AttackResult_t angriff = { .attacker = 1, .target = RAD_UNIT_NONE };
    RAD_EventManagerPublishUnitAttacked(events, &erste, &angriff);
    RAD_EventManagerPublishUnitAttacked(events, &weitere, &angriff);

    TEST_ASSERT_EQUAL_INT(1, schiedsrichter.attacked);
    TEST_ASSERT_EQUAL_INT(1, von_anna.attacked);
    TEST_ASSERT_EQUAL_INT(1, von_bert.attacked);

    RAD_DestroyEventManager(&events);
}
