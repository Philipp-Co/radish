#include <unity.h>
#include <stdio.h>
#include <string.h>

#include <radish/view/rendering/iso_effect_sprite.h>

///
/// Der Leser der effect.json (view/rendering/iso_effect_sprite.h). Wie bei den
/// Lesern der sprite.json und der tile.json aus denselben Dateien, die
/// check-jsonschema prueft: was das Schema annimmt, nimmt der Leser an, was es
/// abweist, auch er.
///

static char datei[2 * RAD_ISO_EFFECT_SPRITE_JSON_MAX];

static RAD_IsoEffectSpriteResult_t lade_datei(const char *pfad, RAD_IsoEffectSpriteInfo_t *info)
{
    FILE *file = fopen(pfad, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, pfad);
    const size_t length = fread(datei, 1, sizeof(datei), file);
    fclose(file);
    return RAD_IsoEffectSpriteParse(datei, length, info);
}

static RAD_IsoEffectSpriteResult_t lade(const char *json, RAD_IsoEffectSpriteInfo_t *info)
{
    return RAD_IsoEffectSpriteParse(json, strlen(json), info);
}

void test_effect_liest_explosion_2(void)
{
    RAD_IsoEffectSpriteInfo_t info;
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_SPRITE_OK,
                          lade_datei(RAD_EFFECT_EXAMPLES_DIR "/valid/explosion_2.json", &info));

    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_KIND_EXPLOSION, info.kind);
    TEST_ASSERT_EQUAL_INT(256, info.draw_width);
    TEST_ASSERT_EQUAL_FLOAT(2500.0f, info.duration_ms);

    // Nach RAD_IsoFacing_t: sw = X_MINUS, se = Y_PLUS, ne = X_PLUS, nw = Y_MINUS.
    const RAD_IsoEffectAtlasInfo_t *ne = &info.atlases[RAD_ISO_FACING_X_PLUS];
    TEST_ASSERT_EQUAL_STRING("explosion_2.png", ne->file);
    TEST_ASSERT_EQUAL_INT(128, ne->frame_width);
    TEST_ASSERT_EQUAL_INT(128, ne->frame_height);
    TEST_ASSERT_EQUAL_INT(0, ne->first_frame);
    TEST_ASSERT_EQUAL_INT(146, ne->frame_count);
    TEST_ASSERT_EQUAL_FLOAT(0.414f, ne->anchor_x);
    TEST_ASSERT_EQUAL_FLOAT(0.883f, ne->anchor_y);

    TEST_ASSERT_EQUAL_INT(292, info.atlases[RAD_ISO_FACING_X_MINUS].first_frame);
    TEST_ASSERT_EQUAL_INT(438, info.atlases[RAD_ISO_FACING_Y_PLUS].first_frame);
    TEST_ASSERT_EQUAL_INT(146, info.atlases[RAD_ISO_FACING_Y_MINUS].first_frame);
}

void test_effect_nimmt_die_gueltigen_beispiele_an(void)
{
    RAD_IsoEffectSpriteInfo_t info;

    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_SPRITE_OK,
                          lade_datei(RAD_EFFECT_EXAMPLES_DIR "/valid/eine_folge_fuer_alle.json", &info));
    for(int32_t facing = 0; facing < RAD_ISO_FACINGS; ++facing)
    {
        TEST_ASSERT_EQUAL_STRING("explosion_0.png", info.atlases[facing].file);
        TEST_ASSERT_EQUAL_INT(0, info.atlases[facing].first_frame);
    }

    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_SPRITE_OK,
                          lade_datei(RAD_EFFECT_EXAMPLES_DIR "/valid/an_den_grenzen.json", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_DRAW_WIDTH_MAX, info.draw_width);
    TEST_ASSERT_EQUAL_FLOAT(RAD_ISO_EFFECT_DURATION_MS_MAX, info.duration_ms);
    const RAD_IsoEffectAtlasInfo_t *sw = &info.atlases[RAD_ISO_FACING_X_MINUS];
    TEST_ASSERT_EQUAL_INT(27 + 4, (int)strlen(sw->file));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_FRAME_SIZE_MAX, sw->frame_width);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_FIRST_FRAME_MAX, sw->first_frame);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_FRAMES_MAX, sw->frame_count);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sw->anchor_x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sw->anchor_y);
}

