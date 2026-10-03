#include <radish/server/control/game_start.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Eine eigene, statische Kopie von jsmn: radish_game bringt seine Implementierung
// mit (game/src/serialization/jsmn_impl.c), haelt den Parser aber privat. Mit
// JSMN_STATIC bleiben die Funktionen in dieser Datei und stossen beim Linken nicht
// mit denen aus radish_game zusammen. JSMN_STRICT wie dort.
#define JSMN_STATIC
#define JSMN_STRICT
#include <jsmn.h>

///
/// Die Token-Liste von jsmn, flach und in Dokumentreihenfolge. Gelesen wird ueber
/// Indizes: ein Objekt steht vor seinen Schluesseln, ein Schluessel vor seinem Wert
/// (jsmn gibt einem Schluessel size == 1, sein Wert ist das eine Kind).
///
typedef struct
{
    const char *json;
    const jsmntok_t *tokens;
    int32_t number_of_tokens;
} RAD_GameStartTokens_t;

static RAD_ControlGameStartResult_t RAD_ReadPlayer(const RAD_GameStartTokens_t *t, int32_t index, RAD_ControlGameStartPlayer_t *player);
static RAD_ControlGameStartResult_t RAD_ReadArmy(const RAD_GameStartTokens_t *t, int32_t index, RAD_ControlGameStartPlayer_t *player);
static RAD_ControlGameStartResult_t RAD_ReadUnit(const RAD_GameStartTokens_t *t, int32_t index, RAD_Unit_t *unit);
static RAD_ControlGameStartResult_t RAD_ReadMember(const RAD_GameStartTokens_t *t, int32_t index, RAD_UnitMember_t *member);
static RAD_ControlGameStartResult_t RAD_ReadWeapon(const RAD_GameStartTokens_t *t, int32_t index, RAD_Weapon_t *weapon);
static RAD_ControlGameStartResult_t RAD_ReadEquipment(const RAD_GameStartTokens_t *t, int32_t index);
static RAD_ControlGameStartResult_t RAD_ReadName(const RAD_GameStartTokens_t *t, int32_t index, char out[RAD_UNIT_NAME_MAX]);
static bool RAD_ReadValue(const RAD_GameStartTokens_t *t, int32_t index, int16_t *value);
static bool RAD_ReadBool(const RAD_GameStartTokens_t *t, int32_t index, bool *value);
static bool RAD_IsIdentifierCharacter(char c);
static int32_t RAD_SubtreeEnd(const RAD_GameStartTokens_t *t, int32_t index);
static bool RAD_IsType(const RAD_GameStartTokens_t *t, int32_t index, jsmntype_t type);
static bool RAD_KeyIs(const RAD_GameStartTokens_t *t, int32_t index, const char *key);
static int32_t RAD_StringLength(const RAD_GameStartTokens_t *t, int32_t index);
static bool RAD_CopyString(const RAD_GameStartTokens_t *t, int32_t index, char *out, size_t size, int32_t min_length);


const char* RAD_ControlGameStartResultText(RAD_ControlGameStartResult_t result)
{
    switch(result)
    {
        case RAD_CONTROL_GAME_START_OK:                   return "in Ordnung";
        case RAD_CONTROL_GAME_START_ERROR_NOT_FOUND:      return "Datei nicht zu oeffnen";
        case RAD_CONTROL_GAME_START_ERROR_UNREADABLE:     return "Datei nicht zu lesen";
        case RAD_CONTROL_GAME_START_ERROR_TOO_LARGE:      return "zu gross fuer einen Spielstart";
        case RAD_CONTROL_GAME_START_ERROR_OUT_OF_MEMORY:  return "kein Speicher fuer den Inhalt";
        case RAD_CONTROL_GAME_START_ERROR_SYNTAX:         return "kein gueltiges JSON";
        case RAD_CONTROL_GAME_START_ERROR_SCHEMA:         return "nicht der erwartete Aufbau";
        case RAD_CONTROL_GAME_START_ERROR_LIMIT:          return "mehr, als das Spiel halten kann";
        default:                                          return "unbekanntes Ergebnis";
    }
}

RAD_ControlGameStart_t* RAD_ControlCreateGameStart(void)
{
    // calloc und nicht malloc: ein frischer Spielstart ist leer, nicht zufaellig.
    return calloc(1, sizeof(RAD_ControlGameStart_t));
}

