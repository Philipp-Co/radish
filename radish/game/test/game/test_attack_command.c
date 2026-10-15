#include <unity.h>
#include <string.h>
#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand: er liest an der
// Einheit nach, was der Angriff hinterlassen hat. Die Begruendung fuer diesen
// Suchpfad steht in test/CMakeLists.txt.
#include <radish/game/model/game.h>
#include <radish/game/model/world/world.h>

///
/// Der Angriff auf ein Feld (RAD_COMMAND_TYPE_ATTACK): die Fabrik, die Regel
/// (RAD_GameCheckAttack), was er an der Einheit hinterlaesst (RAD_Unit_t.turn),
/// und der Kampf selbst (RAD_AttackResult_t).
///
/// **Die Kampftests kennen die Wuerfelfolge nicht und brauchen sie nicht.** Ein
/// W10 zeigt 1 bis 10: gegen 1 gelingt jeder Wurf, gegen 11 keiner. So laesst
/// sich jede Stufe eines Schusses erzwingen oder verhindern.
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
    // Ein Feld weit ziehen koennen, wie eine aus einer Armee.
    RAD_WorldUnitById(&game->world, figur)->movement = 1;
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

///
/// Mitschrift des attacked-Ereignisses (event_manager.h).
///
typedef struct
{
    int32_t attacked;
    int32_t destroyed;
    RAD_AttackResult_t result;
} RAD_AngriffMitschrift_t;

static RAD_AngriffMitschrift_t angriffe;

static void notiere_angriff(void *user_argument, const RAD_AttackResult_t *result)
{
    RAD_AngriffMitschrift_t *m = user_argument;
    m->attacked++;
    m->result = *result;
}

static void notiere_zerstoerung(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    RAD_AngriffMitschrift_t *m = user_argument;
    (void)unit; (void)x; (void)y;
    m->destroyed++;
}

static void ohne_belang(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument; (void)unit; (void)x; (void)y;
}

static void ohne_zug(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)user_argument; (void)unit; (void)path; (void)result;
}

/// Meldet die Ereignisse der Einheiten in "angriffe", vorher geleert.
static void mitschreiben(void)
{
    memset(&angriffe, 0, sizeof(angriffe));
    RAD_EventManagerSubscribeToUnitEvents(events, (RAD_EventsUnitChangedCallback_t){
        .user_argument = &angriffe,
        .spawned = ohne_belang,
        .destroyed = notiere_zerstoerung,
        .moved = ohne_zug,
        .attacked = notiere_angriff
    });
}

///
/// **Ein angenommener Angriff wird gemeldet** -- einmal, mit Einheit und Ziel,
/// und erst, nachdem er gezaehlt ist. Ein abgelehnter wird nicht gemeldet.
///
void test_attack_meldet_den_angriff(void)
{
    aufbauen();

    const RAD_UnitId_t figur = bewaffnete_figur(2, 2, 3);

    mitschreiben();

    // Ausser Reichweite: abgelehnt, nichts gemeldet.
    greife_an(figur, 7, 7);
    TEST_ASSERT_EQUAL_INT(0, angriffe.attacked);

    greife_an(figur, 4, 2);
    TEST_ASSERT_EQUAL_INT(1, angriffe.attacked);
    TEST_ASSERT_EQUAL_INT(figur, angriffe.result.attacker);
    TEST_ASSERT_EQUAL_INT(4, angriffe.result.x);
    TEST_ASSERT_EQUAL_INT(2, angriffe.result.y);

    // Ein zweiter im selben Zug ist abgelehnt.
    greife_an(figur, 3, 2);
    TEST_ASSERT_EQUAL_INT(1, angriffe.attacked);

    abbauen();
}

///
/// Ein Schuetze auf (x, y): ein Mitglied mit Genauigkeit "accuracy" und einer
/// Waffe, die bis 5 Felder reicht, mit "shots" Schuessen, Staerke "strength"
/// und Durchschlag "penetration".
///
static RAD_UnitId_t schuetze(int16_t x, int16_t y, int16_t accuracy, int16_t shots, int16_t strength, int16_t penetration)
{
    const RAD_UnitId_t figur = bewaffnete_figur(x, y, 5);
    RAD_UnitMember_t *member = &RAD_WorldUnitById(&game->world, figur)->members[0];
    member->accuracy = accuracy;
    member->weapons[0].shots = shots;
    member->weapons[0].strength = strength;
    member->weapons[0].penetration = penetration;
    return figur;
}

