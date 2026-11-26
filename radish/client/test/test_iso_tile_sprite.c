#include <unity.h>
#include <stdio.h>
#include <string.h>

#include <radish/view/rendering/iso_tile_sprite.h>

///
/// Der Leser der tile.json (view/rendering/iso_tile_sprite.h). Wie beim Leser der
/// sprite.json (test_iso_unit_sprite.c) aus denselben Dateien, die
/// check-jsonschema prueft: was das Schema annimmt, nimmt der Leser an, was es
/// abweist, auch er.
///

static char datei[2 * RAD_ISO_TILE_SPRITE_JSON_MAX];

static RAD_IsoTileSpriteResult_t lade_datei(const char *pfad, RAD_IsoTileSpriteInfo_t *info)
{
    FILE *file = fopen(pfad, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, pfad);
    const size_t length = fread(datei, 1, sizeof(datei), file);
    fclose(file);
    return RAD_IsoTileSpriteParse(datei, length, info);
}

static RAD_IsoTileSpriteResult_t lade(const char *json, RAD_IsoTileSpriteInfo_t *info)
{
    return RAD_IsoTileSpriteParse(json, strlen(json), info);
}

void test_tile_liest_das_hohe_gras(void)
{
    RAD_IsoTileSpriteInfo_t info;
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_OK, lade_datei(RAD_TILE_EXAMPLES_DIR "/valid/hohes_gras.json", &info));

    TEST_ASSERT_EQUAL_STRING("grass_tile.png", info.image);
    TEST_ASSERT_EQUAL_INT(2, info.number_of_decor);
    TEST_ASSERT_EQUAL_STRING("high_grass_back.png", info.decor[0].image);
    TEST_ASSERT_EQUAL_INT(6, info.decor[0].offset_x);
    TEST_ASSERT_EQUAL_INT(-66, info.decor[0].offset_y);
    TEST_ASSERT_FALSE(info.decor[0].in_front);
    TEST_ASSERT_EQUAL_STRING("high_grass_front.png", info.decor[1].image);
    TEST_ASSERT_TRUE(info.decor[1].in_front);
}

///
/// Die uebrigen gueltigen Beispiele. Was fehlt, ist leer oder 0.
///
void test_tile_nimmt_die_gueltigen_beispiele_an(void)
{
    RAD_IsoTileSpriteInfo_t info;

    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_OK, lade_datei(RAD_TILE_EXAMPLES_DIR "/valid/ohne_bild.json", &info));
    TEST_ASSERT_EQUAL_STRING("", info.image);
    TEST_ASSERT_EQUAL_INT(0, info.number_of_decor);

    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_OK, lade_datei(RAD_TILE_EXAMPLES_DIR "/valid/nur_grundbild.json", &info));
    TEST_ASSERT_EQUAL_STRING("grass_tile.png", info.image);
    TEST_ASSERT_EQUAL_INT(0, info.number_of_decor);

    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_OK,
                          lade_datei(RAD_TILE_EXAMPLES_DIR "/valid/dekoration_ohne_versatz.json", &info));
    TEST_ASSERT_EQUAL_STRING("", info.image);
    TEST_ASSERT_EQUAL_INT(1, info.number_of_decor);
    TEST_ASSERT_EQUAL_STRING("forest.png", info.decor[0].image);
    TEST_ASSERT_EQUAL_INT(0, info.decor[0].offset_x);
    TEST_ASSERT_EQUAL_INT(0, info.decor[0].offset_y);
    TEST_ASSERT_FALSE(info.decor[0].in_front);

    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_OK, lade_datei(RAD_TILE_EXAMPLES_DIR "/valid/an_den_grenzen.json", &info));
    TEST_ASSERT_EQUAL_INT(27 + 4, (int)strlen(info.image));
    TEST_ASSERT_EQUAL_INT(-RAD_ISO_TILE_DECOR_OFFSET_MAX, info.decor[0].offset_x);
    TEST_ASSERT_EQUAL_INT(-RAD_ISO_TILE_DECOR_OFFSET_MAX, info.decor[0].offset_y);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_DECOR_OFFSET_MAX, info.decor[1].offset_x);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_DECOR_OFFSET_MAX, info.decor[1].offset_y);
}