void RAD_ControlDestroyGameStart(RAD_ControlGameStart_t **start)
{
    if(start == NULL)
    {
        return;
    }
    free(*start);
    *start = NULL;
}

RAD_ControlGameStartResult_t RAD_ControlLoadGameStart(const char *path, RAD_ControlGameStart_t *start)
{
    if(path == NULL || start == NULL)
    {
        return RAD_CONTROL_GAME_START_ERROR_NOT_FOUND;
    }

    // Binaermodus, wie beim Weltleser (game/src/serialization/world_file.c): nur
    // dann ist die Zahl aus ftell die Zahl der Bytes, die fread liefert.
    FILE *file = fopen(path, "rb");
    if(file == NULL)
    {
        return RAD_CONTROL_GAME_START_ERROR_NOT_FOUND;
    }

    long size = -1;
    if(0 == fseek(file, 0, SEEK_END))
    {
        size = ftell(file);
    }
    if(size < 0 || 0 != fseek(file, 0, SEEK_SET))
    {
        fclose(file);
        return RAD_CONTROL_GAME_START_ERROR_UNREADABLE;
    }
    if(size > (long)RAD_CONTROL_GAME_START_FILE_MAX)
    {
        fclose(file);
        return RAD_CONTROL_GAME_START_ERROR_TOO_LARGE;
    }

    // Ein Byte mehr, damit auch eine leere Datei nicht malloc(0) anfordert --
    // das darf NULL liefern und saehe dann aus wie "kein Speicher".
    char *json = malloc((size_t)size + 1);
    if(json == NULL)
    {
        fclose(file);
        return RAD_CONTROL_GAME_START_ERROR_OUT_OF_MEMORY;
    }

    const size_t read = fread(json, 1, (size_t)size, file);
    fclose(file);

    RAD_ControlGameStartResult_t result = RAD_CONTROL_GAME_START_ERROR_UNREADABLE;
    if(read == (size_t)size)
    {
        result = RAD_ControlParseGameStart(json, read, start);
    }

    free(json);
    return result;
}

RAD_ControlGameStartResult_t RAD_ControlParseGameStart(const char *json, size_t length, RAD_ControlGameStart_t *start)
{
    if(json == NULL || start == NULL)
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }

    // Zweimal jsmn, wie in game/src/serialization/json_reader.c: der erste Lauf
    // zaehlt nur, damit genau so viel belegt wird wie noetig.
    jsmn_parser parser;
    jsmn_init(&parser);
    const int counted = jsmn_parse(&parser, json, length, NULL, 0);
    if(counted < 1)
    {
        return RAD_CONTROL_GAME_START_ERROR_SYNTAX;
    }

    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * (size_t)counted);
    if(tokens == NULL)
    {
        return RAD_CONTROL_GAME_START_ERROR_OUT_OF_MEMORY;
    }

    jsmn_init(&parser);
    const int number_of_tokens = jsmn_parse(&parser, json, length, tokens, (unsigned int)counted);
    if(number_of_tokens != counted)
    {
        free(tokens);
        return RAD_CONTROL_GAME_START_ERROR_SYNTAX;
    }

    const RAD_GameStartTokens_t t = {
        .json = json,
        .tokens = tokens,
        .number_of_tokens = number_of_tokens
    };

    // Erst in einen eigenen Stand, dann nach "start": bei einem Fehler bleibt der
    // des Aufrufers, wie er war. Auf dem Heap, aus demselben Grund wie "start"
    // selbst (game_start.h).
    RAD_ControlGameStart_t *parsed = RAD_ControlCreateGameStart();
    if(parsed == NULL)
    {
        free(tokens);
        return RAD_CONTROL_GAME_START_ERROR_OUT_OF_MEMORY;
    }

    RAD_ControlGameStartResult_t result = RAD_CONTROL_GAME_START_ERROR_SCHEMA;

    if(RAD_IsType(&t, 0, JSMN_OBJECT))
    {
        int32_t key = 1;
        for(int32_t i=0;i < tokens[0].size; ++i)
        {
            if(RAD_KeyIs(&t, key, "spieldaten"))
            {
                const int32_t array = key + 1;
                if(!RAD_IsType(&t, array, JSMN_ARRAY)
                   || tokens[array].size != RAD_CONTROL_GAME_START_NUMBER_OF_PLAYERS)
                {
                    result = RAD_CONTROL_GAME_START_ERROR_SCHEMA;
                    break;
                }

                int32_t element = array + 1;
                result = RAD_CONTROL_GAME_START_OK;
                for(int32_t p=0;(result == RAD_CONTROL_GAME_START_OK) && (p < tokens[array].size); ++p)
                {
                    result = RAD_ReadPlayer(&t, element, &parsed->players[p]);
                    element = RAD_SubtreeEnd(&t, element);
                }
                if(result != RAD_CONTROL_GAME_START_OK)
                {
                    break;
                }
            }
            key = RAD_SubtreeEnd(&t, key);
        }
    }

    if(result == RAD_CONTROL_GAME_START_OK)
    {
        *start = *parsed;
    }

    RAD_ControlDestroyGameStart(&parsed);
    free(tokens);
    return result;
}