///
/// Ein Ziel auf (x, y) mit "members" Mitgliedern, jedes mit "health"
/// Lebenspunkten, Ruestung "armor" und Widerstand "toughness". Die Profile
/// heissen "A", "B", "C" ... in ihrer Reihenfolge.
///
static RAD_UnitId_t ziel(int16_t x, int16_t y, int32_t members, int16_t health, int16_t armor, int16_t toughness)
{
    const RAD_UnitId_t figur = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_PLAYER, x, y);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, figur);
    RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, figur);
    unit->number_of_members = members;
    for(int32_t m = 0; m < members; ++m)
    {
        unit->members[m].profile[0] = (char)('A' + m);
        unit->members[m].health = health;
        unit->members[m].health_max = health;
        unit->members[m].armor = armor;
        unit->members[m].toughness = toughness;
    }
    return figur;
}

static const RAD_Unit_t* einheit(RAD_UnitId_t figur)
{
    return RAD_WorldUnitById(&game->world, figur);
}

/// Stufe 1 scheitert immer: kein Treffer, keine weitere Stufe, kein Schaden.
void test_kampf_verfehlt(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 11, 5, 10, 10);
    const RAD_UnitId_t getroffene = ziel(4, 2, 1, 3, 0, 0);

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(angreifende, angriffe.result.attacker);
    TEST_ASSERT_EQUAL_INT(getroffene, angriffe.result.target);
    TEST_ASSERT_EQUAL_INT(5, angriffe.result.shots);
    TEST_ASSERT_EQUAL_INT(0, angriffe.result.hits);
    TEST_ASSERT_EQUAL_INT(3, einheit(getroffene)->members[0].health);

    abbauen();
}

/// Stufe 2 scheitert immer: getroffen, aber nicht durch die Ruestung.
void test_kampf_prallt_an_der_ruestung_ab(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 1, 5, 10, 0);
    const RAD_UnitId_t getroffene = ziel(4, 2, 1, 3, 11, 0);

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(5, angriffe.result.hits);
    TEST_ASSERT_EQUAL_INT(0, angriffe.result.penetrations);
    TEST_ASSERT_EQUAL_INT(3, einheit(getroffene)->members[0].health);

    abbauen();
}

/// Durchschlag zaehlt zum Wurf: gegen Ruestung 11 kommt Durchschlag 10 immer durch.
void test_kampf_durchschlag_hilft_gegen_ruestung(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 1, 5, 0, 10);
    ziel(4, 2, 1, 3, 11, 11);

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(5, angriffe.result.penetrations);
    TEST_ASSERT_EQUAL_INT(0, angriffe.result.wounds);

    abbauen();
}

/// Stufe 3 scheitert immer: durch die Ruestung, aber ohne Wirkung.
void test_kampf_ohne_wirkung_gegen_den_widerstand(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 1, 5, 0, 0);
    const RAD_UnitId_t getroffene = ziel(4, 2, 1, 3, 0, 11);

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(5, angriffe.result.penetrations);
    TEST_ASSERT_EQUAL_INT(0, angriffe.result.wounds);
    TEST_ASSERT_EQUAL_INT(3, einheit(getroffene)->members[0].health);

    abbauen();
}

/// Alles gelingt: jeder Schuss kostet einen Lebenspunkt, der Hoechstwert bleibt.
void test_kampf_jeder_wirkende_schuss_kostet_einen_lebenspunkt(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 1, 3, 0, 0);
    const RAD_UnitId_t getroffene = ziel(4, 2, 1, 5, 0, 0);

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(3, angriffe.result.shots);
    TEST_ASSERT_EQUAL_INT(3, angriffe.result.hits);
    TEST_ASSERT_EQUAL_INT(3, angriffe.result.penetrations);
    TEST_ASSERT_EQUAL_INT(3, angriffe.result.wounds);
    TEST_ASSERT_EQUAL_INT(0, angriffe.result.members_killed);
    TEST_ASSERT_FALSE(angriffe.result.target_destroyed);
    TEST_ASSERT_EQUAL_INT(2, einheit(getroffene)->members[0].health);
    TEST_ASSERT_EQUAL_INT(5, einheit(getroffene)->members[0].health_max);

    abbauen();
}

