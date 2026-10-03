#include <unity.h>
#include <string.h>

#include <radish/server/control/game_start.h>

///
/// Die Spielstart-Datei (control/game_start.h): wer spielt, und mit welcher Armee
/// -- jede Einheit mit allen Werten.
///
/// Geladen wird meist aus einem String. Der letzte Test liest ueber
/// RAD_ControlLoadGameStart das gueltige Beispiel der Schema-Tests
/// (game/test/schema/spielstart/valid/) -- so passen Schema und Leser zusammen.
///

/// Bausteine nach dem Schema, jeder mit allen Pflichtschluesseln.
#define WAFFE(name, klasse) \
    "{\"name\":\"" name "\",\"klasse\":\"" klasse "\",\"schuesse\":1,\"staerke\":3," \
    "\"min_reichweite\":0,\"max_reichweite\":24,\"durchschlag\":2}"

#define MITGLIED(profil, waffen) \
    "{\"profil\":\"" profil "\",\"leben\":1,\"ruestung\":3,\"staerke\":3,\"genauigkeit\":4," \
    "\"waffen\":[" waffen "],\"ausruestung\":[]}"

#define EINHEIT(typ, mitglieder) \
    "{\"typ\":\"" typ "\",\"bewegung\":6,\"transportkapazitaet\":0,\"kann_ziele_einnehmen\":true," \
    "\"entitaeten\":[" mitglieder "]}"

#define EINHEIT_1 EINHEIT("Trupp", MITGLIED("Soldat", ""))
#define EINHEIT_2 EINHEIT("Trupp", MITGLIED("A", WAFFE("G", "heavy")) "," MITGLIED("B", ""))

#define SPIELER(name, kennung, einheiten) \
    "{\"name\":\"" name "\",\"kennung\":\"" kennung "\"," \
    "\"armee\":{\"name\":\"Armee " name "\",\"spezies\":\"Menschen\",\"einheiten\":[" einheiten "]}}"

#define ZWEI_SPIELER(erster) \
    "{\"spieldaten\":[" erster "," SPIELER("b", "bbbbbbbb", EINHEIT_1) "]}"

/// Ein Spielstart ist zu gross fuer den Stapel (game_start.h) -- einer fuer alle
/// Tests, frisch vor jedem.
static RAD_ControlGameStart_t *start;

static RAD_ControlGameStartResult_t lade(const char *json)
{
    RAD_ControlDestroyGameStart(&start);
    start = RAD_ControlCreateGameStart();
    TEST_ASSERT_NOT_NULL(start);
    return RAD_ControlParseGameStart(json, strlen(json), start);
}

void test_spielstart_liest_beide_spieler(void)
{
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_OK, lade(
        "{\"spieldaten\":["
        SPIELER("test-user", "aB3xK9pQ", EINHEIT_1 "," EINHEIT_2) ","
        SPIELER("test-user-2", "Zz1Yy2Xx", EINHEIT_1)
        "]}"));

    TEST_ASSERT_EQUAL_STRING("test-user", start->players[0].name);
    TEST_ASSERT_EQUAL_STRING("aB3xK9pQ", start->players[0].identifier);
    TEST_ASSERT_EQUAL_STRING("Armee test-user", start->players[0].army_name);
    TEST_ASSERT_EQUAL_INT(2, start->players[0].number_of_units);

    TEST_ASSERT_EQUAL_STRING("test-user-2", start->players[1].name);
    TEST_ASSERT_EQUAL_INT(1, start->players[1].number_of_units);

    RAD_ControlDestroyGameStart(&start);
}

