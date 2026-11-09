#ifndef __RAD_RENDERING_ISO_UNIT_SPRITE_H__
#define __RAD_RENDERING_ISO_UNIT_SPRITE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/rendering/iso_facing.h>

///
/// Die Beschreibung eines Einheitenbildes, wie sie in
/// assets/units/<schluessel>/sprite.json steht (client/schema/sprite.schema.json),
/// und ihr Leser.
///
/// **Ohne SDL und ohne Heap.** Diese Datei kennt nur Zahlen und Dateinamen; die
/// Texturen dazu laedt rendering/iso_unit_images.c. So laesst sich der Leser auf
/// dem Host testen (client/test/), und alles, was er braucht, steht in festen
/// Feldern: der Lesepuffer und die Tokens bei ihm selbst, die Beschreibung beim
/// Aufrufer. Was nicht hineinpasst, weist er ab, statt es abzuschneiden.
///
/// **Jede Figur bietet alle Animationen an** (RAD_IsoUnitAnimation_t). Fehlt
/// eine, ist die ganze Beschreibung ungueltig -- halbe Figuren gibt es nicht.
///

///
/// Laenge des Schluessels samt Null: der Ordner unter assets/units/, nur
/// [a-z0-9_]. Wie RAD_UNIT_SPRITE_KEY_MAX im Spiel und
/// RAD_CLIENT_UNIT_SPRITE_KEY_MAX im Modell.
///
#define RAD_ISO_UNIT_SPRITE_KEY_MAX 32

///
/// Laenge eines Atlas-Dateinamens samt Null, z.B. "turn.png": hoechstens 27
/// Zeichen aus [a-z0-9_] und ".png". Zusammen mit dem Schluessel passt der Pfad
/// so immer in RAD_ISO_UNIT_PATH_MAX:
/// "/assets/units/" (14) + 31 + "/" + 31 + Null = 78.
///
#define RAD_ISO_UNIT_FILE_NAME_MAX 32
#define RAD_ISO_UNIT_PATH_MAX 96

///
/// Obergrenzen der Zahlen in einem Atlas -- dieselben wie im Schema. Sie halten
/// jede Rechnung damit weit innerhalb von int32_t.
///
#define RAD_ISO_UNIT_FRAMES_MAX 1024
#define RAD_ISO_UNIT_FRAME_SIZE_MAX 1024
#define RAD_ISO_UNIT_DRAW_WIDTH_MAX 1024
#define RAD_ISO_UNIT_FPS_MAX 60.0f
#define RAD_ISO_UNIT_DURATION_MS_MAX 10000.0f

///
/// Groesse einer sprite.json in Bytes und wie viele JSON-Tokens sie haben darf.
/// Eine voll belegte Datei (17 Atlanten, alle Zeiten) hat rund 3,5 KB und gut 300
/// Tokens; beides laesst Luft fuer Einrueckung und Reihenfolge.
///
#define RAD_ISO_UNIT_SPRITE_JSON_MAX 8192
#define RAD_ISO_UNIT_SPRITE_TOKENS_MAX 384

///
/// Was eine Figur kann. Die Liste steht hier fest, weil das Spiel auf jede
/// Animation eigens reagiert; eine neue Einheit braucht nur Daten, eine neue
/// Animation Code.
///
///  - TURN: die Figur dreht sich; ihre Schluesselbilder sind zugleich das
///    Standbild je Blickrichtung. Ein Atlas fuer alle Richtungen.
///  - AIM: zielen bei fester Blickrichtung (beim Panzer der Turm), je
///    Blickrichtung eine volle Drehung.
///  - MOVE: ein Schritt von Feld zu Feld, je Richtung, in Schleife.
///  - FIRE: der Schuss, je Richtung, einmal.
///  - DESTROYED: die Zerstoerung, je Richtung, einmal; das letzte Bild bleibt.
///
typedef enum
{
    RAD_ISO_UNIT_ANIMATION_TURN = 0,
    RAD_ISO_UNIT_ANIMATION_AIM,
    RAD_ISO_UNIT_ANIMATION_MOVE,
    RAD_ISO_UNIT_ANIMATION_FIRE,
    RAD_ISO_UNIT_ANIMATION_DESTROYED,
    RAD_ISO_UNIT_ANIMATIONS
} RAD_IsoUnitAnimation_t;

///
/// Wie viele Atlanten eine Figur hat -- genau so viele, nicht hoechstens: einer
/// fuer TURN, vier fuer jede andere Animation.
///
#define RAD_ISO_UNIT_ATLASES (1 + (RAD_ISO_UNIT_ANIMATIONS - 1) * RAD_ISO_FACINGS)

///
/// Die Stelle eines Atlas in RAD_IsoUnitSpriteInfo_t.atlases. 0 ist die Drehung.
///
typedef int32_t RAD_IsoUnitAtlas_t;
#define RAD_ISO_UNIT_ATLAS_TURN 0

