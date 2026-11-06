#include <unity.h>
#include <string.h>

#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/control/visibility.h>

///
/// Die Sicht (control/visibility.h): welche Felder und Einheiten ein Spieler
/// sieht. Zwei Spieler, damit sich eigene und fremde Einheiten unterscheiden
/// lassen; die Welt hat ihre volle Groesse, solange ein Test sie nicht aendert.
///

static const RAD_UserId_t ich = (RAD_UserId_t)0x4711;
static const RAD_UserId_t gegner = (RAD_UserId_t)0x0815;

static RAD_EventManager_t *events;
static RAD_Game_t *game;

static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    game = RAD_CreateGame(events, ich);
    TEST_ASSERT_NOT_NULL(game);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, ich));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, gegner));
}

static void abbauen(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

/// Eine Einheit ohne Besitzer auf (x, y), die "sichtweite" Felder weit sieht.
static RAD_UnitId_t herrenlos(int16_t x, int16_t y, uint8_t sichtweite)
{
    const RAD_UnitId_t figur = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, x, y, NULL);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, figur);
    RAD_WorldUnitById(&game->world, figur)->sight_range = sichtweite;
    return figur;
}

/// Wie herrenlos, aber "besitzer" gehoert sie.
static RAD_UnitId_t figur(RAD_UserId_t besitzer, int16_t x, int16_t y, uint8_t sichtweite)
{
    const RAD_UnitId_t id = herrenlos(x, y, sichtweite);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameBindUnit(game, besitzer, id));
    return id;
}

/// Eine Einheit von "besitzer" in der Reserve, mit einem Mitglied -- ohne geht es
/// nicht hinein (RAD_WorldAddReserveUnit).
static RAD_UnitId_t in_reserve(RAD_UserId_t besitzer, uint8_t sichtweite)
{
    RAD_Unit_t werte;
    memset(&werte, 0, sizeof(werte));
    werte.type = RAD_UNIT_TYPE_PLAYER;
    werte.sight_range = sichtweite;
    werte.number_of_members = 1;
    const RAD_UnitId_t id = RAD_WorldAddReserveUnit(&game->world, &werte, besitzer);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, id);
    return id;
}

///
/// Macht das Feld (x, y) sichtblockierend (RAD_Terrain_t.blocks_sight), wie in
/// den Tests zum Angriff am Feld selbst.
///
static void versperre(int32_t x, int32_t y)
{
    RAD_WorldTileAt(&game->world, x, y)->terrain.blocks_sight = true;
}

///
/// Gezaehlt wird waagerecht plus senkrecht, wie bei der Reichweite einer Waffe:
/// genau sight_range Felder weit ist noch zu sehen, eins weiter nicht mehr.
///
void test_sicht_reicht_genau_bis_zur_sichtweite(void)
{
    aufbauen();
    const RAD_UnitId_t spaeher = figur(ich, 5, 5, 3);

    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, spaeher, 8, 5));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, 9, 5));
    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, spaeher, 6, 7));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, 7, 7));
    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, spaeher, 2, 5));

    TEST_ASSERT_TRUE(RAD_GameIsTileVisible(game, ich, 8, 5));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, 9, 5));

    abbauen();
}

void test_sichtweite_null_sieht_nur_das_eigene_feld(void)
{
    aufbauen();
    const RAD_UnitId_t blind = figur(ich, 5, 5, 0);

    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, blind, 5, 5));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, blind, 6, 5));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, blind, 5, 4));

    abbauen();
}

///
/// Dieselbe Sichtlinie wie beim Angriff: das sichtblockierende Feld selbst ist
/// zu sehen, was dahinter liegt nicht.
///
void test_gelaende_versperrt_was_dahinter_liegt(void)
{
    aufbauen();
    const RAD_UnitId_t spaeher = figur(ich, 2, 2, 5);
    versperre(4, 2);

    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, spaeher, 3, 2));
    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, spaeher, 4, 2));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, 5, 2));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, 5, 2));

    abbauen();
}

