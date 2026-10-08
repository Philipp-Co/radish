#include <unity.h>
#include <string.h>
#include <radish/game/model/world/world.h>

///
/// Der Lebenslauf einer Einheit in der Welt: Reserve, Feld, zerstoert
/// (RAD_WorldAddReserveUnit, RAD_WorldDeployUnit, RAD_WorldRemoveUnit).
///
/// Zwei Zusagen ziehen sich durch alle Tests: die Doppelbuchfuehrung zwischen
/// Einheit und Tile stimmt in jedem Zustand (RAD_WorldIsConsistent), und ein
/// Ereignis geht nur heraus, wenn sich auf dem Feld etwas geaendert hat.
///

typedef struct
{
    int32_t spawned;
    int32_t destroyed;
    int32_t moved;

    RAD_UnitId_t last_unit;
    int32_t last_x;
    int32_t last_y;
} RAD_UnitEreignisse_t;

static void zaehle_spawned(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    RAD_UnitEreignisse_t *gezaehlt = user_argument;
    gezaehlt->spawned++;
    gezaehlt->last_unit = unit->id;
    gezaehlt->last_x = x;
    gezaehlt->last_y = y;
}

static void zaehle_destroyed(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    RAD_UnitEreignisse_t *gezaehlt = user_argument;
    gezaehlt->destroyed++;
    gezaehlt->last_unit = unit->id;
    gezaehlt->last_x = x;
    gezaehlt->last_y = y;
}

static void zaehle_moved(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)unit;
    (void)path;
    (void)result;
    RAD_UnitEreignisse_t *gezaehlt = user_argument;
    gezaehlt->moved++;
}

static void abonniere_einheiten_ereignisse(RAD_EventManager_t *events, RAD_UnitEreignisse_t *gezaehlt)
{
    memset(gezaehlt, 0, sizeof(*gezaehlt));
    gezaehlt->last_unit = RAD_UNIT_NONE;

    RAD_EventManagerSubscribeToUnitEvents(events, (RAD_EventsUnitChangedCallback_t){
        .user_argument = gezaehlt,
        .spawned = zaehle_spawned,
        .destroyed = zaehle_destroyed,
        .moved = zaehle_moved
    });
}

///
/// Eine Einheit, wie sie aus einer Armee kommt: ein Trupp mit zwei Mitgliedern, das
/// erste mit einer Waffe. Die Felder, die die Welt selbst vergibt, sind absichtlich
/// mit Unsinn gefuellt -- RAD_WorldAddReserveUnit darf davon nichts uebernehmen.
///
static RAD_Unit_t trupp(void)
{
    RAD_Unit_t values;
    memset(&values, 0, sizeof(values));

    values.id = 42;
    values.state = RAD_UNIT_STATE_DESTROYED;
    values.owner = 0xBADull;
    values.x = 5;
    values.y = 5;
    values.conditions.burning = 1;

    values.type = RAD_UNIT_TYPE_PLAYER;
    strcpy(values.name, "Trupp");
    values.movement = 6;
    values.transport_capacity = 0;
    values.can_capture = true;

    values.number_of_members = 2;

    strcpy(values.members[0].profile, "Anfuehrer");
    values.members[0].health = 2;
    values.members[0].armor = 3;
    values.members[0].strength = 4;
    values.members[0].accuracy = 3;
    values.members[0].number_of_weapons = 1;
    strcpy(values.members[0].weapons[0].name, "Gewehr");
    values.members[0].weapons[0].weapon_class = RAD_WEAPON_CLASS_STANDARD;
    values.members[0].weapons[0].shots = 1;
    values.members[0].weapons[0].strength = 3;
    values.members[0].weapons[0].max_range = 24;

    strcpy(values.members[1].profile, "Soldat");
    values.members[1].health = 1;

    return values;
}

typedef struct
{
    RAD_EventManager_t *events;
    RAD_UnitPool_t *units;
    RAD_World_t world;
} RAD_WeltImTest_t;

static RAD_WeltImTest_t welt;

static void aufbauen(void)
{
    welt.events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(welt.events);

    welt.units = RAD_CreateUnitPool();
    TEST_ASSERT_NOT_NULL(welt.units);

    RAD_CreateWorld(&welt.world, welt.events, welt.units);
    RAD_InitWorld(&welt.world);
}

static void abbauen(void)
{
    RAD_DestroyUnitPool(&welt.units);
    RAD_DestroyEventManager(&welt.events);
}

