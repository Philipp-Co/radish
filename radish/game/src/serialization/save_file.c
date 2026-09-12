#include <radish/game/game.h>
#include <radish/game/serialization/serialization.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

///
/// Die Datei -- die aeussere Schale der Serialisierung und ihre einzige oeffentliche
/// Seite (game.h).
///
/// **Sie liegt hier und nicht beim Aufrufer**, und das ist der Unterschied zu
/// vorher: dieselben vier Fragen -- gibt es die Datei, laesst sie sich lesen, ist
/// sie nicht zu gross, hat sie Speicher gebraucht -- standen bis eben im Server
/// (control/loader/save_file.c), weil er als einziger eine Datei anfasste. Nur
/// braucht jeder, der einen Spielstand laden will, genau dieselben vier, und die
/// Grenze, an der sie sich entscheiden, ist RAD_SAVE_JSON_MAX -- eine Zahl aus der
/// Groesse der Welt. Sie beim Aufrufer zu beantworten hiess, ihm die Zahl zu
/// zeigen und darauf zu vertrauen, dass er sie gleich auslegt.
///
/// **Sie ist duenn mit Absicht.** Was ein Spielstand ist, steht in
/// serialization.h; was in ihm falsch sein kann, entscheiden die Serialisierer.
/// Hier kommen nur die Fragen dazu, die die Datei stellt, und die Abbildung des
/// einen Ergebnisses auf das andere.
///
/// **Zwei Aufzaehlungen und eine Abbildung dazwischen.** RAD_SerializeResult_t ist
/// intern und beschreibt den Text, RAD_GameSaveResult_t ist oeffentlich und
/// beschreibt den Spielstand -- dieselbe Trennung wie zwischen RAD_GameResult_t
/// und RAD_ControlResult_t im Server (game.h). Der Aufrufer bekommt eine
/// Aufzaehlung fuer beides, Datei und Inhalt, und muss nicht zwei Ergebnisse
/// nebeneinander auswerten.
///
/// Binaermodus, obwohl es Text ist: nur dann ist die Zahl aus ftell die Zahl der
/// Bytes, die fread liefert. Im Textmodus darf die C-Bibliothek uebersetzen, und
/// die Laenge stimmte dann nicht mit dem ueberein, was ankommt.
///

static RAD_GameSaveResult_t RAD_GameSaveResultFromSerialize(RAD_SerializeResult_t result);
static long RAD_SaveFileSize(FILE *file);


const char* RAD_GameSaveResultText(RAD_GameSaveResult_t result)
{
    switch(result)
    {
        case RAD_GAME_SAVE_OK:                        return "in Ordnung";

        case RAD_GAME_SAVE_ERROR_NOT_FOUND:           return "Datei nicht zu oeffnen";
        case RAD_GAME_SAVE_ERROR_UNREADABLE:          return "Datei nicht zu lesen";
        case RAD_GAME_SAVE_ERROR_NOT_WRITABLE:        return "Datei nicht zu schreiben";
        case RAD_GAME_SAVE_ERROR_TOO_LARGE:           return "zu gross fuer einen Spielstand";
        case RAD_GAME_SAVE_ERROR_OUT_OF_MEMORY:       return "kein Speicher fuer den Inhalt";

        case RAD_GAME_SAVE_ERROR_SYNTAX:              return "kein gueltiges JSON";
        case RAD_GAME_SAVE_ERROR_SCHEMA:              return "nicht der erwartete Aufbau";
        case RAD_GAME_SAVE_ERROR_FORMAT:              return "fremdes Format";
        case RAD_GAME_SAVE_ERROR_VERSION:             return "andere Formatversion";
        case RAD_GAME_SAVE_ERROR_WORLD_SIZE:          return "andere Weltgroesse";

        case RAD_GAME_SAVE_ERROR_TILE_TYPE:           return "unbekannter Tile-Typ";
        case RAD_GAME_SAVE_ERROR_ENTITY_TYPE:         return "unbekannter Entitaets-Typ";
        case RAD_GAME_SAVE_ERROR_ENTITY_ID:           return "unbrauchbare Entitaets-Id";
        case RAD_GAME_SAVE_ERROR_ENTITY_POSITION:     return "Entitaet ausserhalb der Welt";
        case RAD_GAME_SAVE_ERROR_TILE_OCCUPIED:       return "zwei Entitaeten auf einem Tile";

        case RAD_GAME_SAVE_ERROR_INCONSISTENT:        return "Spielstand widerspricht sich selbst";

        default:                                      return "unbekanntes Ergebnis";
    }
}