///
/// Jeder Wert landet in seinem Feld -- und die Felder, die das Spiel vergibt,
/// stehen auf "nicht vergeben".
///
void test_spielstart_liest_die_werte_der_einheiten(void)
{
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_OK, lade(ZWEI_SPIELER(
        SPIELER("a", "aaaaaaaa",
            "{\"typ\":\"Transporter\",\"bewegung\":12,\"transportkapazitaet\":2,\"kann_ziele_einnehmen\":false,"
            "\"entitaeten\":["
              "{\"profil\":\"Anfuehrer\",\"leben\":2,\"ruestung\":3,\"staerke\":4,\"genauigkeit\":5,"
              "\"waffen\":["
                "{\"name\":\"Kanone\",\"klasse\":\"super_heavy\",\"schuesse\":7,\"staerke\":9,"
                "\"min_reichweite\":6,\"max_reichweite\":48,\"durchschlag\":3}"
              "],\"ausruestung\":[{\"name\":\"Medipack\"}]}"
            "]}"))));

    const RAD_Unit_t *unit = &start->players[0].units[0];
    TEST_ASSERT_EQUAL_STRING("Transporter", unit->name);
    TEST_ASSERT_EQUAL_INT(12, unit->movement);
    TEST_ASSERT_EQUAL_INT(2, unit->transport_capacity);
    TEST_ASSERT_FALSE(unit->can_capture);
    TEST_ASSERT_EQUAL_INT(1, unit->number_of_members);

    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, unit->id);
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, unit->owner);
    TEST_ASSERT_EQUAL_INT(-1, unit->x);
    TEST_ASSERT_EQUAL_INT(-1, unit->y);

    const RAD_UnitMember_t *member = &unit->members[0];
    TEST_ASSERT_EQUAL_STRING("Anfuehrer", member->profile);
    TEST_ASSERT_EQUAL_INT(2, member->health);
    TEST_ASSERT_EQUAL_INT(3, member->armor);
    TEST_ASSERT_EQUAL_INT(4, member->strength);
    TEST_ASSERT_EQUAL_INT(5, member->accuracy);
    TEST_ASSERT_EQUAL_INT(1, member->number_of_weapons);

    const RAD_Weapon_t *weapon = &member->weapons[0];
    TEST_ASSERT_EQUAL_STRING("Kanone", weapon->name);
    TEST_ASSERT_EQUAL_INT(RAD_WEAPON_CLASS_SUPER_HEAVY, weapon->weapon_class);
    TEST_ASSERT_EQUAL_INT(7, weapon->shots);
    TEST_ASSERT_EQUAL_INT(9, weapon->strength);
    TEST_ASSERT_EQUAL_INT(6, weapon->min_range);
    TEST_ASSERT_EQUAL_INT(48, weapon->max_range);
    TEST_ASSERT_EQUAL_INT(3, weapon->penetration);

    RAD_ControlDestroyGameStart(&start);
}

///
/// Unbekannte Schluessel werden uebergangen (control/game_start.h), auf jeder Ebene.
///
void test_spielstart_uebergeht_unbekannte_schluessel(void)
{
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_OK, lade(
        "{\"neu\":1,\"spieldaten\":["
        "{\"rang\":3,\"name\":\"a\",\"kennung\":\"aaaaaaaa\","
        " \"armee\":{\"farbe\":\"rot\",\"name\":\"x\",\"einheiten\":["
        "{\"rolle\":\"x\",\"typ\":\"Trupp\",\"bewegung\":6,\"transportkapazitaet\":0,\"kann_ziele_einnehmen\":true,"
        "\"entitaeten\":[{\"rang\":1,\"profil\":\"S\",\"leben\":1,\"ruestung\":1,\"staerke\":1,\"genauigkeit\":1,"
        "\"waffen\":[],\"ausruestung\":[{\"name\":\"M\",\"wirkung\":2}]}]}"
        "]}},"
        SPIELER("b", "bbbbbbbb", EINHEIT_1)
        "]}"));

    TEST_ASSERT_EQUAL_STRING("a", start->players[0].name);
    TEST_ASSERT_EQUAL_STRING("S", start->players[0].units[0].members[0].profile);

    RAD_ControlDestroyGameStart(&start);
}