void test_world_reserve_einheit_steht_auf_keinem_feld(void)
{
    aufbauen();

    RAD_UnitEreignisse_t gezaehlt;
    abonniere_einheiten_ereignisse(welt.events, &gezaehlt);

    const RAD_Unit_t values = trupp();
    const RAD_UnitId_t id = RAD_WorldAddReserveUnit(&welt.world, &values, 0x1234ull);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, id);

    const RAD_Unit_t *unit = RAD_WorldUnitById(&welt.world, id);
    TEST_ASSERT_NOT_NULL(unit);

    // Was die Welt vergibt.
    TEST_ASSERT_EQUAL_INT(id, unit->id);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, unit->state);
    TEST_ASSERT_EQUAL_UINT64(0x1234ull, unit->owner);
    TEST_ASSERT_EQUAL_INT(-1, unit->x);
    TEST_ASSERT_EQUAL_INT(-1, unit->y);
    TEST_ASSERT_EQUAL_INT(0, unit->conditions.burning);

    // Was aus der Armee kommt.
    TEST_ASSERT_EQUAL_STRING("Trupp", unit->name);
    TEST_ASSERT_EQUAL_INT(6, unit->movement);
    TEST_ASSERT_TRUE(unit->can_capture);
    TEST_ASSERT_EQUAL_INT(2, unit->number_of_members);
    TEST_ASSERT_EQUAL_STRING("Anfuehrer", unit->members[0].profile);
    TEST_ASSERT_EQUAL_INT(1, unit->members[0].number_of_weapons);
    TEST_ASSERT_EQUAL_STRING("Gewehr", unit->members[0].weapons[0].name);
    TEST_ASSERT_EQUAL_INT(24, unit->members[0].weapons[0].max_range);
    TEST_ASSERT_EQUAL_STRING("Soldat", unit->members[1].profile);

    // Dem Spiel bekannt, auf dem Feld nicht: kein Tile zeigt auf sie, und gemeldet
    // wurde nichts.
    TEST_ASSERT_EQUAL_INT(1, RAD_UnitPoolNumberOfUnits(welt.units));
    for(int32_t y=0;y < welt.world.height; ++y)
    {
        for(int32_t x=0;x < welt.world.width; ++x)
        {
            TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldTileAt(&welt.world, x, y)->unit);
        }
    }
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.spawned);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    abbauen();
}

void test_world_reserve_lehnt_unhaltbare_werte_ab(void)
{
    aufbauen();

    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldAddReserveUnit(&welt.world, NULL, RAD_USER_NONE));

    RAD_Unit_t ohne_mitglieder = trupp();
    ohne_mitglieder.number_of_members = 0;
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldAddReserveUnit(&welt.world, &ohne_mitglieder, RAD_USER_NONE));

    RAD_Unit_t zu_viele_mitglieder = trupp();
    zu_viele_mitglieder.number_of_members = RAD_UNIT_MAX_MEMBERS + 1;
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldAddReserveUnit(&welt.world, &zu_viele_mitglieder, RAD_USER_NONE));

    RAD_Unit_t zu_viele_waffen = trupp();
    zu_viele_waffen.members[1].number_of_weapons = RAD_UNIT_MAX_WEAPONS + 1;
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldAddReserveUnit(&welt.world, &zu_viele_waffen, RAD_USER_NONE));

    // Ein Name, der sein Feld bis zum letzten Byte fuellt, hat keinen Platz fuer
    // die Null -- er wird abgelehnt, nicht gekuerzt.
    RAD_Unit_t name_ohne_ende = trupp();
    memset(name_ohne_ende.members[0].weapons[0].name, 'X', RAD_UNIT_NAME_MAX);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldAddReserveUnit(&welt.world, &name_ohne_ende, RAD_USER_NONE));

    // Abgelehnt heisst: kein Slot belegt.
    TEST_ASSERT_EQUAL_INT(0, RAD_UnitPoolNumberOfUnits(welt.units));
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    abbauen();
}

void test_world_reserve_lehnt_ab_wenn_der_pool_voll_ist(void)
{
    aufbauen();

    // Mehr Einheiten, als die Welt Felder hat: die Reserve steht auf keinem, also
    // begrenzt nur der Pool.
    const RAD_Unit_t values = trupp();
    for(int32_t i=0;i < RAD_MAX_UNITS; ++i)
    {
        TEST_ASSERT_EQUAL_INT(i, RAD_WorldAddReserveUnit(&welt.world, &values, RAD_USER_NONE));
    }
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldAddReserveUnit(&welt.world, &values, RAD_USER_NONE));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldSpawnUnit(&welt.world, RAD_UNIT_TYPE_NPC, 0, 0));

    TEST_ASSERT_EQUAL_INT(RAD_MAX_UNITS, RAD_UnitPoolNumberOfUnits(welt.units));
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    abbauen();
}

