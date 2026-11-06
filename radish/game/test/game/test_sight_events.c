#include <unity.h>
#include <string.h>
#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand: er setzt
// Einheiten direkt in die Welt. Die Begruendung fuer diesen Suchpfad steht in
// test/CMakeLists.txt.
#include <radish/game/model/game.h>
#include <radish/game/model/world/world.h>

///
/// Was jemandem in die Sicht kommt und aus ihr verschwindet, gemeldet von
/// RAD_GameExecuteCommand nach jedem Kommando (control/visibility.h). Zwei
/// Spieler; mitgeschrieben wird je Spieler, in der Reihenfolge der Meldungen.
///

static const RAD_UserId_t ich = (RAD_UserId_t)0x4711;
static const RAD_UserId_t gegner = (RAD_UserId_t)0x0815;

static RAD_EventManager_t *events;
static RAD_Game_t *game;

/// Eine Meldung: 's' in Sicht, 'l' aus der Sicht, 'm' Zug, 'd' zerstoert, 'a' aufgestellt.
typedef struct
{
    char art;
    RAD_UnitId_t unit;
    int32_t x;
    int32_t y;
    int8_t schritte;
} RAD_Meldung_t;

typedef struct
{
    RAD_Meldung_t meldungen[16];
    int32_t anzahl;
} RAD_SichtMitschrift_t;

static RAD_SichtMitschrift_t von_mir;
static RAD_SichtMitschrift_t vom_gegner;

static void notiere(RAD_SichtMitschrift_t *m, RAD_Meldung_t meldung)
{
    TEST_ASSERT_LESS_THAN_INT(16, m->anzahl);
    m->meldungen[m->anzahl++] = meldung;
}

static void notiere_sighted(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    notiere(user_argument, (RAD_Meldung_t){ .art = 's', .unit = unit->id, .x = x, .y = y });
}

static void notiere_lost(void *user_argument, RAD_UnitId_t unit, int32_t x, int32_t y)
{
    notiere(user_argument, (RAD_Meldung_t){ .art = 'l', .unit = unit, .x = x, .y = y });
}

static void notiere_moved(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)result;
    notiere(user_argument, (RAD_Meldung_t){
        .art = 'm', .unit = (unit != NULL) ? unit->id : RAD_UNIT_NONE, .schritte = path->number_of_steps
    });
}

static void notiere_destroyed(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    notiere(user_argument, (RAD_Meldung_t){ .art = 'd', .unit = unit->id, .x = x, .y = y });
}

static void notiere_spawned(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    notiere(user_argument, (RAD_Meldung_t){ .art = 'a', .unit = unit->id, .x = x, .y = y });
}

static RAD_EventsUnitChangedCallback_t mitschrift(RAD_SichtMitschrift_t *m)
{
    memset(m, 0, sizeof(*m));
    return (RAD_EventsUnitChangedCallback_t){
        .user_argument = m,
        .spawned = notiere_spawned,
        .destroyed = notiere_destroyed,
        .moved = notiere_moved,
        .sighted = notiere_sighted,
        .lost = notiere_lost
    };
}

static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    game = RAD_CreateGame(events, ich);
    TEST_ASSERT_NOT_NULL(game);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, ich));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, gegner));
}

/// Erst abonnieren, wenn alles steht: das Aufstellen geht diese Tests nichts an.
static void mitschreiben(void)
{
    RAD_EventManagerSubscribeToUnitEventsOf(events, ich, mitschrift(&von_mir));
    RAD_EventManagerSubscribeToUnitEventsOf(events, gegner, mitschrift(&vom_gegner));
}

static void abbauen(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

/// Eine Einheit von "besitzer" auf (x, y), die "sicht" Felder weit sieht und
/// fuenf weit ziehen kann.
static RAD_UnitId_t figur(RAD_UserId_t besitzer, int16_t x, int16_t y, uint8_t sicht)
{
    const RAD_UnitId_t id = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, x, y, NULL);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, id);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameBindUnit(game, besitzer, id));
    RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, id);
    unit->sight_range = sicht;
    unit->movement = 5;
    unit->number_of_members = 1;
    unit->members[0].health = 1;
    unit->members[0].number_of_weapons = 1;
    unit->members[0].weapons[0].shots = 1;
    unit->members[0].weapons[0].max_range = 5;
    unit->members[0].weapons[0].strength = 10;
    unit->members[0].weapons[0].penetration = 10;
    unit->members[0].accuracy = 1;
    return id;
}