///
/// Eine Einheit dazwischen versperrt die Sicht wie Gelaende -- gleich wessen sie
/// ist. Sie selbst sieht man.
///
void test_einheit_versperrt_was_dahinter_liegt_und_wird_selbst_gesehen(void)
{
    aufbauen();
    const RAD_UnitId_t spaeher = figur(ich, 2, 2, 5);
    const RAD_UnitId_t vorne = figur(gegner, 4, 2, 0);
    const RAD_UnitId_t dahinter = figur(gegner, 6, 2, 0);

    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, spaeher, 4, 2));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, 5, 2));
    TEST_ASSERT_TRUE(RAD_GameIsUnitVisible(game, ich, vorne));
    TEST_ASSERT_FALSE(RAD_GameIsUnitVisible(game, ich, dahinter));

    // Auch eine eigene Einheit verstellt den Blick.
    const RAD_UnitId_t eigene = figur(ich, 2, 4, 0);
    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, spaeher, 2, 4));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, 2, 5));
    TEST_ASSERT_TRUE(RAD_GameIsUnitVisible(game, ich, eigene));

    abbauen();
}

///
/// Geht die Linie genau durch eine Ecke, ist sie dort nur versperrt, wenn beide
/// Felder daneben es sind.
///
void test_durch_eine_diagonale_luecke_sieht_man(void)
{
    aufbauen();
    const RAD_UnitId_t spaeher = figur(ich, 2, 2, 4);

    versperre(3, 2);
    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, spaeher, 4, 4));

    versperre(2, 3);
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, 4, 4));

    abbauen();
}

///
/// Sehen kann nur, was auf dem Feld steht: aus der Reserve und zerstoert sieht
/// eine Einheit nichts, auch nicht das Feld, auf dem sie zuletzt stand.
///
void test_reserve_und_zerstoerte_sehen_nichts(void)
{
    aufbauen();
    const RAD_UnitId_t wartend = in_reserve(ich, 5);
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, wartend, 0, 0));

    const RAD_UnitId_t gefallen = figur(ich, 3, 3, 2);
    TEST_ASSERT_TRUE(RAD_GameIsTileVisible(game, ich, 3, 3));
    RAD_WorldRemoveUnit(&game->world, gefallen, NULL);

    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, gefallen, 3, 3));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, 3, 3));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, 4, 3));

    abbauen();
}

///
/// Ein Spieler sieht, was irgendeine seiner Einheiten sieht -- und nur das: was
/// eine fremde sieht, zaehlt fuer ihn nicht.
///
void test_eigene_einheiten_ergaenzen_sich_fremde_zaehlen_nicht(void)
{
    aufbauen();
    figur(ich, 1, 1, 1);
    figur(ich, 10, 10, 1);
    figur(gegner, 5, 5, 3);

    TEST_ASSERT_TRUE(RAD_GameIsTileVisible(game, ich, 2, 1));
    TEST_ASSERT_TRUE(RAD_GameIsTileVisible(game, ich, 11, 10));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, 5, 5));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, 6, 5));

    TEST_ASSERT_TRUE(RAD_GameIsTileVisible(game, gegner, 6, 5));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, gegner, 2, 1));

    abbauen();
}

///
/// Eine Einheit ohne Besitzer sieht selbst, aber fuer niemanden -- und
/// RAD_USER_NONE sieht nichts.
///
void test_ohne_besitzer_sieht_niemand(void)
{
    aufbauen();
    const RAD_UnitId_t niemandes = herrenlos(3, 3, 5);

    TEST_ASSERT_TRUE(RAD_GameUnitSeesTile(game, niemandes, 4, 3));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, 4, 3));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, RAD_USER_NONE, 4, 3));
    TEST_ASSERT_FALSE(RAD_GameIsUnitVisible(game, RAD_USER_NONE, niemandes));

    abbauen();
}