void test_spielstart_lehnt_fehlerhafte_dateien_ab(void)
{
    static const char *const fehlerhaft[] = {
        // kein JSON
        "{\"spieldaten\":",
        // ohne Spieler
        "{}",
        // ein Spieler zu wenig, einer zu viel
        "{\"spieldaten\":[" SPIELER("a", "aaaaaaaa", EINHEIT_1) "]}",
        "{\"spieldaten\":[" SPIELER("a", "aaaaaaaa", EINHEIT_1) "," SPIELER("b", "bbbbbbbb", EINHEIT_1) ","
            SPIELER("c", "cccccccc", EINHEIT_1) "]}",
        // leerer Name
        ZWEI_SPIELER(SPIELER("", "aaaaaaaa", EINHEIT_1)),
        // Kennung zu kurz, Kennung mit Zeichen ausserhalb [A-Za-z0-9]
        ZWEI_SPIELER(SPIELER("a", "aaaa", EINHEIT_1)),
        ZWEI_SPIELER(SPIELER("a", "aaaa-aaa", EINHEIT_1)),
        // Armee ohne Einheiten
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", "")),
        // Einheit ohne Entitaeten
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("Trupp", ""))),
        // ohne Armee
        "{\"spieldaten\":[{\"name\":\"a\",\"kennung\":\"aaaaaaaa\"}," SPIELER("b", "bbbbbbbb", EINHEIT_1) "]}",
        // Name keine Zeichenkette
        "{\"spieldaten\":[{\"name\":1,\"kennung\":\"aaaaaaaa\",\"armee\":{}}," SPIELER("b", "bbbbbbbb", EINHEIT_1) "]}",
        // Einheit ohne Pflichtschluessel "bewegung"
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa",
            "{\"typ\":\"Trupp\",\"transportkapazitaet\":0,\"kann_ziele_einnehmen\":true,"
            "\"entitaeten\":[" MITGLIED("S", "") "]}")),
        // Mitglied ohne "ausruestung"
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("Trupp",
            "{\"profil\":\"S\",\"leben\":1,\"ruestung\":3,\"staerke\":3,\"genauigkeit\":4,\"waffen\":[]}"))),
        // leerer Einheitentyp
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("", MITGLIED("S", "")))),
        // Wert negativ, zu gross, keine ganze Zahl, ein String
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa",
            "{\"typ\":\"T\",\"bewegung\":-1,\"transportkapazitaet\":0,\"kann_ziele_einnehmen\":true,"
            "\"entitaeten\":[" MITGLIED("S", "") "]}")),
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa",
            "{\"typ\":\"T\",\"bewegung\":32768,\"transportkapazitaet\":0,\"kann_ziele_einnehmen\":true,"
            "\"entitaeten\":[" MITGLIED("S", "") "]}")),
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa",
            "{\"typ\":\"T\",\"bewegung\":1.5,\"transportkapazitaet\":0,\"kann_ziele_einnehmen\":true,"
            "\"entitaeten\":[" MITGLIED("S", "") "]}")),
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa",
            "{\"typ\":\"T\",\"bewegung\":\"6\",\"transportkapazitaet\":0,\"kann_ziele_einnehmen\":true,"
            "\"entitaeten\":[" MITGLIED("S", "") "]}")),
        // kein Wahrheitswert
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa",
            "{\"typ\":\"T\",\"bewegung\":6,\"transportkapazitaet\":0,\"kann_ziele_einnehmen\":1,"
            "\"entitaeten\":[" MITGLIED("S", "") "]}")),
        // unbekannte Waffenklasse
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("T", MITGLIED("S", WAFFE("G", "gigantisch"))))),
        // Ausruestung ohne Namen
        ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("T",
            "{\"profil\":\"S\",\"leben\":1,\"ruestung\":3,\"staerke\":3,\"genauigkeit\":4,"
            "\"waffen\":[],\"ausruestung\":[{}]}"))),
    };

    for(size_t i=0;i < sizeof(fehlerhaft) / sizeof(fehlerhaft[0]); ++i)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(i == 0 ? RAD_CONTROL_GAME_START_ERROR_SYNTAX : RAD_CONTROL_GAME_START_ERROR_SCHEMA,
                                      lade(fehlerhaft[i]), fehlerhaft[i]);
    }

    RAD_ControlDestroyGameStart(&start);
}

///
/// Was das Schema zulaesst, die festen Felder des Spiels aber nicht halten: eine
/// eigene Absage (RAD_CONTROL_GAME_START_ERROR_LIMIT), nichts wird gekuerzt.
///
void test_spielstart_lehnt_ab_was_das_spiel_nicht_halten_kann(void)
{
    // Ein Name mit 32 Zeichen -- einer mehr, als neben der Null Platz hat.
    #define NAME_32 "abcdefghijklmnopqrstuvwxyz012345"
    // Ein Name mit 31 Zeichen geht gerade noch.
    #define NAME_31 "abcdefghijklmnopqrstuvwxyz01234"

    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_OK,
        lade(ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT(NAME_31, MITGLIED(NAME_31, WAFFE(NAME_31, "standard")))))));

    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_ERROR_LIMIT,
        lade(ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT(NAME_32, MITGLIED("S", ""))))));
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_ERROR_LIMIT,
        lade(ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("T", MITGLIED(NAME_32, ""))))));
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_ERROR_LIMIT,
        lade(ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("T", MITGLIED("S", WAFFE(NAME_32, "standard")))))));

    // Fuenf Waffen, eine mehr als RAD_UNIT_MAX_WEAPONS.
    #define W WAFFE("G", "standard")
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_ERROR_LIMIT,
        lade(ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("T", MITGLIED("S", W "," W "," W "," W "," W))))));

    // Elf Mitglieder, eines mehr als RAD_UNIT_MAX_MEMBERS.
    #define M MITGLIED("S", "")
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_ERROR_LIMIT,
        lade(ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", EINHEIT("T", M "," M "," M "," M "," M "," M "," M "," M "," M "," M "," M)))));

    // 33 Einheiten, eine mehr als RAD_CONTROL_GAME_START_MAX_UNITS.
    #define E EINHEIT_1
    #define E4 E "," E "," E "," E
    #define E16 E4 "," E4 "," E4 "," E4
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_OK,
        lade(ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", E16 "," E16))));
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_MAX_UNITS, start->players[0].number_of_units);
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_ERROR_LIMIT,
        lade(ZWEI_SPIELER(SPIELER("a", "aaaaaaaa", E16 "," E16 "," E))));

    RAD_ControlDestroyGameStart(&start);
}