/// Zieht "figur" waagerecht von x0 nach x1 auf Zeile "y", Feld fuer Feld.
static void ziehe_waagerecht(RAD_UnitId_t figur, int16_t x0, int16_t x1, int16_t y)
{
    RAD_Path_t weg = { .number_of_steps = 0 };
    const int16_t schritt = (x1 > x0) ? 1 : -1;
    for(int16_t x = x0; ; x = (int16_t)(x + schritt))
    {
        weg.steps_to[weg.number_of_steps].x = x;
        weg.steps_to[weg.number_of_steps].y = y;
        weg.number_of_steps++;
        if(x == x1)
        {
            break;
        }
    }

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameMoveUnit(game, figur, &weg, &command));
    RAD_GameExecuteCommand(game, &command);
}

///
/// Mein Zug bringt eine stehende fremde Einheit in meine Sicht: ich bekomme
/// "in Sicht" mit ihrem Feld, nach dem Zug selbst. Der Gegner sieht nichts.
///
void test_sicht_eigener_zug_deckt_eine_stehende_einheit_auf(void)
{
    aufbauen();
    const RAD_UnitId_t meine = figur(ich, 2, 2, 2);
    const RAD_UnitId_t fremde = figur(gegner, 6, 2, 0);
    mitschreiben();

    ziehe_waagerecht(meine, 2, 4, 2);

    TEST_ASSERT_EQUAL_INT(2, von_mir.anzahl);
    TEST_ASSERT_EQUAL_CHAR('m', von_mir.meldungen[0].art);
    TEST_ASSERT_EQUAL_INT(meine, von_mir.meldungen[0].unit);
    TEST_ASSERT_EQUAL_CHAR('s', von_mir.meldungen[1].art);
    TEST_ASSERT_EQUAL_INT(fremde, von_mir.meldungen[1].unit);
    TEST_ASSERT_EQUAL_INT(6, von_mir.meldungen[1].x);
    TEST_ASSERT_EQUAL_INT(2, von_mir.meldungen[1].y);
    TEST_ASSERT_EQUAL_INT(0, vom_gegner.anzahl);

    abbauen();
}

///
/// Mein Zug weg von ihr laesst sie aus meiner Sicht fallen -- gemeldet mit dem
/// Feld, auf dem sie steht und auf dem ich sie zuletzt sah.
///
void test_sicht_eigener_zug_verliert_eine_stehende_einheit(void)
{
    aufbauen();
    const RAD_UnitId_t meine = figur(ich, 4, 2, 2);
    const RAD_UnitId_t fremde = figur(gegner, 6, 2, 0);
    mitschreiben();

    ziehe_waagerecht(meine, 4, 2, 2);

    TEST_ASSERT_EQUAL_INT(2, von_mir.anzahl);
    TEST_ASSERT_EQUAL_CHAR('m', von_mir.meldungen[0].art);
    TEST_ASSERT_EQUAL_CHAR('l', von_mir.meldungen[1].art);
    TEST_ASSERT_EQUAL_INT(fremde, von_mir.meldungen[1].unit);
    TEST_ASSERT_EQUAL_INT(6, von_mir.meldungen[1].x);
    TEST_ASSERT_EQUAL_INT(2, von_mir.meldungen[1].y);
    TEST_ASSERT_EQUAL_INT(0, vom_gegner.anzahl);

    abbauen();
}