///
/// Der Atlas fuer "animation" in Blickrichtung "facing"; bei TURN zaehlt die
/// Richtung nicht. -1 ausserhalb der Aufzaehlungen.
///
RAD_IsoUnitAtlas_t RAD_IsoUnitAtlasOf(RAD_IsoUnitAnimation_t animation, RAD_IsoFacing_t facing);

///
/// Ob "animation" eine Drehung ist (TURN, AIM) -- mit Schluesselbildern, nach
/// Winkel gezeigt -- oder eine Folge (MOVE, FIRE, DESTROYED) mit fps.
///
bool RAD_IsoUnitAnimationIsRotation(RAD_IsoUnitAnimation_t animation);

///
/// Ob eine Folge in Schleife laeuft (MOVE) oder einmal (FIRE, DESTROYED).
///
bool RAD_IsoUnitAnimationLoops(RAD_IsoUnitAnimation_t animation);

///
/// Ein Atlas, wie er in seiner Datei aufgeteilt ist: zeilenweise von links oben,
/// Zellen von frame_width x frame_height. "key_frames" nur bei einer Drehung,
/// "fps" nur bei einer Folge; das jeweils andere ist 0.
///
typedef struct
{
    char file[RAD_ISO_UNIT_FILE_NAME_MAX];
    int32_t frame_width;
    int32_t frame_height;
    int32_t frame_count;
    int32_t key_frames[RAD_ISO_FACINGS];
    float fps;
} RAD_IsoUnitAtlasInfo_t;

///
/// Eine ganze sprite.json. Die Zeiten sind 0, wenn die Datei keine nennt -- dann
/// gilt der Standard aus iso_unit_images.h.
///
typedef struct
{
    RAD_IsoUnitAtlasInfo_t atlases[RAD_ISO_UNIT_ATLASES];
    float anchor_x;
    float anchor_y;
    int32_t draw_width;
    bool own_shadow;
    float step_ms;
    float turn_ms;
    float aim_ms;
    float aim_hold_ms;
} RAD_IsoUnitSpriteInfo_t;

typedef enum
{
    RAD_ISO_UNIT_SPRITE_OK = 0,
    /// Laenger als RAD_ISO_UNIT_SPRITE_JSON_MAX.
    RAD_ISO_UNIT_SPRITE_ERROR_TOO_LARGE,
    /// Kein gueltiges JSON.
    RAD_ISO_UNIT_SPRITE_ERROR_SYNTAX,
    /// Mehr als RAD_ISO_UNIT_SPRITE_TOKENS_MAX Tokens.
    RAD_ISO_UNIT_SPRITE_ERROR_TOO_MANY_TOKENS,
    /// Gueltiges JSON, aber nicht nach dem Schema -- auch was das Schema nicht
    /// ausdruecken kann (key_frames nicht aufsteigend oder nicht < frame_count).
    RAD_ISO_UNIT_SPRITE_ERROR_SCHEMA
} RAD_IsoUnitSpriteResult_t;

const char* RAD_IsoUnitSpriteResultText(RAD_IsoUnitSpriteResult_t result);

///
/// Liest "length" Bytes JSON aus "json" (ohne abschliessende Null) nach "out".
/// Bei einem Fehler bleibt "out" unangetastet. Unbekannte Schluessel sind ein
/// Fehler, wie im Schema (additionalProperties: false); kommt ein Schluessel
/// zweimal vor, gilt der letzte.
///
/// Nicht wiedereintrittsfaehig: die Tokens liegen in einem statischen Feld.
/// Der Client liest nur aus dem Hauptfaden.
///
RAD_IsoUnitSpriteResult_t RAD_IsoUnitSpriteParse(const char *json, size_t length, RAD_IsoUnitSpriteInfo_t *out);

///
/// Ob "key" ein gueltiger Schluessel ist: 1 bis 31 Zeichen aus [a-z0-9_]. Nur
/// ein solcher wird zum Pfad -- "../" oder "/" kommen nicht durch.
///
bool RAD_IsoUnitSpriteKeyValid(const char *key);

///
/// Der Pfad eines Atlas: "/assets/units/<key>/<file>". false, wenn der Schluessel
/// nicht gueltig ist oder "size" nicht reicht; "out" ist dann leer.
///
bool RAD_IsoUnitSpritePath(const char *key, const char *file, char *out, size_t size);

///
/// Welches Bild einer Folge "elapsed_ms" nach ihrem Beginn zu sehen ist: in
/// Schleife reihum, sonst bis zum letzten und dort stehen bleibend. -1 fuer einen
/// Atlas ohne Bilder oder ohne fps.
///
int32_t RAD_IsoUnitSequenceFrame(const RAD_IsoUnitAtlasInfo_t *atlas, float elapsed_ms, bool loop);

///
/// Wie lange eine Folge einmal durchlaeuft, in Millisekunden; 0 ohne fps.
///
float RAD_IsoUnitSequenceMs(const RAD_IsoUnitAtlasInfo_t *atlas);

#endif
