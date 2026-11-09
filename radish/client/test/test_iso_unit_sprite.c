#include <unity.h>
#include <stdio.h>
#include <string.h>

#include <radish/rendering/iso_unit_sprite.h>

///
/// Der Leser der sprite.json (rendering/iso_unit_sprite.h). Gelesen wird aus
/// denselben Dateien, die check-jsonschema prueft (client/test/CMakeLists.txt):
/// was das Schema annimmt, muss der Leser annehmen, was es abweist, auch er. Dazu
/// kommt, was nur der Leser prueft, und die Grenzen seiner festen Puffer.
///

///
/// Eine Datei in einen festen Puffer -- etwas groesser als
/// RAD_ISO_UNIT_SPRITE_JSON_MAX, damit "zu gross" auch ankommt.
///
static char datei[2 * RAD_ISO_UNIT_SPRITE_JSON_MAX];

static size_t lies(const char *pfad)
{
    FILE *file = fopen(pfad, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, pfad);
    const size_t length = fread(datei, 1, sizeof(datei), file);
    fclose(file);
    return length;
}

static RAD_IsoUnitSpriteResult_t lade_datei(const char *pfad, RAD_IsoUnitSpriteInfo_t *info)
{
    const size_t length = lies(pfad);
    return RAD_IsoUnitSpriteParse(datei, length, info);
}

static RAD_IsoUnitSpriteResult_t lade(const char *json, RAD_IsoUnitSpriteInfo_t *info)
{
    return RAD_IsoUnitSpriteParse(json, strlen(json), info);
}

void test_sprite_liest_den_panzer(void)
{
    RAD_IsoUnitSpriteInfo_t info;
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_OK, lade_datei(RAD_ASSETS_DIR "/units/heavy_tank/sprite.json", &info));

    const RAD_IsoUnitAtlasInfo_t *turn = &info.atlases[RAD_ISO_UNIT_ATLAS_TURN];
    TEST_ASSERT_EQUAL_STRING("turn.png", turn->file);
    TEST_ASSERT_EQUAL_INT(128, turn->frame_width);
    TEST_ASSERT_EQUAL_INT(128, turn->frame_height);
    TEST_ASSERT_EQUAL_INT(396, turn->frame_count);
    const int32_t turn_keys[RAD_ISO_FACINGS] = { 0, 100, 198, 298 };
    TEST_ASSERT_EQUAL_INT32_ARRAY(turn_keys, turn->key_frames, RAD_ISO_FACINGS);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, turn->fps);

    // se ist Y_PLUS, beim Panzer der Turm bei 90 Grad.
    const RAD_IsoUnitAtlasInfo_t *aim_se =
        &info.atlases[RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_AIM, RAD_ISO_FACING_Y_PLUS)];
    TEST_ASSERT_EQUAL_STRING("turret_rotation_90.png", aim_se->file);
    TEST_ASSERT_EQUAL_INT(196, aim_se->frame_count);
    TEST_ASSERT_EQUAL_INT(147, aim_se->key_frames[3]);

    const RAD_IsoUnitAtlasInfo_t *destroyed_nw =
        &info.atlases[RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_DESTROYED, RAD_ISO_FACING_Y_MINUS)];
    TEST_ASSERT_EQUAL_STRING("still_nw.png", destroyed_nw->file);
    TEST_ASSERT_EQUAL_INT(1, destroyed_nw->frame_count);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, destroyed_nw->fps);

    TEST_ASSERT_EQUAL_FLOAT(0.5f, info.anchor_x);
    TEST_ASSERT_EQUAL_FLOAT(0.523f, info.anchor_y);
    TEST_ASSERT_EQUAL_INT(256, info.draw_width);
    TEST_ASSERT_TRUE(info.own_shadow);
    TEST_ASSERT_EQUAL_FLOAT(750.0f, info.step_ms);
    TEST_ASSERT_EQUAL_FLOAT(1000.0f, info.turn_ms);
    TEST_ASSERT_EQUAL_FLOAT(500.0f, info.aim_ms);
    TEST_ASSERT_EQUAL_FLOAT(500.0f, info.aim_hold_ms);
}