///
/// Ein Mitglied ohne Lebenspunkte ist weg; die anderen ruecken auf, in ihrer
/// Reihenfolge. Welches getroffen wird, entscheidet der Wurf -- geprueft wird
/// deshalb, was davon unabhaengig gilt.
///
void test_kampf_ein_mitglied_faellt_und_die_anderen_ruecken_auf(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 1, 1, 0, 0);
    const RAD_UnitId_t getroffene = ziel(4, 2, 3, 1, 0, 0);

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(1, angriffe.result.members_killed);
    TEST_ASSERT_FALSE(angriffe.result.target_destroyed);

    const RAD_Unit_t *unit = einheit(getroffene);
    TEST_ASSERT_EQUAL_INT(2, unit->number_of_members);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, unit->state);
    TEST_ASSERT_TRUE(unit->members[0].profile[0] < unit->members[1].profile[0]);
    TEST_ASSERT_EQUAL_INT(1, unit->members[0].health);
    TEST_ASSERT_EQUAL_INT(1, unit->members[1].health);
    TEST_ASSERT_EQUAL_INT(0, unit->members[2].profile[0]);

    abbauen();
}

///
/// Das letzte Mitglied faellt: die Einheit ist zerstoert und vom Feld, und der
/// Angriff ist zu Ende -- die uebrigen neun Schuesse werden nicht abgegeben.
///
void test_kampf_die_einheit_faellt_und_der_angriff_endet(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 1, 10, 0, 0);
    const RAD_UnitId_t getroffene = ziel(4, 2, 1, 1, 0, 0);

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(1, angriffe.result.shots);
    TEST_ASSERT_EQUAL_INT(1, angriffe.result.members_killed);
    TEST_ASSERT_TRUE(angriffe.result.target_destroyed);
    TEST_ASSERT_EQUAL_INT(getroffene, angriffe.result.target);
    TEST_ASSERT_EQUAL_INT(1, angriffe.destroyed);

    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DESTROYED, einheit(getroffene)->state);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldTileAt(&game->world, 4, 2)->unit);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    abbauen();
}

/// Faellt die letzte Einheit eines Spielers, hat er verloren.
void test_kampf_die_letzte_einheit_faellt_und_das_spiel_endet(void)
{
    aufbauen();
    const RAD_UserId_t verteidiger = (RAD_UserId_t)0x0815;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, verteidiger));

    RAD_Unit_t values = { .number_of_members = 1 };
    RAD_UnitId_t angreifende = RAD_UNIT_NONE;
    RAD_UnitId_t getroffene = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, angreifer, &values, &angreifende));
    values.members[0].health = 1;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, verteidiger, &values, &getroffene));
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&game->world, angreifende, 2, 2));
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&game->world, getroffene, 4, 2));

    bewaffne(angreifende, 0, 5);
    RAD_UnitMember_t *member = &RAD_WorldUnitById(&game->world, angreifende)->members[0];
    member->accuracy = 1;
    member->weapons[0].shots = 1;

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_TRUE(RAD_GameIsFinished(game));
    TEST_ASSERT_EQUAL_UINT64(angreifer, RAD_GameWinner(game));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_END_ELIMINATED, RAD_GameEndReason(game));

    abbauen();
}

///
/// Es feuert jede Waffe jedes Mitglieds, die das Feld erreicht -- mit allen
/// ihren Schuessen und der Genauigkeit ihres Traegers. Eine Waffe ausser
/// Reichweite schweigt.
///
void test_kampf_jede_waffe_in_reichweite_feuert(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 11, 2, 0, 0);
    ziel(4, 2, 1, 100, 11, 11);

    RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, angreifende);
    // Das erste Mitglied traegt dazu eine Waffe, die nur das Nachbarfeld erreicht.
    unit->members[0].number_of_weapons = 2;
    unit->members[0].weapons[1].max_range = 1;
    unit->members[0].weapons[1].shots = 7;
    // Ein zweites Mitglied, das immer trifft, mit drei Schuessen.
    unit->number_of_members = 2;
    unit->members[1] = unit->members[0];
    unit->members[1].accuracy = 1;
    unit->members[1].number_of_weapons = 1;
    unit->members[1].weapons[0].shots = 3;

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(5, angriffe.result.shots);
    TEST_ASSERT_EQUAL_INT(3, angriffe.result.hits);

    abbauen();
}