///
/// Eine fremde Einheit zieht in meine Sicht: erst der Teil des Weges, den ich
/// sehe, mit dem Randfeld davor, aus dem sie kommt -- dann "in Sicht" mit dem
/// Feld, auf dem sie jetzt steht.
///
void test_sicht_fremder_zug_hinein(void)
{
    aufbauen();
    // Ich sehe von (5, 6) drei Felder weit: von ihrem Weg (6, 4) und (5, 4), dazu
    // kommt (7, 4) als Randfeld.
    figur(ich, 5, 6, 3);
    const RAD_UnitId_t fremde = figur(gegner, 9, 4, 0);
    mitschreiben();

    ziehe_waagerecht(fremde, 9, 5, 4);

    TEST_ASSERT_EQUAL_INT(2, von_mir.anzahl);
    TEST_ASSERT_EQUAL_CHAR('m', von_mir.meldungen[0].art);
    TEST_ASSERT_EQUAL_INT(fremde, von_mir.meldungen[0].unit);
    TEST_ASSERT_EQUAL_INT(3, von_mir.meldungen[0].schritte);
    TEST_ASSERT_EQUAL_CHAR('s', von_mir.meldungen[1].art);
    TEST_ASSERT_EQUAL_INT(fremde, von_mir.meldungen[1].unit);
    TEST_ASSERT_EQUAL_INT(5, von_mir.meldungen[1].x);
    TEST_ASSERT_EQUAL_INT(4, von_mir.meldungen[1].y);

    abbauen();
}

///
/// Eine fremde Einheit zieht aus meiner Sicht hinaus: erst der Teil des Weges,
/// den ich sehe, mit dem Randfeld danach, in das sie verschwindet -- dann "aus
/// der Sicht" mit dem letzten Feld, das ich sah, nicht mit dem, auf dem sie
/// jetzt steht.
///
void test_sicht_fremder_zug_hinaus(void)
{
    aufbauen();
    figur(ich, 2, 5, 2);
    const RAD_UnitId_t fremde = figur(gegner, 2, 4, 0);
    mitschreiben();

    ziehe_waagerecht(fremde, 2, 7, 4);

    TEST_ASSERT_EQUAL_INT(2, von_mir.anzahl);
    TEST_ASSERT_EQUAL_CHAR('m', von_mir.meldungen[0].art);
    TEST_ASSERT_EQUAL_INT(3, von_mir.meldungen[0].schritte);
    TEST_ASSERT_EQUAL_CHAR('l', von_mir.meldungen[1].art);
    TEST_ASSERT_EQUAL_INT(fremde, von_mir.meldungen[1].unit);
    TEST_ASSERT_EQUAL_INT(3, von_mir.meldungen[1].x);
    TEST_ASSERT_EQUAL_INT(4, von_mir.meldungen[1].y);

    abbauen();
}

///
/// Eine Zerstoerung meldet sich selbst: wer die Einheit sah, bekommt
/// "zerstoert" und kein "aus der Sicht" dazu.
///
void test_sicht_zerstoerung_meldet_kein_aus_der_sicht(void)
{
    aufbauen();
    const RAD_UnitId_t meine = figur(ich, 2, 2, 2);
    const RAD_UnitId_t fremde = figur(gegner, 4, 2, 0);
    mitschreiben();

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameAttack(game, meine, 4, 2, &command));
    RAD_GameExecuteCommand(game, &command);

    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DESTROYED, RAD_WorldUnitById(&game->world, fremde)->state);
    TEST_ASSERT_EQUAL_INT(1, von_mir.anzahl);
    TEST_ASSERT_EQUAL_CHAR('d', von_mir.meldungen[0].art);
    TEST_ASSERT_EQUAL_INT(fremde, von_mir.meldungen[0].unit);

    abbauen();
}

///
/// Ein Aufstellen meldet sich selbst: wer das Feld sieht, bekommt "aufgestellt"
/// und kein "in Sicht" dazu.
///
void test_sicht_aufstellen_meldet_kein_in_sicht(void)
{
    aufbauen();
    figur(ich, 3, 4, 3);

    RAD_Unit_t values;
    memset(&values, 0, sizeof(values));
    strcpy(values.name, "Trupp");
    values.number_of_members = 1;
    RAD_UnitId_t fremde = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, gegner, &values, &fremde));
    int32_t min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    TEST_ASSERT_TRUE(RAD_GameDeployZone(game, gegner, &min_x, &min_y, &max_x, &max_y));
    mitschreiben();

    // Am naechsten Feld der Zone, das ich sehe; die Zone liegt am anderen Rand.
    figur(ich, (int16_t)(min_x - 1), 4, 1);
    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameDeployUnit(game, fremde, (int16_t)min_x, 4, &command));
    command.header.user = gegner;
    RAD_GameExecuteCommand(game, &command);

    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, RAD_WorldUnitById(&game->world, fremde)->state);
    TEST_ASSERT_EQUAL_INT(1, von_mir.anzahl);
    TEST_ASSERT_EQUAL_CHAR('a', von_mir.meldungen[0].art);
    TEST_ASSERT_EQUAL_INT(fremde, von_mir.meldungen[0].unit);

    abbauen();
}