///
/// Jedes gueltige Beispiel. Ohne "timing_ms" und "own_shadow" sind beide 0 --
/// dann gilt der Standard (iso_unit_images.h).
///
void test_sprite_nimmt_die_gueltigen_beispiele_an(void)
{
    RAD_IsoUnitSpriteInfo_t info;
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_OK,
                          lade_datei(RAD_SPRITE_EXAMPLES_DIR "/valid/an_den_grenzen.json", &info));
    TEST_ASSERT_EQUAL_INT(1024, info.atlases[RAD_ISO_UNIT_ATLAS_TURN].frame_count);
    TEST_ASSERT_EQUAL_INT(1023, info.atlases[RAD_ISO_UNIT_ATLAS_TURN].key_frames[3]);
    TEST_ASSERT_EQUAL_INT(1024, info.draw_width);
    TEST_ASSERT_EQUAL_INT(27 + 4, (int)strlen(info.atlases[RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_MOVE,
                                                                              RAD_ISO_FACING_X_MINUS)].file));

    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_OK,
                          lade_datei(RAD_SPRITE_EXAMPLES_DIR "/valid/infanterie_ohne_zeiten.json", &info));
    TEST_ASSERT_FALSE(info.own_shadow);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, info.step_ms);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, info.aim_hold_ms);
    TEST_ASSERT_EQUAL_FLOAT(12.0f,
        info.atlases[RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_MOVE, RAD_ISO_FACING_X_PLUS)].fps);
}

///
/// Jedes ungueltige Beispiel, das auch check-jsonschema abweist. Die Liste steht
/// von Hand hier, wie die Tests in main.c -- eine neue Datei unter invalid/ gehoert
/// auch hierher.
///
void test_sprite_weist_die_ungueltigen_beispiele_ab(void)
{
    static const char *const ungueltig[] = {
        "animation_fehlt", "anker_ausserhalb", "bildzahl_keine_ganze_zahl", "dateiname_ungueltig",
        "dateiname_zu_lang", "drei_key_frames", "falsche_version", "fps_bei_drehung", "fps_null",
        "key_frames_bei_folge", "ohne_anker", "ohne_zeichenbreite", "richtung_fehlt",
        "schatten_kein_wahrheitswert", "unbekannte_animation", "unbekannte_richtung",
        "unbekannte_zeit", "unbekanntes_feld", "zeit_null", "zellbreite_null", "zu_viele_bilder",
    };

    for(size_t i = 0; i < sizeof(ungueltig) / sizeof(ungueltig[0]); ++i)
    {
        char pfad[256];
        snprintf(pfad, sizeof(pfad), RAD_SPRITE_EXAMPLES_DIR "/invalid/%s.json", ungueltig[i]);
        RAD_IsoUnitSpriteInfo_t info;
        TEST_ASSERT_EQUAL_INT_MESSAGE(RAD_ISO_UNIT_SPRITE_ERROR_SCHEMA, lade_datei(pfad, &info), ungueltig[i]);
    }
}

///
/// Was nur der Leser prueft: die Schluesselbilder streng aufsteigend und im Atlas.
///
void test_sprite_prueft_die_schluesselbilder(void)
{
    const size_t length = lies(RAD_ASSETS_DIR "/units/heavy_tank/sprite.json");
    datei[length] = '\0';
    char *keys = strstr(datei, "\"key_frames\": [\n        0,\n        100,");
    TEST_ASSERT_NOT_NULL_MESSAGE(keys, "Aufbau der sprite.json des Panzers hat sich geaendert");

    RAD_IsoUnitSpriteInfo_t info;
    // 100 -> 000: nicht mehr aufsteigend.
    memcpy(strstr(keys, "100"), "000", 3);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_ERROR_SCHEMA, RAD_IsoUnitSpriteParse(datei, length, &info));

    // 000 -> 396: hinter dem letzten der 396 Bilder.
    memcpy(strstr(keys, "000"), "396", 3);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_ERROR_SCHEMA, RAD_IsoUnitSpriteParse(datei, length, &info));

    // Und zurueck auf 100: wieder gueltig -- es lag wirklich an dieser Stelle.
    memcpy(strstr(keys, "396"), "100", 3);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_OK, RAD_IsoUnitSpriteParse(datei, length, &info));
}