RAD_GameSaveResult_t RAD_SaveGameToFile(const RAD_Game_t *game, const char *path)
{
    if(game == NULL || path == NULL)
    {
        return RAD_GAME_SAVE_ERROR_NOT_WRITABLE;
    }

    // Erst der ganze Text, dann die Datei: ein Spiel, das sich nicht schreiben
    // laesst, soll keine halbe Datei hinterlassen. Der Puffer liegt auf dem Heap
    // und nicht auf dem Stapel -- RAD_SAVE_JSON_MAX sind 32 KB, und die will
    // niemand in einem Aufrufrahmen haben.
    char *json = malloc(RAD_SAVE_JSON_MAX);
    if(json == NULL)
    {
        return RAD_GAME_SAVE_ERROR_OUT_OF_MEMORY;
    }

    size_t written = 0;
    const RAD_SerializeResult_t serialized =
        RAD_SerializeGameToJson(game, json, RAD_SAVE_JSON_MAX, true, &written);

    if(serialized != RAD_SERIALIZE_OK)
    {
        free(json);
        return RAD_GameSaveResultFromSerialize(serialized);
    }

    FILE *file = fopen(path, "wb");
    if(file == NULL)
    {
        free(json);
        return RAD_GAME_SAVE_ERROR_NOT_WRITABLE;
    }

    const size_t put = fwrite(json, 1, written, file);

    // fclose kann selbst noch scheitern -- gepuffertes wird erst hier
    // hinausgeschrieben. Ein Fehler daran ist derselbe Fehler wie einer beim
    // Schreiben, und ohne diese Pruefung waere eine abgeschnittene Datei ein
    // erfolgreiches Speichern.
    const bool closed = (fclose(file) == 0);

    free(json);

    if(put != written || !closed)
    {
        return RAD_GAME_SAVE_ERROR_NOT_WRITABLE;
    }

    return RAD_GAME_SAVE_OK;
}

RAD_GameSaveResult_t RAD_LoadGameFromFile(RAD_Game_t *game, const char *path)
{
    if(game == NULL || path == NULL)
    {
        return RAD_GAME_SAVE_ERROR_NOT_FOUND;
    }

    FILE *file = fopen(path, "rb");
    if(file == NULL)
    {
        return RAD_GAME_SAVE_ERROR_NOT_FOUND;
    }

    const long size = RAD_SaveFileSize(file);
    if(size < 0)
    {
        fclose(file);
        return RAD_GAME_SAVE_ERROR_UNREADABLE;
    }

    if(size > (long)RAD_SAVE_JSON_MAX)
    {
        fclose(file);
        return RAD_GAME_SAVE_ERROR_TOO_LARGE;
    }

    // Ein Byte mehr als gemessen. Nicht wegen des Inhalts --
    // RAD_DeserializeGameFromJson bekommt die Laenge mit und liest nicht darueber
    // hinaus --, sondern damit auch eine leere Datei eine Anforderung ueber null
    // Byte vermeidet: malloc(0) darf NULL liefern, und das saehe hier aus wie
    // "kein Speicher" statt wie "kein JSON".
    char *json = malloc((size_t)size + 1);
    if(json == NULL)
    {
        fclose(file);
        return RAD_GAME_SAVE_ERROR_OUT_OF_MEMORY;
    }

    const size_t read = fread(json, 1, (size_t)size, file);
    fclose(file);

    if(read != (size_t)size)
    {
        free(json);
        return RAD_GAME_SAVE_ERROR_UNREADABLE;
    }

    const RAD_SerializeResult_t result = RAD_DeserializeGameFromJson(game, json, read);

    free(json);

    return RAD_GameSaveResultFromSerialize(result);
}