RAD_UserId_t RAD_ControlUserIdFromIdentifier(const char *identifier)
{
    if(identifier == NULL)
    {
        return RAD_USER_NONE;
    }

    RAD_UserId_t user = 0;
    for(int32_t i=0;i < RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH; ++i)
    {
        // Ein Zeichen ausserhalb [A-Za-z0-9] -- die Null eines zu kurzen Strings
        // eingeschlossen -- ist keine Kennung (game_start.h).
        if(!RAD_IsIdentifierCharacter(identifier[i]))
        {
            return RAD_USER_NONE;
        }
        user = (user << 8) | (RAD_UserId_t)(unsigned char)identifier[i];
    }

    if(identifier[RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH] != '\0')
    {
        return RAD_USER_NONE;
    }
    return user;
}

bool RAD_ControlIdentifierFromUserId(RAD_UserId_t user, char out[RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH + 1])
{
    char identifier[RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH + 1];
    for(int32_t i=0;i < RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH; ++i)
    {
        const int32_t shift = 8 * (RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH - 1 - i);
        identifier[i] = (char)((user >> shift) & 0xFF);
        if(!RAD_IsIdentifierCharacter(identifier[i]))
        {
            out[0] = '\0';
            return false;
        }
    }
    identifier[RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH] = '\0';

    memcpy(out, identifier, sizeof(identifier));
    return true;
}

///
/// Ein Spieler: Name, Kennung und Armee, alle drei Pflicht.
///
static RAD_ControlGameStartResult_t RAD_ReadPlayer(const RAD_GameStartTokens_t *t, int32_t index, RAD_ControlGameStartPlayer_t *player)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }

    bool name_found = false;
    bool identifier_found = false;
    bool army_found = false;

    int32_t key = index + 1;
    for(int32_t i=0;i < t->tokens[index].size; ++i)
    {
        const int32_t value = key + 1;

        if(RAD_KeyIs(t, key, "name"))
        {
            if(!RAD_CopyString(t, value, player->name, sizeof(player->name), 1))
            {
                return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            }
            name_found = true;
        }
        else if(RAD_KeyIs(t, key, "kennung"))
        {
            // Genau acht Zeichen aus [A-Za-z0-9] -- dieselbe Regel wie im Schema, und
            // die, unter der sich die Kennung in eine Id packen laesst.
            if(RAD_StringLength(t, value) != RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH
               || !RAD_CopyString(t, value, player->identifier, sizeof(player->identifier), 1)
               || RAD_ControlUserIdFromIdentifier(player->identifier) == RAD_USER_NONE)
            {
                return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            }
            identifier_found = true;
        }
        else if(RAD_KeyIs(t, key, "armee"))
        {
            const RAD_ControlGameStartResult_t result = RAD_ReadArmy(t, value, player);
            if(result != RAD_CONTROL_GAME_START_OK)
            {
                return result;
            }
            army_found = true;
        }

        key = RAD_SubtreeEnd(t, key);
    }

    if(!name_found || !identifier_found || !army_found)
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }
    return RAD_CONTROL_GAME_START_OK;
}

