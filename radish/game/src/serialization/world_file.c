#include <radish/game/game.h>
#include <radish/game/serialization/serialization.h>
#include <radish/game/serialization/world_definition.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

///
/// Die Datei -- die aeussere Schale des Weltlesers und seine einzige oeffentliche
/// Seite (game.h).
///
/// **Sie ist duenn mit Absicht.** Was eine Weltdefinition ist, steht in
/// world_definition.h; was in ihr falsch sein kann, entscheidet der Leser dort.
/// Hier kommen nur die Fragen dazu, die die Datei stellt -- gibt es sie, laesst sie
/// sich lesen, ist sie nicht zu gross, hat sie Speicher gebraucht --, und die
/// Abbildung des einen Ergebnisses auf das andere.
///
/// **Zwei Aufzaehlungen und eine Abbildung dazwischen.** RAD_SerializeResult_t ist
/// intern und beschreibt den Text, RAD_GameLoadResult_t ist oeffentlich und
/// beschreibt das Laden -- dieselbe Trennung wie zwischen RAD_GameResult_t und
/// RAD_ControlResult_t im Server (game.h). Der Aufrufer bekommt eine Aufzaehlung
/// fuer beides, Datei und Inhalt, und muss nicht zwei Ergebnisse nebeneinander
/// auswerten.
///
/// Binaermodus, obwohl es Text ist: nur dann ist die Zahl aus ftell die Zahl der
/// Bytes, die fread liefert. Im Textmodus darf die C-Bibliothek uebersetzen, und
/// die Laenge stimmte dann nicht mit dem ueberein, was ankommt.
///

static RAD_GameLoadResult_t RAD_GameLoadResultFromSerialize(RAD_SerializeResult_t result);
static RAD_GameLoadResult_t RAD_ReadWholeFile(const char *path, char **json, size_t *length);
static long RAD_FileSize(FILE *file);


const char* RAD_GameLoadResultText(RAD_GameLoadResult_t result)
{
    switch(result)
    {
        case RAD_GAME_LOAD_OK:                        return "in Ordnung";

        case RAD_GAME_LOAD_ERROR_NOT_FOUND:           return "Datei nicht zu oeffnen";
        case RAD_GAME_LOAD_ERROR_UNREADABLE:          return "Datei nicht zu lesen";
        case RAD_GAME_LOAD_ERROR_TOO_LARGE:           return "zu gross fuer eine Weltdefinition";
        case RAD_GAME_LOAD_ERROR_OUT_OF_MEMORY:       return "kein Speicher fuer den Inhalt";

        case RAD_GAME_LOAD_ERROR_SYNTAX:              return "kein gueltiges JSON";
        case RAD_GAME_LOAD_ERROR_SCHEMA:              return "nicht der erwartete Aufbau";
        case RAD_GAME_LOAD_ERROR_VERSION:             return "andere Formatversion";
        case RAD_GAME_LOAD_ERROR_WORLD_SIZE:          return "andere Weltgroesse";

        case RAD_GAME_LOAD_ERROR_TILE_TYPE:           return "unbekannter Tile-Typ";

        case RAD_GAME_LOAD_ERROR_INCONSISTENT:        return "Weltdefinition widerspricht sich selbst";
        case RAD_GAME_LOAD_ERROR_WORLD_OCCUPIED:      return "auf der Welt stehen schon Figuren";

        default:                                      return "unbekanntes Ergebnis";
    }
}

RAD_GameLoadResult_t RAD_LoadWorldFromFile(RAD_Game_t *game, const char *path)
{
    if(game == NULL)
    {
        return RAD_GAME_LOAD_ERROR_NOT_FOUND;
    }

    char *json = NULL;
    size_t length = 0;
    const RAD_GameLoadResult_t read = RAD_ReadWholeFile(path, &json, &length);
    if(read != RAD_GAME_LOAD_OK)
    {
        return read;
    }

    const RAD_SerializeResult_t result = RAD_DeserializeWorldDefinitionFromJson(game, json, length);

    free(json);

    return RAD_GameLoadResultFromSerialize(result);
}