///
/// Jedes ungueltige Beispiel. Die Liste steht von Hand hier -- eine neue Datei
/// unter invalid/ gehoert auch hierher.
///
void test_tile_weist_die_ungueltigen_beispiele_ab(void)
{
    static const char *const ungueltig[] = {
        "dateiname_ungueltig", "dateiname_zu_lang", "dekoration_kein_array", "dekoration_ohne_bild",
        "drei_dekorationen", "falsche_version", "ohne_version", "unbekanntes_feld",
        "unbekanntes_feld_in_dekoration", "versatz_keine_ganze_zahl", "versatz_zu_gross",
        "vorne_kein_wahrheitswert",
    };

    for(size_t i = 0; i < sizeof(ungueltig) / sizeof(ungueltig[0]); ++i)
    {
        char pfad[256];
        snprintf(pfad, sizeof(pfad), RAD_TILE_EXAMPLES_DIR "/invalid/%s.json", ungueltig[i]);
        RAD_IsoTileSpriteInfo_t info;
        TEST_ASSERT_EQUAL_INT_MESSAGE(RAD_ISO_TILE_SPRITE_ERROR_SCHEMA, lade_datei(pfad, &info), ungueltig[i]);
    }
}

///
/// Die festen Puffer: zu gross, zu viele Tokens, kaputtes JSON -- und "out"
/// bleibt dabei, wie es war.
///
void test_tile_grenzen_und_fehler_lassen_die_ausgabe_stehen(void)
{
    RAD_IsoTileSpriteInfo_t info;
    memset(&info, 0x5A, sizeof(info));
    RAD_IsoTileSpriteInfo_t vorher = info;

    memset(datei, ' ', RAD_ISO_TILE_SPRITE_JSON_MAX + 1);
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_ERROR_TOO_LARGE,
                          RAD_IsoTileSpriteParse(datei, RAD_ISO_TILE_SPRITE_JSON_MAX + 1, &info));

    size_t length = 0;
    datei[length++] = '[';
    for(int32_t i = 0; i < RAD_ISO_TILE_SPRITE_TOKENS_MAX; ++i)
    {
        datei[length++] = '1';
        datei[length++] = ',';
    }
    datei[length++] = '1';
    datei[length++] = ']';
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_ERROR_TOO_MANY_TOKENS, RAD_IsoTileSpriteParse(datei, length, &info));

    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_ERROR_SYNTAX, lade("{\"version\":", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_ERROR_SYNTAX, lade("", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_ERROR_SCHEMA, lade("[]", &info));
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_ERROR_SCHEMA, lade("{}", &info));
    // Ein Minus allein ist keine Zahl.
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_ERROR_SCHEMA,
                          lade("{\"version\":1,\"decor\":[{\"image\":\"a.png\",\"offset_x\":-}]}", &info));

    TEST_ASSERT_EQUAL_MEMORY(&vorher, &info, sizeof(info));
}

///
/// Kommt "decor" zweimal vor, gilt der letzte ganz -- nichts bleibt vom ersten.
///
void test_tile_doppelte_dekoration_gilt_die_letzte(void)
{
    RAD_IsoTileSpriteInfo_t info;
    TEST_ASSERT_EQUAL_INT(RAD_ISO_TILE_SPRITE_OK,
                          lade("{\"version\":1,"
                               "\"decor\":[{\"image\":\"a.png\",\"offset_x\":5},{\"image\":\"b.png\"}],"
                               "\"decor\":[{\"image\":\"c.png\"}]}", &info));
    TEST_ASSERT_EQUAL_INT(1, info.number_of_decor);
    TEST_ASSERT_EQUAL_STRING("c.png", info.decor[0].image);
    TEST_ASSERT_EQUAL_INT(0, info.decor[0].offset_x);
    TEST_ASSERT_EQUAL_STRING("", info.decor[1].image);
}

void test_tile_pfad(void)
{
    char pfad[RAD_ISO_TILE_PATH_MAX];
    TEST_ASSERT_TRUE(RAD_IsoTileSpritePath("forest", "tile.json", pfad, sizeof(pfad)));
    TEST_ASSERT_EQUAL_STRING("/assets/tiles/forest/tile.json", pfad);

    // Der laengste Typ mit dem laengsten Dateinamen passt.
    TEST_ASSERT_TRUE(RAD_IsoTileSpritePath("water_shallow", "abcdefghijklmnopqrstuvwxyz0.png", pfad, sizeof(pfad)));

    TEST_ASSERT_FALSE(RAD_IsoTileSpritePath("../units", "tile.json", pfad, sizeof(pfad)));
    TEST_ASSERT_EQUAL_STRING("", pfad);
    TEST_ASSERT_FALSE(RAD_IsoTileSpritePath("", "tile.json", pfad, sizeof(pfad)));
    TEST_ASSERT_FALSE(RAD_IsoTileSpritePath(NULL, "tile.json", pfad, sizeof(pfad)));
    TEST_ASSERT_FALSE(RAD_IsoTileSpritePath("forest", "", pfad, sizeof(pfad)));
    TEST_ASSERT_FALSE(RAD_IsoTileSpritePath("forest", "tile.json", pfad, 20));
}