///
/// Eine Armee: ihr Name und ihre Einheiten. "spezies" ist Pflicht im Schema, wird
/// aber nicht gebraucht und deshalb auch nicht verlangt -- wie ein unbekannter
/// Schluessel.
///
static RAD_ControlGameStartResult_t RAD_ReadArmy(const RAD_GameStartTokens_t *t, int32_t index, RAD_ControlGameStartPlayer_t *player)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }

    bool name_found = false;
    bool units_found = false;

    int32_t key = index + 1;
    for(int32_t i=0;i < t->tokens[index].size; ++i)
    {
        const int32_t value = key + 1;

        if(RAD_KeyIs(t, key, "name"))
        {
            if(!RAD_CopyString(t, value, player->army_name, sizeof(player->army_name), 1))
            {
                return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            }
            name_found = true;
        }
        else if(RAD_KeyIs(t, key, "einheiten"))
        {
            if(!RAD_IsType(t, value, JSMN_ARRAY) || t->tokens[value].size < 1)
            {
                return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            }
            if(t->tokens[value].size > RAD_CONTROL_GAME_START_MAX_UNITS)
            {
                return RAD_CONTROL_GAME_START_ERROR_LIMIT;
            }

            int32_t unit = value + 1;
            for(int32_t u=0;u < t->tokens[value].size; ++u)
            {
                const RAD_ControlGameStartResult_t result = RAD_ReadUnit(t, unit, &player->units[u]);
                if(result != RAD_CONTROL_GAME_START_OK)
                {
                    return result;
                }
                unit = RAD_SubtreeEnd(t, unit);
            }

            player->number_of_units = t->tokens[value].size;
            units_found = true;
        }

        key = RAD_SubtreeEnd(t, key);
    }

    if(!name_found || !units_found)
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }
    return RAD_CONTROL_GAME_START_OK;
}

///
/// Die Pflichtschluessel eines Objekts als Bits: jeder gefundene setzt seines, und
/// am Ende muessen alle gesetzt sein. Ein Schluessel zweimal ist kein Fehler -- der
/// letzte gilt, wie bei jsmn ueberhaupt.
///
#define RAD_FOUND(bit) (1u << (bit))

///
/// Eine Einheit: der Einheitentyp mit seinen Werten und die Mitglieder.
///
static RAD_ControlGameStartResult_t RAD_ReadUnit(const RAD_GameStartTokens_t *t, int32_t index, RAD_Unit_t *unit)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }

    // Leer anfangen: was nicht in der Datei steht, ist 0 und nicht der Rest einer
    // frueheren Einheit. Id, Zustand und Position vergibt das Spiel (game_start.h).
    memset(unit, 0, sizeof(*unit));
    unit->id = RAD_UNIT_NONE;
    unit->type = RAD_UNIT_TYPE_PLAYER;
    unit->owner = RAD_USER_NONE;
    unit->x = -1;
    unit->y = -1;

    enum { TYP, BEWEGUNG, TRANSPORT, ZIELE, ENTITAETEN, ALLE = (1u << 5) - 1 };
    uint32_t found = 0;

    int32_t key = index + 1;
    for(int32_t i=0;i < t->tokens[index].size; ++i)
    {
        const int32_t value = key + 1;
        RAD_ControlGameStartResult_t result = RAD_CONTROL_GAME_START_OK;

        if(RAD_KeyIs(t, key, "typ"))
        {
            result = RAD_ReadName(t, value, unit->name);
            found |= RAD_FOUND(TYP);
        }
        else if(RAD_KeyIs(t, key, "bewegung"))
        {
            result = RAD_ReadValue(t, value, &unit->movement) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(BEWEGUNG);
        }
        else if(RAD_KeyIs(t, key, "transportkapazitaet"))
        {
            result = RAD_ReadValue(t, value, &unit->transport_capacity) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(TRANSPORT);
        }
        else if(RAD_KeyIs(t, key, "kann_ziele_einnehmen"))
        {
            result = RAD_ReadBool(t, value, &unit->can_capture) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(ZIELE);
        }
        else if(RAD_KeyIs(t, key, "entitaeten"))
        {
            if(!RAD_IsType(t, value, JSMN_ARRAY) || t->tokens[value].size < 1)
            {
                return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            }
            if(t->tokens[value].size > RAD_UNIT_MAX_MEMBERS)
            {
                return RAD_CONTROL_GAME_START_ERROR_LIMIT;
            }

            int32_t member = value + 1;
            for(int32_t m=0;(result == RAD_CONTROL_GAME_START_OK) && (m < t->tokens[value].size); ++m)
            {
                result = RAD_ReadMember(t, member, &unit->members[m]);
                member = RAD_SubtreeEnd(t, member);
            }
            unit->number_of_members = t->tokens[value].size;
            found |= RAD_FOUND(ENTITAETEN);
        }

        if(result != RAD_CONTROL_GAME_START_OK)
        {
            return result;
        }
        key = RAD_SubtreeEnd(t, key);
    }

    return (found == ALLE) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
}