///
/// Die Datei ganz in den Speicher: vier Fragen -- oeffnen, messen, Grenze, lesen
/// -- und die Grenze RAD_JSON_FILE_MAX. Bei RAD_GAME_LOAD_OK gehoert "*json" dem
/// Aufrufer und wird mit free() freigegeben.
///
static RAD_GameLoadResult_t RAD_ReadWholeFile(const char *path, char **json, size_t *length)
{
    *json = NULL;
    *length = 0;

    if(path == NULL)
    {
        return RAD_GAME_LOAD_ERROR_NOT_FOUND;
    }

    FILE *file = fopen(path, "rb");
    if(file == NULL)
    {
        return RAD_GAME_LOAD_ERROR_NOT_FOUND;
    }

    const long size = RAD_FileSize(file);
    if(size < 0)
    {
        fclose(file);
        return RAD_GAME_LOAD_ERROR_UNREADABLE;
    }

    if(size > (long)RAD_JSON_FILE_MAX)
    {
        fclose(file);
        return RAD_GAME_LOAD_ERROR_TOO_LARGE;
    }

    // Ein Byte mehr als gemessen. Nicht wegen des Inhalts --
    // RAD_DeserializeWorldDefinitionFromJson bekommt die Laenge mit und liest nicht darueber
    // hinaus --, sondern damit auch eine leere Datei eine Anforderung ueber null
    // Byte vermeidet: malloc(0) darf NULL liefern, und das saehe hier aus wie
    // "kein Speicher" statt wie "kein JSON".
    char *buffer = malloc((size_t)size + 1);
    if(buffer == NULL)
    {
        fclose(file);
        return RAD_GAME_LOAD_ERROR_OUT_OF_MEMORY;
    }

    const size_t read = fread(buffer, 1, (size_t)size, file);
    fclose(file);

    if(read != (size_t)size)
    {
        free(buffer);
        return RAD_GAME_LOAD_ERROR_UNREADABLE;
    }

    *json = buffer;
    *length = read;
    return RAD_GAME_LOAD_OK;
}

///
/// Vom inneren Ergebnis auf das aeussere.
///
/// **Ohne default, und die Rueckgabe steht hinter dem switch.** So verlangt -Wswitch
/// fuer jeden Wert von RAD_SerializeResult_t einen Fall: wer der inneren Aufzaehlung
/// einen hinzufuegt, bekommt hier eine Warnung und muss entscheiden, was er nach
/// aussen heisst. Mit einem default waere er stillschweigend "nicht der erwartete
/// Aufbau" -- und ein neuer Fehlergrund, den niemand mehr abbildet, ist genau der,
/// von dem der Aufrufer nichts erfaehrt.
///
static RAD_GameLoadResult_t RAD_GameLoadResultFromSerialize(RAD_SerializeResult_t result)
{
    switch(result)
    {
        case RAD_SERIALIZE_OK:                        return RAD_GAME_LOAD_OK;

        case RAD_SERIALIZE_ERROR_OUT_OF_MEMORY:       return RAD_GAME_LOAD_ERROR_OUT_OF_MEMORY;

        case RAD_SERIALIZE_ERROR_SYNTAX:              return RAD_GAME_LOAD_ERROR_SYNTAX;
        case RAD_SERIALIZE_ERROR_SCHEMA:              return RAD_GAME_LOAD_ERROR_SCHEMA;
        case RAD_SERIALIZE_ERROR_VERSION:             return RAD_GAME_LOAD_ERROR_VERSION;
        case RAD_SERIALIZE_ERROR_SIZE_MISMATCH:       return RAD_GAME_LOAD_ERROR_WORLD_SIZE;

        case RAD_SERIALIZE_ERROR_TILE_TYPE:           return RAD_GAME_LOAD_ERROR_TILE_TYPE;

        case RAD_SERIALIZE_ERROR_INCONSISTENT:        return RAD_GAME_LOAD_ERROR_INCONSISTENT;
        case RAD_SERIALIZE_ERROR_WORLD_OCCUPIED:      return RAD_GAME_LOAD_ERROR_WORLD_OCCUPIED;
    }

    // Ein Wert ausserhalb der Aufzaehlung. Erreichbar nur ueber eine Zahl, die
    // niemand vergeben hat -- und dann ist es kein Erfolg.
    return RAD_GAME_LOAD_ERROR_SCHEMA;
}

///
/// Laenge der Datei in Bytes, -1 wenn sie sich nicht ermitteln laesst. Der Cursor
/// steht danach wieder am Anfang.
///
/// Eine leere Datei ergibt 0 und faellt damit nicht hier auf, sondern erst beim
/// Lesen: 0 Byte JSON sind ein Syntaxfehler, und den beschreibt die
/// Serialisierung genauer als es dieses Modul koennte.
///
static long RAD_FileSize(FILE *file)
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