///
/// Die festen Puffer: zu gross, zu viele Tokens, kaputtes JSON -- und "out"
/// bleibt dabei, wie es war.
///
void test_sprite_grenzen_und_fehler_lassen_die_ausgabe_stehen(void)
{
    RAD_IsoUnitSpriteInfo_t info;
    memset(&info, 0x5A, sizeof(info));
    RAD_IsoUnitSpriteInfo_t vorher = info;

    memset(datei, ' ', RAD_ISO_UNIT_SPRITE_JSON_MAX + 1);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_ERROR_TOO_LARGE,
                          RAD_IsoUnitSpriteParse(datei, RAD_ISO_UNIT_SPRITE_JSON_MAX + 1, &info));

    // Ein Feld mit mehr Zahlen, als Tokens Platz haben.
    size_t length = 0;
    datei[length++] = '[';
    for(int32_t i = 0; i < RAD_ISO_UNIT_SPRITE_TOKENS_MAX; ++i)
    {
        datei[length++] = '1';
        datei[length++] = ',';
    }
    datei[length++] = '1';
    datei[length++] = ']';
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_ERROR_TOO_MANY_TOKENS, RAD_IsoUnitSpriteParse(datei, length, &info));

    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_ERROR_SYNTAX, lade("{\"version\":", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_ERROR_SYNTAX, lade("", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_ERROR_SCHEMA, lade("[]", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_SPRITE_ERROR_SCHEMA, lade("{}", &info));

    TEST_ASSERT_EQUAL_MEMORY(&vorher, &info, sizeof(info));
}

void test_sprite_atlanten_je_animation_und_richtung(void)
{
    TEST_ASSERT_EQUAL_INT(17, RAD_ISO_UNIT_ATLASES);

    // Die Drehung hat einen Atlas fuer alle Richtungen.
    TEST_ASSERT_EQUAL_INT(0, RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_TURN, RAD_ISO_FACING_Y_MINUS));
    TEST_ASSERT_EQUAL_INT(1, RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_AIM, RAD_ISO_FACING_X_MINUS));
    TEST_ASSERT_EQUAL_INT(4, RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_AIM, RAD_ISO_FACING_Y_MINUS));
    TEST_ASSERT_EQUAL_INT(5, RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_MOVE, RAD_ISO_FACING_X_MINUS));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_UNIT_ATLASES - 1,
                          RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_DESTROYED, RAD_ISO_FACING_Y_MINUS));

    TEST_ASSERT_EQUAL_INT(-1, RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATIONS, RAD_ISO_FACING_X_MINUS));
    TEST_ASSERT_EQUAL_INT(-1, RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_FIRE, RAD_ISO_FACINGS));

    TEST_ASSERT_TRUE(RAD_IsoUnitAnimationIsRotation(RAD_ISO_UNIT_ANIMATION_AIM));
    TEST_ASSERT_FALSE(RAD_IsoUnitAnimationIsRotation(RAD_ISO_UNIT_ANIMATION_FIRE));
    TEST_ASSERT_TRUE(RAD_IsoUnitAnimationLoops(RAD_ISO_UNIT_ANIMATION_MOVE));
    TEST_ASSERT_FALSE(RAD_IsoUnitAnimationLoops(RAD_ISO_UNIT_ANIMATION_DESTROYED));
}