///
/// Ein misslungener Ladevorgang laesst den Stand des Aufrufers stehen.
///
void test_spielstart_fehler_laesst_stand_unberuehrt(void)
{
    RAD_ControlGameStart_t *vorher = RAD_ControlCreateGameStart();
    TEST_ASSERT_NOT_NULL(vorher);
    strcpy(vorher->players[0].name, "vorher");

    const char *json = "{\"spieldaten\":[" SPIELER("neu", "aaaaaaaa", EINHEIT_1) "]}";
    TEST_ASSERT_NOT_EQUAL_INT(RAD_CONTROL_GAME_START_OK, RAD_ControlParseGameStart(json, strlen(json), vorher));

    TEST_ASSERT_EQUAL_STRING("vorher", vorher->players[0].name);

    RAD_ControlDestroyGameStart(&vorher);
    TEST_ASSERT_NULL(vorher);
}

void test_spielstart_fehlende_datei(void)
{
    RAD_ControlGameStart_t *gelesen = RAD_ControlCreateGameStart();
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_ERROR_NOT_FOUND,
        RAD_ControlLoadGameStart("/gibt/es/nicht/spielstart.json", gelesen));
    RAD_ControlDestroyGameStart(&gelesen);
}

void test_spielstart_aus_datei(void)
{
    RAD_ControlGameStart_t *gelesen = RAD_ControlCreateGameStart();
    TEST_ASSERT_EQUAL_INT(RAD_CONTROL_GAME_START_OK,
        RAD_ControlLoadGameStart(RAD_SCHEMA_EXAMPLES_DIR "/spielstart/valid/zwei_spieler.json", gelesen));

    TEST_ASSERT_EQUAL_STRING("test-user", gelesen->players[0].name);
    TEST_ASSERT_EQUAL_STRING("Erste Kompanie", gelesen->players[0].army_name);
    TEST_ASSERT_EQUAL_INT(2, gelesen->players[0].number_of_units);
    TEST_ASSERT_EQUAL_STRING("Zz1Yy2Xx", gelesen->players[1].identifier);

    // Ein Blick in die Werte: der Trupp mit Anfuehrer und Kanone, der Transporter
    // ohne Waffen.
    const RAD_Unit_t *trupp = &gelesen->players[0].units[0];
    TEST_ASSERT_EQUAL_STRING("Trupp", trupp->name);
    TEST_ASSERT_EQUAL_INT(2, trupp->number_of_members);
    TEST_ASSERT_EQUAL_STRING("Kanone", trupp->members[0].weapons[1].name);
    TEST_ASSERT_EQUAL_INT(RAD_WEAPON_CLASS_HEAVY, trupp->members[0].weapons[1].weapon_class);

    const RAD_Unit_t *transporter = &gelesen->players[0].units[1];
    TEST_ASSERT_EQUAL_STRING("Transporter", transporter->name);
    TEST_ASSERT_EQUAL_INT(2, transporter->transport_capacity);
    TEST_ASSERT_EQUAL_INT(0, transporter->members[0].number_of_weapons);

    RAD_ControlDestroyGameStart(&gelesen);
}

///
/// Kennung und Id (game_start.h): gepackt, umkehrbar, und nie RAD_USER_NONE fuer
/// eine gueltige Kennung.
///
void test_spielstart_kennung_wird_zur_id(void)
{
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, RAD_ControlUserIdFromIdentifier("aB3xK9pQ"));
    TEST_ASSERT_EQUAL_UINT64(0x3030303030303030ull, RAD_ControlUserIdFromIdentifier("00000000"));

    char zurueck[RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH + 1];
    TEST_ASSERT_TRUE(RAD_ControlIdentifierFromUserId(RAD_ControlUserIdFromIdentifier("Zz1Yy2Xx"), zurueck));
    TEST_ASSERT_EQUAL_STRING("Zz1Yy2Xx", zurueck);

    // Keine Kennung: zu kurz, zu lang, falsche Zeichen, NULL.
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_ControlUserIdFromIdentifier("aaaa"));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_ControlUserIdFromIdentifier("aaaaaaaaa"));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_ControlUserIdFromIdentifier("aaaa aaa"));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_ControlUserIdFromIdentifier(NULL));

    // Und keine Id: 0x1, wie ihn der Client heute fest sendet, ist keine Kennung.
    TEST_ASSERT_FALSE(RAD_ControlIdentifierFromUserId((RAD_UserId_t)0x1, zurueck));
    TEST_ASSERT_EQUAL_STRING("", zurueck);
}