void test_world_einheit_aus_der_reserve_aufstellen(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    const RAD_UnitId_t id = RAD_WorldAddReserveUnit(&welt.world, &values, 0x1234ull);

    RAD_UnitEreignisse_t gezaehlt;
    abonniere_einheiten_ereignisse(welt.events, &gezaehlt);

    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&welt.world, id, 3, 4));

    const RAD_Unit_t *unit = RAD_WorldUnitById(&welt.world, id);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, unit->state);
    TEST_ASSERT_EQUAL_INT(3, unit->x);
    TEST_ASSERT_EQUAL_INT(4, unit->y);
    TEST_ASSERT_EQUAL_INT(id, RAD_WorldTileAt(&welt.world, 3, 4)->unit);

    // Die Werte gehen beim Aufstellen nicht verloren.
    TEST_ASSERT_EQUAL_STRING("Trupp", unit->name);
    TEST_ASSERT_EQUAL_UINT64(0x1234ull, unit->owner);

    // Fuer den Abonnenten ein Spawn auf genau diesem Feld.
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.spawned);
    TEST_ASSERT_EQUAL_INT(id, gezaehlt.last_unit);
    TEST_ASSERT_EQUAL_INT(3, gezaehlt.last_x);
    TEST_ASSERT_EQUAL_INT(4, gezaehlt.last_y);

    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    abbauen();
}

void test_world_aufstellen_wird_abgelehnt(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    const RAD_UnitId_t id = RAD_WorldAddReserveUnit(&welt.world, &values, RAD_USER_NONE);
    const RAD_UnitId_t im_weg = RAD_WorldSpawnUnit(&welt.world, RAD_UNIT_TYPE_NPC, 1, 1);
    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&welt.world, 2, 2));

    RAD_UnitEreignisse_t gezaehlt;
    abonniere_einheiten_ereignisse(welt.events, &gezaehlt);

    // Das Feld taugt nicht: besetzt, ausserhalb, ohne Gelaende.
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, id, 1, 1));
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, id, -1, 0));
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, id, welt.world.width, 0));
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, id, 2, 2));

    // Die Einheit taugt nicht: es gibt sie nicht, oder sie steht nicht in der
    // Reserve -- schon auf dem Feld, oder zerstoert.
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, RAD_UNIT_NONE, 0, 0));
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, RAD_MAX_UNITS, 0, 0));
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, id + 10, 0, 0));
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, im_weg, 0, 0));

    const RAD_UnitId_t zerstoert = RAD_WorldAddReserveUnit(&welt.world, &values, RAD_USER_NONE);
    RAD_WorldRemoveUnit(&welt.world, zerstoert);
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, zerstoert, 0, 0));

    // Abgelehnt heisst: nichts geschrieben und nichts gemeldet.
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, RAD_WorldUnitById(&welt.world, id)->state);
    TEST_ASSERT_EQUAL_INT(im_weg, RAD_WorldTileAt(&welt.world, 1, 1)->unit);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldTileAt(&welt.world, 0, 0)->unit);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.spawned);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    // Und zweimal aufstellen geht nicht: beim zweiten Mal steht sie schon.
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&welt.world, id, 0, 0));
    TEST_ASSERT_FALSE(RAD_WorldDeployUnit(&welt.world, id, 3, 3));
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.spawned);

    abbauen();
}

void test_world_nur_einheiten_auf_dem_feld_ziehen(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    const RAD_UnitId_t id = RAD_WorldAddReserveUnit(&welt.world, &values, RAD_USER_NONE);

    RAD_UnitEreignisse_t gezaehlt;
    abonniere_einheiten_ereignisse(welt.events, &gezaehlt);

    // Aus der Reserve laesst sich nicht ziehen -- es gibt kein Feld, von dem aus.
    TEST_ASSERT_FALSE(RAD_WorldMoveUnit(&welt.world, id, 1, 0));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldTileAt(&welt.world, 1, 0)->unit);
    TEST_ASSERT_EQUAL_INT(0, gezaehlt.moved);

    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&welt.world, id, 0, 0));
    TEST_ASSERT_TRUE(RAD_WorldMoveUnit(&welt.world, id, 1, 0));
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.moved);

    // Und aus dem Grab auch nicht.
    RAD_WorldRemoveUnit(&welt.world, id);
    TEST_ASSERT_FALSE(RAD_WorldMoveUnit(&welt.world, id, 2, 0));
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.moved);

    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    abbauen();
}