///
/// Nur ein gueltiger Schluessel wird zum Pfad, und der Pfad passt immer in
/// RAD_ISO_UNIT_PATH_MAX.
///
void test_sprite_schluessel_und_pfad(void)
{
    TEST_ASSERT_TRUE(RAD_IsoUnitSpriteKeyValid("heavy_tank"));
    TEST_ASSERT_TRUE(RAD_IsoUnitSpriteKeyValid("abcdefghijklmnopqrstuvwxyz_0123"));
    TEST_ASSERT_FALSE(RAD_IsoUnitSpriteKeyValid("abcdefghijklmnopqrstuvwxyz_01234"));
    TEST_ASSERT_FALSE(RAD_IsoUnitSpriteKeyValid(""));
    TEST_ASSERT_FALSE(RAD_IsoUnitSpriteKeyValid(NULL));
    TEST_ASSERT_FALSE(RAD_IsoUnitSpriteKeyValid("Heavy_Tank"));
    TEST_ASSERT_FALSE(RAD_IsoUnitSpriteKeyValid("../tiles"));
    TEST_ASSERT_FALSE(RAD_IsoUnitSpriteKeyValid("units/x"));

    char pfad[RAD_ISO_UNIT_PATH_MAX];
    TEST_ASSERT_TRUE(RAD_IsoUnitSpritePath("heavy_tank", "turn.png", pfad, sizeof(pfad)));
    TEST_ASSERT_EQUAL_STRING("/assets/units/heavy_tank/turn.png", pfad);

    // Laengster Schluessel und laengster Dateiname.
    TEST_ASSERT_TRUE(RAD_IsoUnitSpritePath("abcdefghijklmnopqrstuvwxyz_0123",
                                           "abcdefghijklmnopqrstuvwxyz_.png", pfad, sizeof(pfad)));

    TEST_ASSERT_FALSE(RAD_IsoUnitSpritePath("../tiles", "turn.png", pfad, sizeof(pfad)));
    TEST_ASSERT_EQUAL_STRING("", pfad);
    char klein[16];
    TEST_ASSERT_FALSE(RAD_IsoUnitSpritePath("heavy_tank", "turn.png", klein, sizeof(klein)));
    TEST_ASSERT_EQUAL_STRING("", klein);
}

void test_sprite_bilder_einer_folge(void)
{
    const RAD_IsoUnitAtlasInfo_t folge = { .frame_count = 4, .fps = 10.0f };

    TEST_ASSERT_EQUAL_INT(0, RAD_IsoUnitSequenceFrame(&folge, 0.0f, true));
    TEST_ASSERT_EQUAL_INT(0, RAD_IsoUnitSequenceFrame(&folge, -50.0f, true));
    TEST_ASSERT_EQUAL_INT(1, RAD_IsoUnitSequenceFrame(&folge, 100.0f, true));
    TEST_ASSERT_EQUAL_INT(3, RAD_IsoUnitSequenceFrame(&folge, 399.0f, true));
    // In Schleife wieder von vorn, einmal abgespielt auf dem letzten stehen.
    TEST_ASSERT_EQUAL_INT(1, RAD_IsoUnitSequenceFrame(&folge, 500.0f, true));
    TEST_ASSERT_EQUAL_INT(3, RAD_IsoUnitSequenceFrame(&folge, 500.0f, false));
    TEST_ASSERT_EQUAL_INT(3, RAD_IsoUnitSequenceFrame(&folge, 1.0e9f, false));

    TEST_ASSERT_EQUAL_FLOAT(400.0f, RAD_IsoUnitSequenceMs(&folge));

    const RAD_IsoUnitAtlasInfo_t drehung = { .frame_count = 4 };
    TEST_ASSERT_EQUAL_INT(-1, RAD_IsoUnitSequenceFrame(&drehung, 100.0f, true));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, RAD_IsoUnitSequenceMs(&drehung));
}
