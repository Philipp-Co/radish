#include "save_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

///
/// Die Datei wird als Ganzes gelesen und dann uebergeben: RAD_DeserializeGameFromJson
/// braucht den vollstaendigen Text am Stueck, weil jsmn ihn zweimal durchlaeuft --
/// einmal zum Zaehlen der Token, einmal zum Lesen.
///
/// Binaermodus, obwohl es Text ist: nur dann ist die Zahl aus ftell die Zahl der
/// Bytes, die fread liefert. Im Textmodus darf die C-Bibliothek uebersetzen, und
/// die Laenge stimmte dann nicht mit dem ueberein, was ankommt.
///

static long RAD_SaveFileSize(FILE *file);


const char* RAD_SaveFileResultText(RAD_SaveFileResult_t result)
{
    switch(result)
    {
        case RAD_SAVE_FILE_OK:                  return "in Ordnung";
        case RAD_SAVE_FILE_ERROR_NOT_FOUND:     return "Datei nicht zu oeffnen";
        case RAD_SAVE_FILE_ERROR_UNREADABLE:    return "Datei nicht zu lesen";
        case RAD_SAVE_FILE_ERROR_TOO_LARGE:     return "Datei zu gross fuer einen Spielstand";
        case RAD_SAVE_FILE_ERROR_OUT_OF_MEMORY: return "kein Speicher fuer den Inhalt";
        case RAD_SAVE_FILE_ERROR_CONTENT:       return "Inhalt abgelehnt";
        default:                                return "unbekanntes Ergebnis";
    }
}

RAD_SaveFileResult_t RAD_ReadGameFromSaveFile(const char *path, RAD_Game_t *game, RAD_SerializeResult_t *out_reason)
{
    FILE *file = fopen(path, "rb");
    if(file == NULL)
    {
        return RAD_SAVE_FILE_ERROR_NOT_FOUND;
    }

    const long size = RAD_SaveFileSize(file);
    if(size < 0)
    {
        fclose(file);
        return RAD_SAVE_FILE_ERROR_UNREADABLE;
    }

    if(size > (long)RAD_SAVE_JSON_MAX)
    {
        fclose(file);
        return RAD_SAVE_FILE_ERROR_TOO_LARGE;
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
        return RAD_SAVE_FILE_ERROR_OUT_OF_MEMORY;
    }

    const size_t read = fread(json, 1, (size_t)size, file);
    fclose(file);

    if(read != (size_t)size)
    {
        free(json);
        return RAD_SAVE_FILE_ERROR_UNREADABLE;
    }

    const RAD_SerializeResult_t result = RAD_DeserializeGameFromJson(game, json, read);

    free(json);

    if(result != RAD_SERIALIZE_OK)
    {
        if(out_reason != NULL)
        {
            *out_reason = result;
        }
        return RAD_SAVE_FILE_ERROR_CONTENT;
    }

    return RAD_SAVE_FILE_OK;
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
