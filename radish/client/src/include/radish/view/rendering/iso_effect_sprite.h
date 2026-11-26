#ifndef __RAD_VIEW_RENDERING_ISO_EFFECT_SPRITE_H__
#define __RAD_VIEW_RENDERING_ISO_EFFECT_SPRITE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/view/rendering/iso_facing.h>

///
/// Die Beschreibung eines Effekts, wie sie in assets/effects/<schluessel>/
/// effect.json steht (client/schema/effect.schema.json), und ihr Leser -- das
/// Gegenstueck zu iso_unit_sprite.h fuer die Effekte, etwa die Explosionen.
///
/// **Ohne SDL und ohne Heap**, wie die Leser der sprite.json und der tile.json:
/// Zahlen und Dateinamen in festen Feldern, Tokens bei ihm selbst. Die
/// Texturen dazu laedt view/rendering/iso_effect_images.c. So laesst er sich
/// auf dem Host testen (client/test/).
///
/// **Der Ordner ist der Schluessel** des Effekts. Welche es gibt, sagt nicht der
/// Code: die Dateien erzeugt das Backend aus der Datenbank (api/domain/effects/),
/// und der Client nimmt jeden Ordner unter assets/effects/ mit einer effect.json.
///

///
/// Laenge eines Dateinamens bzw. Schluessels samt Null: hoechstens 27 Zeichen
/// aus [a-z0-9_] und ".png" bzw. 31 Zeichen aus [a-z0-9_]. Der laengste Pfad:
/// "/assets/effects/" (16) + 31 + "/" + 31 + Null = 80.
///
#define RAD_ISO_EFFECT_FILE_NAME_MAX 32
#define RAD_ISO_EFFECT_KEY_MAX 32
#define RAD_ISO_EFFECT_PATH_MAX 80

///
/// Grenzen einer Folge (effect.schema.json): Zellgroesse, erste Zelle und Zahl
/// der Bilder, Breite auf dem Bildschirm und Dauer.
///
#define RAD_ISO_EFFECT_FRAME_SIZE_MAX 1024
#define RAD_ISO_EFFECT_FIRST_FRAME_MAX 4095
#define RAD_ISO_EFFECT_FRAMES_MAX 1024
#define RAD_ISO_EFFECT_DRAW_WIDTH_MAX 1024
#define RAD_ISO_EFFECT_DURATION_MS_MAX 10000.0f

///
/// Groesse einer effect.json in Bytes und wie viele JSON-Tokens sie haben darf.
/// Eine voll belegte Datei hat gut 1,2 KB und 70 Tokens.
///
#define RAD_ISO_EFFECT_SPRITE_JSON_MAX 4096
#define RAD_ISO_EFFECT_SPRITE_TOKENS_MAX 128

///
/// Wofuer der Client einen Effekt nimmt ("kind").
///
typedef enum
{
    /// Bei einem Angriff eine davon, zufaellig (RAD_IsoEffectRandomExplosion).
    RAD_ISO_EFFECT_KIND_EXPLOSION = 0,
    RAD_ISO_EFFECT_KINDS
} RAD_IsoEffectKind_t;

///
/// Die Folge einer Richtung: in welcher Datei, wie die aufgeteilt ist, und wo
/// darin der Anker liegt -- als Anteil an Breite und Hoehe einer Zelle.
///
typedef struct
{
    char file[RAD_ISO_EFFECT_FILE_NAME_MAX];
    int32_t frame_width;
    int32_t frame_height;
    int32_t first_frame;
    int32_t frame_count;
    float anchor_x;
    float anchor_y;
} RAD_IsoEffectAtlasInfo_t;

///
/// Eine ganze effect.json; "atlases" nach RAD_IsoFacing_t, alle vier belegt.
///
typedef struct
{
    RAD_IsoEffectKind_t kind;
    int32_t draw_width;
    float duration_ms;
    RAD_IsoEffectAtlasInfo_t atlases[RAD_ISO_FACINGS];
} RAD_IsoEffectSpriteInfo_t;

typedef enum
{
    RAD_ISO_EFFECT_SPRITE_OK = 0,
    /// Laenger als RAD_ISO_EFFECT_SPRITE_JSON_MAX.
    RAD_ISO_EFFECT_SPRITE_ERROR_TOO_LARGE,
    /// Kein gueltiges JSON.
    RAD_ISO_EFFECT_SPRITE_ERROR_SYNTAX,
    /// Mehr als RAD_ISO_EFFECT_SPRITE_TOKENS_MAX Tokens.
    RAD_ISO_EFFECT_SPRITE_ERROR_TOO_MANY_TOKENS,
    /// Gueltiges JSON, aber nicht nach dem Schema.
    RAD_ISO_EFFECT_SPRITE_ERROR_SCHEMA
} RAD_IsoEffectSpriteResult_t;

const char* RAD_IsoEffectSpriteResultText(RAD_IsoEffectSpriteResult_t result);

///
/// Liest "length" Bytes JSON aus "json" (ohne abschliessende Null) nach "out".
/// Bei einem Fehler bleibt "out" unangetastet. Unbekannte Schluessel sind ein
/// Fehler, wie im Schema (additionalProperties: false); kommt ein Schluessel
/// zweimal vor, gilt der letzte.
///
/// Nicht wiedereintrittsfaehig: die Tokens liegen in einem statischen Feld.
///
RAD_IsoEffectSpriteResult_t RAD_IsoEffectSpriteParse(const char *json, size_t length, RAD_IsoEffectSpriteInfo_t *out);

///
/// Ob "key" ein gueltiger Schluessel ist: 1 bis 31 Zeichen aus [a-z0-9_] -- so
/// kommen "..", "." und alles mit "/" nicht als Ordner durch.
///
bool RAD_IsoEffectSpriteKeyValid(const char *key);

///
/// Der Pfad einer Datei des Effekts "key": "/assets/effects/<key>/<file>".
/// false, wenn "key" oder "file" nicht gehen oder "size" nicht reicht; "out"
/// ist dann leer.
///
bool RAD_IsoEffectSpritePath(const char *key, const char *file, char *out, size_t size);

#endif
