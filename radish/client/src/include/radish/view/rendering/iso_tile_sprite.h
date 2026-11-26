#ifndef __RAD_VIEW_RENDERING_ISO_TILE_SPRITE_H__
#define __RAD_VIEW_RENDERING_ISO_TILE_SPRITE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///
/// Die Beschreibung eines Tile-Bildes, wie sie in assets/tiles/<typ>/tile.json
/// steht (client/schema/tile.schema.json), und ihr Leser -- das Gegenstueck zu
/// iso_unit_sprite.h fuer die Felder.
///
/// **Ohne SDL und ohne Heap**, wie der Leser der sprite.json: Zahlen und
/// Dateinamen in festen Feldern, der Lesepuffer und die Tokens bei ihm selbst.
/// Die Texturen dazu laedt view/rendering/iso_tile_images.c. So laesst er sich
/// auf dem Host testen (client/test/).
///
/// **Der Ordner ist der Tile-Typ** (RAD_IsoTileSpriteKey): es gibt genau die
/// Typen des Spiels, und jeder hat hoechstens ein Bild. Die Datei erzeugt das
/// Backend aus der Datenbank (api/domain/tiles/); die Farbe des Wuerfels, wenn
/// kein Bild da ist, steht dagegen im Code (view/rendering/iso_tile/).
///

///
/// Laenge eines Dateinamens samt Null, z.B. "grass_tile.png": hoechstens 27
/// Zeichen aus [a-z0-9_] und ".png". Der laengste Pfad:
/// "/assets/tiles/" (14) + "water_shallow" (13) + "/" + 31 + Null = 60.
///
#define RAD_ISO_TILE_FILE_NAME_MAX 32
#define RAD_ISO_TILE_PATH_MAX 64

///
/// Wie viele Dekorationen eine Datei nennen darf -- wie RAD_ISO_DECOR_PER_TILE
/// (iso_decor.h, das SDL braucht und hier deshalb nicht eingebunden wird;
/// iso_tile_images.c prueft, dass beide gleich sind).
///
#define RAD_ISO_TILE_DECOR_MAX 2

///
/// Wie weit eine Dekoration gegen ihr Feld verschoben sein darf, in jede
/// Richtung: eine Feldhoehe (RAD_ISO_TILE_HEIGHT). Nach oben ist das zugleich
/// RAD_ISO_DECOR_MAX_RISE, nach dem die Kamera ihre Grenze rechnet.
///
#define RAD_ISO_TILE_DECOR_OFFSET_MAX 128

///
/// Groesse einer tile.json in Bytes und wie viele JSON-Tokens sie haben darf.
/// Eine voll belegte Datei hat gut 300 Bytes und 23 Tokens.
///
#define RAD_ISO_TILE_SPRITE_JSON_MAX 2048
#define RAD_ISO_TILE_SPRITE_TOKENS_MAX 64

///
/// Eine Dekoration (RAD_IsoDecor_t ohne Textur).
///
typedef struct
{
    char image[RAD_ISO_TILE_FILE_NAME_MAX];
    int32_t offset_x;
    int32_t offset_y;
    bool in_front;
} RAD_IsoTileDecorInfo_t;

///
/// Eine ganze tile.json. "image" ist leer, wenn die Datei keines nennt -- dann
/// bleibt der Wuerfel. Gueltig sind die ersten "number_of_decor" Eintraege von
/// "decor".
///
typedef struct
{
    char image[RAD_ISO_TILE_FILE_NAME_MAX];
    RAD_IsoTileDecorInfo_t decor[RAD_ISO_TILE_DECOR_MAX];
    int32_t number_of_decor;
} RAD_IsoTileSpriteInfo_t;

typedef enum
{
    RAD_ISO_TILE_SPRITE_OK = 0,
    /// Laenger als RAD_ISO_TILE_SPRITE_JSON_MAX.
    RAD_ISO_TILE_SPRITE_ERROR_TOO_LARGE,
    /// Kein gueltiges JSON.
    RAD_ISO_TILE_SPRITE_ERROR_SYNTAX,
    /// Mehr als RAD_ISO_TILE_SPRITE_TOKENS_MAX Tokens.
    RAD_ISO_TILE_SPRITE_ERROR_TOO_MANY_TOKENS,
    /// Gueltiges JSON, aber nicht nach dem Schema.
    RAD_ISO_TILE_SPRITE_ERROR_SCHEMA
} RAD_IsoTileSpriteResult_t;

const char* RAD_IsoTileSpriteResultText(RAD_IsoTileSpriteResult_t result);

///
/// Liest "length" Bytes JSON aus "json" (ohne abschliessende Null) nach "out".
/// Bei einem Fehler bleibt "out" unangetastet. Unbekannte Schluessel sind ein
/// Fehler, wie im Schema (additionalProperties: false); kommt ein Schluessel
/// zweimal vor, gilt der letzte.
///
/// Nicht wiedereintrittsfaehig: die Tokens liegen in einem statischen Feld.
///
RAD_IsoTileSpriteResult_t RAD_IsoTileSpriteParse(const char *json, size_t length, RAD_IsoTileSpriteInfo_t *out);

///
/// Der Pfad einer Datei des Typs "key": "/assets/tiles/<key>/<file>". "key" ist
/// der Name des Tile-Typs ([a-z0-9_], 1 bis 31 Zeichen) -- "../" oder "/" kommen
/// nicht durch. false, wenn "key" oder "file" nicht gehen oder "size" nicht
/// reicht; "out" ist dann leer.
///
bool RAD_IsoTileSpritePath(const char *key, const char *file, char *out, size_t size);

#endif