///
/// Lesen fuer einen Spieler (RAD_GameTileAtFor, RAD_GameUnitAtFor): das Feld
/// ohne die Einheit, die er nicht sieht; die Einheiten nur, die er kennen darf.
///
void test_sicht_lesen_je_spieler(void)
{
    aufbauen();
    const RAD_UnitId_t meine = figur(ich, 2, 2, 2);
    const RAD_UnitId_t nah = figur(gegner, 3, 2, 0);
    const RAD_UnitId_t fern = figur(gegner, 8, 8, 0);

    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_GameTileAtFor(game, ich, 3, 2, &tile));
    TEST_ASSERT_EQUAL_INT(nah, tile.unit);
    TEST_ASSERT_TRUE(RAD_GameTileAtFor(game, ich, 8, 8, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, tile.unit);
    TEST_ASSERT_TRUE(RAD_GameTileAtFor(game, gegner, 2, 2, &tile));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, tile.unit);
    TEST_ASSERT_TRUE(RAD_GameTileAtFor(game, gegner, 8, 8, &tile));
    TEST_ASSERT_EQUAL_INT(fern, tile.unit);
    TEST_ASSERT_FALSE(RAD_GameTileAtFor(game, ich, 3, 2, NULL));

    // Ueber alle Indizes: ich kenne meine und die nahe, der Gegner beide eigenen.
    int32_t ich_kenne = 0, gegner_kennt = 0;
    for(int32_t i = 0; i < RAD_GameNumberOfUnits(game); ++i)
    {
        RAD_Unit_t unit;
        if(RAD_GameUnitAtFor(game, ich, i, &unit))
        {
            TEST_ASSERT_TRUE((unit.id == meine) || (unit.id == nah));
            ich_kenne++;
        }
        if(RAD_GameUnitAtFor(game, gegner, i, &unit))
        {
            TEST_ASSERT_TRUE((unit.id == nah) || (unit.id == fern));
            gegner_kennt++;
        }
    }
    TEST_ASSERT_EQUAL_INT(2, ich_kenne);
    TEST_ASSERT_EQUAL_INT(2, gegner_kennt);

    RAD_Unit_t unit;
    TEST_ASSERT_FALSE(RAD_GameUnitAtFor(game, ich, RAD_GameNumberOfUnits(game), &unit));
    TEST_ASSERT_FALSE(RAD_GameUnitAtFor(NULL, ich, 0, &unit));
    TEST_ASSERT_FALSE(RAD_GameUnitAtFor(game, ich, 0, NULL));

    abbauen();
}

///
/// Faehrt sie vom Rand der Sicht los, sehe ich nur ihr Startfeld -- mit dem
/// Randfeld sind es trotzdem zwei, und sie faehrt sichtbar in den Nebel, statt
/// einfach zu verschwinden.
///
void test_sicht_vom_rand_hinaus_faehrt_in_den_nebel(void)
{
    aufbauen();
    figur(ich, 2, 5, 1);
    const RAD_UnitId_t fremde = figur(gegner, 2, 4, 0);
    mitschreiben();

    ziehe_waagerecht(fremde, 2, 4, 4);

    TEST_ASSERT_EQUAL_INT(2, von_mir.anzahl);
    TEST_ASSERT_EQUAL_CHAR('m', von_mir.meldungen[0].art);
    TEST_ASSERT_EQUAL_INT(fremde, von_mir.meldungen[0].unit);
    TEST_ASSERT_EQUAL_INT(2, von_mir.meldungen[0].schritte);
    TEST_ASSERT_EQUAL_CHAR('l', von_mir.meldungen[1].art);
    TEST_ASSERT_EQUAL_INT(2, von_mir.meldungen[1].x);
    TEST_ASSERT_EQUAL_INT(4, von_mir.meldungen[1].y);

    abbauen();
}