///
/// Vom inneren Ergebnis auf das aeussere.
///
/// RAD_SERIALIZE_ERROR_BUFFER_TOO_SMALL wird TOO_LARGE und nicht ein eigener Wert:
/// von aussen gesehen ist "der Puffer reicht nicht" dasselbe wie "die Datei ist zu
/// gross" -- ein Spielstand jenseits der Grenze, einmal beim Schreiben, einmal
/// beim Lesen. Welcher Puffer das intern war, ist keine Auskunft, mit der ein
/// Aufrufer etwas anfangen koennte.
///
/// **Ohne default, und die Rueckgabe steht hinter dem switch.** So verlangt -Wswitch
/// fuer jeden Wert von RAD_SerializeResult_t einen Fall: wer der inneren Aufzaehlung
/// einen hinzufuegt, bekommt hier eine Warnung und muss entscheiden, was er nach
/// aussen heisst. Mit einem default waere er stillschweigend "nicht der erwartete
/// Aufbau" -- und ein neuer Fehlergrund, den niemand mehr abbildet, ist genau der,
/// von dem der Aufrufer nichts erfaehrt.
///
static RAD_GameSaveResult_t RAD_GameSaveResultFromSerialize(RAD_SerializeResult_t result)
{
    switch(result)
    {
        case RAD_SERIALIZE_OK:                        return RAD_GAME_SAVE_OK;

        case RAD_SERIALIZE_ERROR_BUFFER_TOO_SMALL:    return RAD_GAME_SAVE_ERROR_TOO_LARGE;
        case RAD_SERIALIZE_ERROR_OUT_OF_MEMORY:       return RAD_GAME_SAVE_ERROR_OUT_OF_MEMORY;

        case RAD_SERIALIZE_ERROR_SYNTAX:              return RAD_GAME_SAVE_ERROR_SYNTAX;
        case RAD_SERIALIZE_ERROR_SCHEMA:              return RAD_GAME_SAVE_ERROR_SCHEMA;
        case RAD_SERIALIZE_ERROR_FORMAT:              return RAD_GAME_SAVE_ERROR_FORMAT;
        case RAD_SERIALIZE_ERROR_VERSION:             return RAD_GAME_SAVE_ERROR_VERSION;
        case RAD_SERIALIZE_ERROR_SIZE_MISMATCH:       return RAD_GAME_SAVE_ERROR_WORLD_SIZE;

        case RAD_SERIALIZE_ERROR_TILE_TYPE:           return RAD_GAME_SAVE_ERROR_TILE_TYPE;
        case RAD_SERIALIZE_ERROR_ENTITY_TYPE:         return RAD_GAME_SAVE_ERROR_ENTITY_TYPE;
        case RAD_SERIALIZE_ERROR_ENTITY_ID:           return RAD_GAME_SAVE_ERROR_ENTITY_ID;
        case RAD_SERIALIZE_ERROR_ENTITY_POSITION:     return RAD_GAME_SAVE_ERROR_ENTITY_POSITION;
        case RAD_SERIALIZE_ERROR_TILE_OCCUPIED:       return RAD_GAME_SAVE_ERROR_TILE_OCCUPIED;

        case RAD_SERIALIZE_ERROR_INCONSISTENT:        return RAD_GAME_SAVE_ERROR_INCONSISTENT;
    }

    // Ein Wert ausserhalb der Aufzaehlung. Erreichbar nur ueber eine Zahl, die
    // niemand vergeben hat -- und dann ist es kein Erfolg.
    return RAD_GAME_SAVE_ERROR_SCHEMA;
}

///
/// Laenge der Datei in Bytes, -1 wenn sie sich nicht ermitteln laesst. Der Cursor
/// steht danach wieder am Anfang.
///
/// Eine leere Datei ergibt 0 und faellt damit nicht hier auf, sondern erst beim
/// Lesen: 0 Byte JSON sind ein Syntaxfehler, und den beschreibt die
/// Serialisierung genauer als es dieses Modul koennte.
///
static long RAD_SaveFileSize(FILE *file)
{
    if(0 != fseek(file, 0, SEEK_END))
    {
        return -1;
    }

    const long size = ftell(file);
    if(size < 0)
    {
        return -1;
    }

    if(0 != fseek(file, 0, SEEK_SET))
    {
        return -1;
    }

    return size;
}