///
/// Eine eigene Einheit ist immer zu sehen, gleich wo sie steht; eine fremde nur
/// auf dem Feld und nur, wenn ihr Feld zu sehen ist.
///
void test_eigene_einheit_immer_sichtbar_fremde_nur_in_sicht(void)
{
    aufbauen();
    figur(ich, 2, 2, 3);

    const RAD_UnitId_t eigene_reserve = in_reserve(ich, 1);
    const RAD_UnitId_t fremde_reserve = in_reserve(gegner, 1);
    const RAD_UnitId_t fremd_nah = figur(gegner, 4, 2, 0);
    const RAD_UnitId_t fremd_fern = figur(gegner, 12, 12, 0);

    TEST_ASSERT_TRUE(RAD_GameIsUnitVisible(game, ich, eigene_reserve));
    TEST_ASSERT_FALSE(RAD_GameIsUnitVisible(game, ich, fremde_reserve));
    TEST_ASSERT_TRUE(RAD_GameIsUnitVisible(game, ich, fremd_nah));
    TEST_ASSERT_FALSE(RAD_GameIsUnitVisible(game, ich, fremd_fern));

    // Zerstoert ist sie nicht mehr zu sehen, auch wenn ihr Feld es ist; eine
    // eigene zerstoerte bleibt bekannt.
    RAD_WorldRemoveUnit(&game->world, fremd_nah, NULL);
    TEST_ASSERT_TRUE(RAD_GameIsTileVisible(game, ich, 4, 2));
    TEST_ASSERT_FALSE(RAD_GameIsUnitVisible(game, ich, fremd_nah));

    RAD_WorldRemoveUnit(&game->world, eigene_reserve, NULL);
    TEST_ASSERT_TRUE(RAD_GameIsUnitVisible(game, ich, eigene_reserve));

    abbauen();
}

///
/// Das Raster sagt je Feld dasselbe wie die Einzelabfrage, traegt die Groesse der
/// Welt und ist dahinter leer.
///
void test_raster_stimmt_mit_der_einzelabfrage_ueberein(void)
{
    aufbauen();
    TEST_ASSERT_TRUE(RAD_ResetWorldToSize(&game->world, 10, 7));
    figur(ich, 1, 1, 4);
    figur(ich, 8, 5, 2);
    figur(gegner, 4, 3, 1);
    versperre(3, 1);
    versperre(6, 5);

    RAD_Visibility_t sicht;
    memset(&sicht, 0xff, sizeof(sicht));
    TEST_ASSERT_TRUE(RAD_GameVisibility(game, ich, &sicht));

    TEST_ASSERT_EQUAL_INT(10, sicht.width);
    TEST_ASSERT_EQUAL_INT(7, sicht.height);

    int32_t gesehen = 0;
    for(int32_t y = 0; y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int32_t x = 0; x < RAD_WORLD_WIDTH; ++x)
        {
            const bool in_der_welt = (x < sicht.width) && (y < sicht.height);
            const bool erwartet = in_der_welt && RAD_GameIsTileVisible(game, ich, x, y);
            TEST_ASSERT_EQUAL_MESSAGE(erwartet, sicht.tiles[y][x], "Raster und Einzelabfrage");
            gesehen += erwartet ? 1 : 0;
        }
    }
    // Damit der Vergleich nicht nur leere Raster vergleicht.
    TEST_ASSERT_GREATER_THAN_INT(0, gesehen);
    TEST_ASSERT_TRUE(sicht.tiles[1][1]);
    TEST_ASSERT_FALSE(sicht.tiles[1][4]);

    abbauen();
}

///
/// Was nicht zu beantworten ist, ist "sieht nicht": NULL, eine Stelle ausserhalb
/// der Welt, eine unbekannte Id. Das Raster bleibt dann unberuehrt.
///
void test_ungueltiges_sieht_nichts(void)
{
    aufbauen();
    const RAD_UnitId_t spaeher = figur(ich, 0, 0, 5);

    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(NULL, spaeher, 1, 0));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, -1, 0));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, 0, -1));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, spaeher, game->world.width, 0));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, RAD_UNIT_NONE, 0, 0));
    TEST_ASSERT_FALSE(RAD_GameUnitSeesTile(game, 42, 0, 0));

    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(NULL, ich, 0, 0));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, -1, 0));
    TEST_ASSERT_FALSE(RAD_GameIsTileVisible(game, ich, 0, game->world.height));

    TEST_ASSERT_FALSE(RAD_GameIsUnitVisible(NULL, ich, spaeher));
    TEST_ASSERT_FALSE(RAD_GameIsUnitVisible(game, ich, RAD_UNIT_NONE));
    TEST_ASSERT_FALSE(RAD_GameIsUnitVisible(game, ich, 42));

    RAD_Visibility_t sicht;
    memset(&sicht, 0, sizeof(sicht));
    sicht.width = -5;
    TEST_ASSERT_FALSE(RAD_GameVisibility(NULL, ich, &sicht));
    TEST_ASSERT_EQUAL_INT(-5, sicht.width);
    TEST_ASSERT_FALSE(RAD_GameVisibility(game, ich, NULL));

    abbauen();
}