///
/// Ein Mitglied -- im Schema eine "entitaet": das Profil mit seinen Werten, die
/// Waffen und die Ausruestung.
///
static RAD_ControlGameStartResult_t RAD_ReadMember(const RAD_GameStartTokens_t *t, int32_t index, RAD_UnitMember_t *member)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }

    enum { PROFIL, LEBEN, RUESTUNG, STAERKE, GENAUIGKEIT, WAFFEN, AUSRUESTUNG, ALLE = (1u << 7) - 1 };
    uint32_t found = 0;

    int32_t key = index + 1;
    for(int32_t i=0;i < t->tokens[index].size; ++i)
    {
        const int32_t value = key + 1;
        RAD_ControlGameStartResult_t result = RAD_CONTROL_GAME_START_OK;

        if(RAD_KeyIs(t, key, "profil"))
        {
            result = RAD_ReadName(t, value, member->profile);
            found |= RAD_FOUND(PROFIL);
        }
        else if(RAD_KeyIs(t, key, "leben"))
        {
            result = RAD_ReadValue(t, value, &member->health) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(LEBEN);
        }
        else if(RAD_KeyIs(t, key, "ruestung"))
        {
            result = RAD_ReadValue(t, value, &member->armor) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(RUESTUNG);
        }
        else if(RAD_KeyIs(t, key, "staerke"))
        {
            result = RAD_ReadValue(t, value, &member->strength) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(STAERKE);
        }
        else if(RAD_KeyIs(t, key, "genauigkeit"))
        {
            result = RAD_ReadValue(t, value, &member->accuracy) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(GENAUIGKEIT);
        }
        else if(RAD_KeyIs(t, key, "waffen"))
        {
            // Leer ist erlaubt -- ein Lastwagen traegt keine.
            if(!RAD_IsType(t, value, JSMN_ARRAY))
            {
                return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            }
            if(t->tokens[value].size > RAD_UNIT_MAX_WEAPONS)
            {
                return RAD_CONTROL_GAME_START_ERROR_LIMIT;
            }

            int32_t weapon = value + 1;
            for(int32_t w=0;(result == RAD_CONTROL_GAME_START_OK) && (w < t->tokens[value].size); ++w)
            {
                result = RAD_ReadWeapon(t, weapon, &member->weapons[w]);
                weapon = RAD_SubtreeEnd(t, weapon);
            }
            member->number_of_weapons = t->tokens[value].size;
            found |= RAD_FOUND(WAFFEN);
        }
        else if(RAD_KeyIs(t, key, "ausruestung"))
        {
            result = RAD_ReadEquipment(t, value);
            found |= RAD_FOUND(AUSRUESTUNG);
        }

        if(result != RAD_CONTROL_GAME_START_OK)
        {
            return result;
        }
        key = RAD_SubtreeEnd(t, key);
    }

    return (found == ALLE) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
}