///
/// Jedes ungueltige Beispiel. Die Liste steht von Hand hier -- eine neue Datei
/// unter invalid/ gehoert auch hierher.
///
void test_effect_weist_die_ungueltigen_beispiele_ab(void)
{
    static const char *const ungueltig[] = {
        "anker_ausserhalb", "breite_zu_gross", "dateiname_ungueltig", "dateiname_zu_lang", "dauer_null",
        "dauer_zu_lang", "erste_zelle_negativ", "erste_zelle_zu_gross", "falsche_version", "ohne_anker",
        "ohne_art", "ohne_version", "richtung_fehlt", "unbekannte_art", "unbekannte_richtung",
        "unbekanntes_feld", "unbekanntes_feld_in_folge", "zellbreite_keine_ganze_zahl", "zu_viele_bilder",
    };

    for(size_t i = 0; i < sizeof(ungueltig) / sizeof(ungueltig[0]); ++i)
    {
        char pfad[256];
        snprintf(pfad, sizeof(pfad), "%s/invalid/%s.json", RAD_EFFECT_EXAMPLES_DIR, ungueltig[i]);
        RAD_IsoEffectSpriteInfo_t info;
        TEST_ASSERT_EQUAL_INT_MESSAGE(RAD_ISO_EFFECT_SPRITE_ERROR_SCHEMA, lade_datei(pfad, &info), ungueltig[i]);
    }
}

void test_effect_grenzen_und_fehler_lassen_die_ausgabe_stehen(void)
{
    RAD_IsoEffectSpriteInfo_t info;
    memset(&info, 0x5a, sizeof(info));
    RAD_IsoEffectSpriteInfo_t vorher = info;

    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_SPRITE_ERROR_SYNTAX, lade("{\"version\": ", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_SPRITE_ERROR_SCHEMA, lade("[]", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_SPRITE_ERROR_SCHEMA, lade("{\"version\": 1}", &info));

    static char gross[RAD_ISO_EFFECT_SPRITE_JSON_MAX + 2];
    memset(gross, ' ', sizeof(gross) - 1);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_SPRITE_ERROR_TOO_LARGE, lade(gross, &info));

    static char viele[RAD_ISO_EFFECT_SPRITE_JSON_MAX];
    strcpy(viele, "[");
    for(int32_t i = 0; i < RAD_ISO_EFFECT_SPRITE_TOKENS_MAX; ++i)
    {
        strcat(viele, "1,");
    }
    strcat(viele, "1]");
    TEST_ASSERT_EQUAL_INT(RAD_ISO_EFFECT_SPRITE_ERROR_TOO_MANY_TOKENS, lade(viele, &info));

    TEST_ASSERT_EQUAL_MEMORY(&vorher, &info, sizeof(info));
}

void test_effect_schluessel_und_pfad(void)
{
    char pfad[RAD_ISO_EFFECT_PATH_MAX];

    TEST_ASSERT_TRUE(RAD_IsoEffectSpritePath("explosion_1", "0.png", pfad, sizeof(pfad)));
    TEST_ASSERT_EQUAL_STRING("/assets/effects/explosion_1/0.png", pfad);

    TEST_ASSERT_TRUE(RAD_IsoEffectSpriteKeyValid("abcdefghijklmnopqrstuvwxyz01234"));
    TEST_ASSERT_FALSE(RAD_IsoEffectSpriteKeyValid("abcdefghijklmnopqrstuvwxyz012345"));
    TEST_ASSERT_FALSE(RAD_IsoEffectSpriteKeyValid(""));
    TEST_ASSERT_FALSE(RAD_IsoEffectSpriteKeyValid("."));
    TEST_ASSERT_FALSE(RAD_IsoEffectSpriteKeyValid(".."));
    TEST_ASSERT_FALSE(RAD_IsoEffectSpriteKeyValid("a/b"));
    TEST_ASSERT_FALSE(RAD_IsoEffectSpriteKeyValid(NULL));

    TEST_ASSERT_FALSE(RAD_IsoEffectSpritePath("..", "x.png", pfad, sizeof(pfad)));
    TEST_ASSERT_EQUAL_STRING("", pfad);
    TEST_ASSERT_FALSE(RAD_IsoEffectSpritePath("explosion_1", "0.png", pfad, 10));
    TEST_ASSERT_EQUAL_STRING("", pfad);

    // Der laengste Pfad passt.
    TEST_ASSERT_TRUE(RAD_IsoEffectSpritePath("abcdefghijklmnopqrstuvwxyz01234", "abcdefghijklmnopqrstuvwxyz0.png",
                                             pfad, sizeof(pfad)));
}