///
/// Wer von einer Aenderung auf einem Feld erfaehrt: der Besitzer immer, sonst
/// wer das Feld sieht. Die Liste wird neu gefuellt, nicht ergaenzt.
///
void test_empfaenger_eines_feldes(void)
{
    aufbauen();
    figur(ich, 2, 2, 3);
    figur(gegner, 10, 10, 3);

    RAD_Recipients_t liste = { .number_of_users = 1, .users = { 0x99 }, .variant = true };

    // Ich sehe (4, 2), der Gegner nicht; Besitzer ist der Gegner.
    TEST_ASSERT_TRUE(RAD_GameRecipientsForTile(game, gegner, 4, 2, &liste));
    TEST_ASSERT_EQUAL_INT(2, liste.number_of_users);
    TEST_ASSERT_TRUE(RAD_RecipientsContains(&liste, gegner));
    TEST_ASSERT_TRUE(RAD_RecipientsContains(&liste, ich));
    TEST_ASSERT_FALSE(RAD_RecipientsContains(&liste, 0x99));
    TEST_ASSERT_FALSE(liste.variant);

    // Niemand sieht (6, 6): nur der Besitzer -- und ohne Besitzer niemand.
    TEST_ASSERT_TRUE(RAD_GameRecipientsForTile(game, ich, 6, 6, &liste));
    TEST_ASSERT_EQUAL_INT(1, liste.number_of_users);
    TEST_ASSERT_TRUE(RAD_RecipientsContains(&liste, ich));
    TEST_ASSERT_TRUE(RAD_GameRecipientsForTile(game, RAD_USER_NONE, 6, 6, &liste));
    TEST_ASSERT_EQUAL_INT(0, liste.number_of_users);

    // Der Besitzer, der das Feld auch sieht, steht nur einmal darin.
    TEST_ASSERT_TRUE(RAD_GameRecipientsForTile(game, ich, 3, 2, &liste));
    TEST_ASSERT_EQUAL_INT(1, liste.number_of_users);

    // Ausserhalb der Welt sieht niemand etwas.
    TEST_ASSERT_TRUE(RAD_GameRecipientsForTile(game, RAD_USER_NONE, -1, 0, &liste));
    TEST_ASSERT_EQUAL_INT(0, liste.number_of_users);

    TEST_ASSERT_FALSE(RAD_GameRecipientsForTile(NULL, ich, 0, 0, &liste));
    TEST_ASSERT_FALSE(RAD_GameRecipientsForTile(game, ich, 0, 0, NULL));

    abbauen();
}