void test_world_entfernen_zerstoert_und_vergibt_die_id_nicht_neu(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    const RAD_UnitId_t auf_dem_feld = RAD_WorldAddReserveUnit(&welt.world, &values, 0x1234ull);
    const RAD_UnitId_t in_reserve = RAD_WorldAddReserveUnit(&welt.world, &values, 0x1234ull);
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&welt.world, auf_dem_feld, 2, 3));

    RAD_UnitEreignisse_t gezaehlt;
    abonniere_einheiten_ereignisse(welt.events, &gezaehlt);

    // Vom Feld: das Feld wird frei, gemeldet wird mit dem Feld, auf dem sie stand.
    RAD_WorldRemoveUnit(&welt.world, auf_dem_feld);

    const RAD_Unit_t *unit = RAD_WorldUnitById(&welt.world, auf_dem_feld);
    TEST_ASSERT_NOT_NULL(unit);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DESTROYED, unit->state);
    TEST_ASSERT_EQUAL_INT(-1, unit->x);
    TEST_ASSERT_EQUAL_INT(-1, unit->y);
    TEST_ASSERT_EQUAL_UINT64(0x1234ull, unit->owner);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldTileAt(&welt.world, 2, 3)->unit);

    TEST_ASSERT_EQUAL_INT(1, gezaehlt.destroyed);
    TEST_ASSERT_EQUAL_INT(auf_dem_feld, gezaehlt.last_unit);
    TEST_ASSERT_EQUAL_INT(2, gezaehlt.last_x);
    TEST_ASSERT_EQUAL_INT(3, gezaehlt.last_y);

    // Noch einmal entfernen tut nichts.
    RAD_WorldRemoveUnit(&welt.world, auf_dem_feld);
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.destroyed);

    // Aus der Reserve: zerstoert, aber still -- auf dem Feld war sie nie.
    RAD_WorldRemoveUnit(&welt.world, in_reserve);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DESTROYED, RAD_WorldUnitById(&welt.world, in_reserve)->state);
    TEST_ASSERT_EQUAL_INT(1, gezaehlt.destroyed);

    // Beide Slots bleiben belegt; die naechste Einheit bekommt eine neue Id.
    TEST_ASSERT_EQUAL_INT(2, RAD_UnitPoolNumberOfUnits(welt.units));
    const RAD_UnitId_t neu = RAD_WorldSpawnUnit(&welt.world, RAD_UNIT_TYPE_NPC, 2, 3);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, neu);
    TEST_ASSERT_NOT_EQUAL(auf_dem_feld, neu);
    TEST_ASSERT_NOT_EQUAL(in_reserve, neu);

    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    abbauen();
}

void test_world_konsistenz_kennt_die_zustaende(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    const RAD_UnitId_t reserve = RAD_WorldAddReserveUnit(&welt.world, &values, RAD_USER_NONE);
    const RAD_UnitId_t feld = RAD_WorldSpawnUnit(&welt.world, RAD_UNIT_TYPE_NPC, 1, 1);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    // Eine Einheit in der Reserve mit einer Position.
    RAD_Unit_t *unit = RAD_WorldUnitById(&welt.world, reserve);
    unit->x = 0;
    unit->y = 0;
    TEST_ASSERT_FALSE(RAD_WorldIsConsistent(&welt.world));
    unit->x = -1;
    unit->y = -1;
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    // Ein Tile, das auf eine Einheit zeigt, die nicht auf dem Feld steht.
    RAD_Unit_t *auf_dem_feld = RAD_WorldUnitById(&welt.world, feld);
    auf_dem_feld->state = RAD_UNIT_STATE_RESERVE;
    TEST_ASSERT_FALSE(RAD_WorldIsConsistent(&welt.world));
    auf_dem_feld->state = RAD_UNIT_STATE_DEPLOYED;
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&welt.world));

    // Mehr Mitglieder, als Platz ist.
    unit->number_of_members = RAD_UNIT_MAX_MEMBERS + 1;
    TEST_ASSERT_FALSE(RAD_WorldIsConsistent(&welt.world));

    abbauen();
}