static RAD_ControlGameStartResult_t RAD_ReadWeapon(const RAD_GameStartTokens_t *t, int32_t index, RAD_Weapon_t *weapon)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }

    enum { NAME, KLASSE, SCHUESSE, STAERKE, MIN, MAX, DURCHSCHLAG, ALLE = (1u << 7) - 1 };
    uint32_t found = 0;

    int32_t key = index + 1;
    for(int32_t i=0;i < t->tokens[index].size; ++i)
    {
        const int32_t value = key + 1;
        RAD_ControlGameStartResult_t result = RAD_CONTROL_GAME_START_OK;

        if(RAD_KeyIs(t, key, "name"))
        {
            result = RAD_ReadName(t, value, weapon->name);
            found |= RAD_FOUND(NAME);
        }
        else if(RAD_KeyIs(t, key, "klasse"))
        {
            // Die Namen aus WeaponClass im Backend. Eine Klasse, die es hier nicht
            // gibt, ist ein Schemafehler -- das Schema zaehlt sie abschliessend auf.
            static const struct { const char *name; RAD_WeaponClass_t value; } classes[] = {
                { "standard",    RAD_WEAPON_CLASS_STANDARD },
                { "heavy",       RAD_WEAPON_CLASS_HEAVY },
                { "super_heavy", RAD_WEAPON_CLASS_SUPER_HEAVY },
            };

            result = RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            for(size_t c=0;c < sizeof(classes) / sizeof(classes[0]); ++c)
            {
                const size_t length = strlen(classes[c].name);
                if(RAD_StringLength(t, value) == (int32_t)length
                   && 0 == memcmp(t->json + t->tokens[value].start, classes[c].name, length))
                {
                    weapon->weapon_class = classes[c].value;
                    result = RAD_CONTROL_GAME_START_OK;
                    break;
                }
            }
            found |= RAD_FOUND(KLASSE);
        }
        else if(RAD_KeyIs(t, key, "schuesse"))
        {
            result = RAD_ReadValue(t, value, &weapon->shots) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(SCHUESSE);
        }
        else if(RAD_KeyIs(t, key, "staerke"))
        {
            result = RAD_ReadValue(t, value, &weapon->strength) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(STAERKE);
        }
        else if(RAD_KeyIs(t, key, "min_reichweite"))
        {
            result = RAD_ReadValue(t, value, &weapon->min_range) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(MIN);
        }
        else if(RAD_KeyIs(t, key, "max_reichweite"))
        {
            result = RAD_ReadValue(t, value, &weapon->max_range) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(MAX);
        }
        else if(RAD_KeyIs(t, key, "durchschlag"))
        {
            result = RAD_ReadValue(t, value, &weapon->penetration) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(DURCHSCHLAG);
        }

        if(result != RAD_CONTROL_GAME_START_OK)
        {
            return result;
        }
        key = RAD_SubtreeEnd(t, key);
    }

    return (found == ALLE) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
}

///
/// Die Ausruestung: ein Array aus Objekten mit Namen. Geprueft und nicht behalten
/// (game_start.h) -- ihr Name hat deshalb auch keine Laengengrenze.
///
static RAD_ControlGameStartResult_t RAD_ReadEquipment(const RAD_GameStartTokens_t *t, int32_t index)
{
    if(!RAD_IsType(t, index, JSMN_ARRAY))
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }

    int32_t item = index + 1;
    for(int32_t e=0;e < t->tokens[index].size; ++e)
    {
        if(!RAD_IsType(t, item, JSMN_OBJECT))
        {
            return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
        }

        bool name_found = false;
        int32_t key = item + 1;
        for(int32_t i=0;i < t->tokens[item].size; ++i)
        {
            if(RAD_KeyIs(t, key, "name"))
            {
                if(RAD_StringLength(t, key + 1) < 1)
                {
                    return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
                }
                name_found = true;
            }
            key = RAD_SubtreeEnd(t, key);
        }
        if(!name_found)
        {
            return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
        }

        item = RAD_SubtreeEnd(t, item);
    }

    return RAD_CONTROL_GAME_START_OK;
}

///
/// Ein Name, der ins Spiel geht -- Einheitentyp, Profil, Waffe. Leer oder kein
/// String ist ein Schemafehler, zu lang fuer RAD_UNIT_NAME_MAX eine Grenze des
/// Spiels (RAD_CONTROL_GAME_START_ERROR_LIMIT).
///
static RAD_ControlGameStartResult_t RAD_ReadName(const RAD_GameStartTokens_t *t, int32_t index, char out[RAD_UNIT_NAME_MAX])
{
    const int32_t length = RAD_StringLength(t, index);
    if(length < 1)
    {
        return RAD_CONTROL_GAME_START_ERROR_SCHEMA;
    }
    if(length >= RAD_UNIT_NAME_MAX)
    {
        return RAD_CONTROL_GAME_START_ERROR_LIMIT;
    }
    return RAD_CopyString(t, index, out, RAD_UNIT_NAME_MAX, 1) ? RAD_CONTROL_GAME_START_OK : RAD_CONTROL_GAME_START_ERROR_SCHEMA;
}