/// Ein leeres Feld: der Angriff zaehlt, aber es wird nicht gewuerfelt.
void test_kampf_auf_ein_leeres_feld_wird_nicht_gewuerfelt(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t angreifende = schuetze(2, 2, 1, 5, 0, 0);
    const uint64_t vorher = game->dice.state;

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_TRUE(hat_angegriffen(angreifende));
    TEST_ASSERT_EQUAL_INT(1, angriffe.attacked);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, angriffe.result.target);
    TEST_ASSERT_EQUAL_INT(0, angriffe.result.shots);
    TEST_ASSERT_EQUAL_UINT64(vorher, game->dice.state);

    abbauen();
}

/// Eine eigene Einheit wird getroffen wie jede andere.
void test_kampf_trifft_auch_eigene_einheiten(void)
{
    aufbauen();
    mitschreiben();

    RAD_Unit_t values = { .number_of_members = 1 };
    RAD_UnitId_t angreifende = RAD_UNIT_NONE;
    RAD_UnitId_t eigene = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, angreifer, &values, &angreifende));
    values.members[0].health = 4;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, angreifer, &values, &eigene));
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&game->world, angreifende, 2, 2));
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&game->world, eigene, 4, 2));

    bewaffne(angreifende, 0, 5);
    RAD_UnitMember_t *member = &RAD_WorldUnitById(&game->world, angreifende)->members[0];
    member->accuracy = 1;
    member->weapons[0].shots = 1;

    greife_an(angreifende, 4, 2);

    TEST_ASSERT_EQUAL_INT(eigene, angriffe.result.target);
    TEST_ASSERT_EQUAL_INT(1, angriffe.result.wounds);
    TEST_ASSERT_EQUAL_INT(3, einheit(eigene)->members[0].health);

    abbauen();
}

/// Zum selben Seed derselbe Kampf: der Wuerfel ist der einzige Zufall.
void test_kampf_derselbe_seed_derselbe_kampf(void)
{
    RAD_AttackResult_t ergebnisse[2];
    for(int32_t lauf = 0; lauf < 2; ++lauf)
    {
        aufbauen();
        mitschreiben();
        RAD_DiceSeed(&game->dice, 42);
        const RAD_UnitId_t angreifende = schuetze(2, 2, 5, 20, 3, 2);
        ziel(4, 2, 5, 3, 5, 5);

        greife_an(angreifende, 4, 2);
        ergebnisse[lauf] = angriffe.result;
        abbauen();
    }

    TEST_ASSERT_EQUAL_INT(ergebnisse[0].hits, ergebnisse[1].hits);
    TEST_ASSERT_EQUAL_INT(ergebnisse[0].penetrations, ergebnisse[1].penetrations);
    TEST_ASSERT_EQUAL_INT(ergebnisse[0].wounds, ergebnisse[1].wounds);
    TEST_ASSERT_EQUAL_INT(ergebnisse[0].members_killed, ergebnisse[1].members_killed);
    // Bei 20 Schuessen mit 60 % Trefferchance faellt nicht alles auf eine Stufe.
    TEST_ASSERT_TRUE(ergebnisse[0].hits > 0);
    TEST_ASSERT_TRUE(ergebnisse[0].hits < 20);
}

///
/// **Deckung erschwert den Treffer** (RAD_Terrain_t.cover): gezaehlt wird die des
/// Zielfeldes, nicht die des Schuetzen. Genauigkeit 1 trifft offen immer; mit
/// Deckung 10 braucht es eine 11, und die wuerfelt kein W10.
///
void test_kampf_deckung_des_zielfeldes_erschwert_den_treffer(void)
{
    aufbauen();
    mitschreiben();
    const RAD_UnitId_t offen = schuetze(2, 2, 1, 5, 0, 0);
    ziel(4, 2, 1, 3, 11, 0);

    // Deckung unter dem Schuetzen zaehlt nicht.
    RAD_WorldTileAt(&game->world, 2, 2)->terrain.cover = 10;
    greife_an(offen, 4, 2);
    TEST_ASSERT_EQUAL_INT(5, angriffe.result.hits);

    const RAD_UnitId_t gedeckt = schuetze(2, 4, 1, 5, 0, 0);
    ziel(4, 4, 1, 3, 11, 0);
    RAD_WorldTileAt(&game->world, 4, 4)->terrain.cover = 10;
    greife_an(gedeckt, 4, 4);
    TEST_ASSERT_EQUAL_INT(5, angriffe.result.shots);
    TEST_ASSERT_EQUAL_INT(0, angriffe.result.hits);

    abbauen();
}