///
/// Was ein Spieler von einem Weg sieht: die sichtbaren Felder in ihrer
/// Reihenfolge -- und Luecken, wo er nicht hinsieht.
///
void test_sichtbarer_teil_eines_weges(void)
{
    aufbauen();
    const RAD_UnitId_t spaeher = figur(ich, 5, 6, 2);

    const RAD_Path_t weg = {
        .steps_to = { { 2, 5 }, { 3, 5 }, { 4, 5 }, { 5, 5 }, { 6, 5 }, { 7, 5 }, { 8, 5 } },
        .number_of_steps = 7
    };

    RAD_Path_t gesehen;
    memset(&gesehen, 0xff, sizeof(gesehen));
    TEST_ASSERT_EQUAL_INT(3, RAD_GameVisiblePartOfPath(game, ich, &weg, &gesehen));
    TEST_ASSERT_EQUAL_INT(3, gesehen.number_of_steps);
    TEST_ASSERT_EQUAL_INT(4, gesehen.steps_to[0].x);
    TEST_ASSERT_EQUAL_INT(5, gesehen.steps_to[1].x);
    TEST_ASSERT_EQUAL_INT(6, gesehen.steps_to[2].x);
    TEST_ASSERT_EQUAL_INT(0, gesehen.steps_to[3].x);

    // Eine Luecke: zwei Einheiten sehen je ein Feld, das dazwischen keine.
    RAD_WorldRemoveUnit(&game->world, spaeher, NULL);
    figur(ich, 3, 6, 1);
    figur(ich, 7, 6, 1);
    TEST_ASSERT_EQUAL_INT(2, RAD_GameVisiblePartOfPath(game, ich, &weg, &gesehen));
    TEST_ASSERT_EQUAL_INT(3, gesehen.steps_to[0].x);
    TEST_ASSERT_EQUAL_INT(7, gesehen.steps_to[1].x);

    TEST_ASSERT_EQUAL_INT(0, RAD_GameVisiblePartOfPath(game, gegner, &weg, &gesehen));
    TEST_ASSERT_EQUAL_INT(0, gesehen.number_of_steps);
    TEST_ASSERT_EQUAL_INT(0, RAD_GameVisiblePartOfPath(NULL, ich, &weg, &gesehen));
    TEST_ASSERT_EQUAL_INT(0, RAD_GameVisiblePartOfPath(game, ich, NULL, &gesehen));
    TEST_ASSERT_EQUAL_INT(0, RAD_GameVisiblePartOfPath(game, ich, &weg, NULL));

    abbauen();
}

///
/// Was ein Beobachter von einem Weg bekommt: das Sichtbare mit je einem
/// Randfeld davor und danach -- Luecken dazwischen bleiben.
///
void test_beobachteter_teil_eines_weges(void)
{
    aufbauen();
    figur(ich, 3, 6, 1);
    figur(ich, 6, 6, 1);

    const RAD_Path_t weg = {
        .steps_to = { { 1, 5 }, { 2, 5 }, { 3, 5 }, { 4, 5 }, { 5, 5 }, { 6, 5 }, { 7, 5 }, { 8, 5 } },
        .number_of_steps = 8
    };

    // Ich sehe (3, 5) und (6, 5); Rand davor (2, 5), danach (7, 5).
    RAD_Path_t beobachtet;
    TEST_ASSERT_EQUAL_INT(4, RAD_GameObservedPartOfPath(game, ich, &weg, &beobachtet));
    TEST_ASSERT_EQUAL_INT(2, beobachtet.steps_to[0].x);
    TEST_ASSERT_EQUAL_INT(3, beobachtet.steps_to[1].x);
    TEST_ASSERT_EQUAL_INT(6, beobachtet.steps_to[2].x);
    TEST_ASSERT_EQUAL_INT(7, beobachtet.steps_to[3].x);

    // Sieht er schon das erste Feld oder noch das letzte, gibt es dort keinen Rand.
    const RAD_Path_t kurz = { .steps_to = { { 3, 5 }, { 4, 5 } }, .number_of_steps = 2 };
    TEST_ASSERT_EQUAL_INT(2, RAD_GameObservedPartOfPath(game, ich, &kurz, &beobachtet));
    TEST_ASSERT_EQUAL_INT(3, beobachtet.steps_to[0].x);
    TEST_ASSERT_EQUAL_INT(4, beobachtet.steps_to[1].x);
    const RAD_Path_t ganz_sichtbar = { .steps_to = { { 3, 5 }, { 3, 6 } }, .number_of_steps = 2 };
    TEST_ASSERT_EQUAL_INT(2, RAD_GameObservedPartOfPath(game, ich, &ganz_sichtbar, &beobachtet));

    // Sieht er nichts davon, gibt es auch keinen Rand.
    TEST_ASSERT_EQUAL_INT(0, RAD_GameObservedPartOfPath(game, gegner, &weg, &beobachtet));
    TEST_ASSERT_EQUAL_INT(0, beobachtet.number_of_steps);
    TEST_ASSERT_EQUAL_INT(0, RAD_GameObservedPartOfPath(NULL, ich, &weg, &beobachtet));
    TEST_ASSERT_EQUAL_INT(0, RAD_GameObservedPartOfPath(game, ich, NULL, &beobachtet));
    TEST_ASSERT_EQUAL_INT(0, RAD_GameObservedPartOfPath(game, ich, &weg, NULL));

    abbauen();
}