///
/// Ein Spielwert ("wert" im Schema): eine ganze Zahl in 0..32767, ohne Vorzeichen,
/// ohne Nachkommastellen und ohne Exponent. Passt damit immer in int16_t.
///
static bool RAD_ReadValue(const RAD_GameStartTokens_t *t, int32_t index, int16_t *value)
{
    if(!RAD_IsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }

    const char *text = t->json + t->tokens[index].start;
    const int32_t length = t->tokens[index].end - t->tokens[index].start;

    // Ein Ziffernstring von hoechstens fuenf Stellen, sonst ist es keine Zahl in
    // diesem Bereich -- true, false, null, "-1" und "1.5" fallen hier heraus.
    if(length < 1 || length > 5)
    {
        return false;
    }

    int32_t parsed = 0;
    for(int32_t i=0;i < length; ++i)
    {
        if(text[i] < '0' || text[i] > '9')
        {
            return false;
        }
        parsed = parsed * 10 + (text[i] - '0');
    }

    if(parsed > INT16_MAX)
    {
        return false;
    }

    *value = (int16_t)parsed;
    return true;
}

static bool RAD_ReadBool(const RAD_GameStartTokens_t *t, int32_t index, bool *value)
{
    if(!RAD_IsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }

    const char *text = t->json + t->tokens[index].start;
    const int32_t length = t->tokens[index].end - t->tokens[index].start;

    if(length == 4 && 0 == memcmp(text, "true", 4))
    {
        *value = true;
        return true;
    }
    if(length == 5 && 0 == memcmp(text, "false", 5))
    {
        *value = false;
        return true;
    }
    return false;
}

static bool RAD_IsIdentifierCharacter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

///
/// Der Index hinter dem Wert an "index" samt allem, was darin verschachtelt ist.
/// Ein Token hat "size" Kinder -- bei einem Objekt die Schluessel, bei einem
/// Schluessel sein Wert --, also genuegt es, size-mal ein Kind zu ueberspringen.
/// Die Tiefe ist die der Datei, und die ist flach.
///
static int32_t RAD_SubtreeEnd(const RAD_GameStartTokens_t *t, int32_t index)
{
    if(index < 0 || index >= t->number_of_tokens)
    {
        return t->number_of_tokens;
    }

    int32_t next = index + 1;
    for(int32_t i=0;i < t->tokens[index].size; ++i)
    {
        next = RAD_SubtreeEnd(t, next);
    }
    return next;
}

static bool RAD_IsType(const RAD_GameStartTokens_t *t, int32_t index, jsmntype_t type)
{
    return index >= 0 && index < t->number_of_tokens && t->tokens[index].type == type;
}

static bool RAD_KeyIs(const RAD_GameStartTokens_t *t, int32_t index, const char *key)
{
    if(!RAD_IsType(t, index, JSMN_STRING) || t->tokens[index].size != 1)
    {
        return false;
    }
    const size_t length = (size_t)(t->tokens[index].end - t->tokens[index].start);
    return length == strlen(key) && 0 == memcmp(t->json + t->tokens[index].start, key, length);
}

///
/// Laenge des String-Werts an "index", -1 wenn dort keiner steht. Ein Schluessel
/// (size == 1) zaehlt nicht als Wert.
///
static int32_t RAD_StringLength(const RAD_GameStartTokens_t *t, int32_t index)
{
    if(!RAD_IsType(t, index, JSMN_STRING) || t->tokens[index].size != 0)
    {
        return -1;
    }
    return (int32_t)(t->tokens[index].end - t->tokens[index].start);
}

///
/// Kopiert den String-Wert an "index" nach "out", wenn er mindestens min_length
/// Zeichen hat und samt Null hineinpasst. Maskierungen werden nicht entpackt --
/// wie beim JSON-Leser des Spiels (json_reader.h) reicht der rohe Ausschnitt.
///
static bool RAD_CopyString(const RAD_GameStartTokens_t *t, int32_t index, char *out, size_t size, int32_t min_length)
{
    const int32_t length = RAD_StringLength(t, index);
    if(length < min_length || (size_t)length >= size)
    {
        return false;
    }
    memcpy(out, t->json + t->tokens[index].start, (size_t)length);
    out[length] = '\0';
    return true;
}
