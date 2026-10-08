#include <radish/game/control/start_game.h>
#include <radish/game/model/game.h>
#include <radish/game/model/unit_pool/unit_pool.h>
#include <radish/game/model/player/player.h>
#include <radish/game/model/reserve/reserve.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Nur die Deklarationen, wie in json_reader.h: die Implementierung erzeugt
// einmalig jsmn_impl.c, mit JSMN_STRICT. Im Server lag hier eine eigene,
// statische Kopie, damit sie nicht mit der aus radish_game zusammenstiess --
// innerhalb des Moduls ist es dieselbe.
#define JSMN_HEADER
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

static RAD_GameStartResult_t RAD_ReadPlayer(const RAD_GameStartTokens_t *t, int32_t index, RAD_GameStartPlayer_t *player);
static RAD_GameStartResult_t RAD_ReadArmy(const RAD_GameStartTokens_t *t, int32_t index, RAD_GameStartPlayer_t *player);
static RAD_GameStartResult_t RAD_ReadUnit(const RAD_GameStartTokens_t *t, int32_t index, RAD_Unit_t *unit);
static RAD_GameStartResult_t RAD_ReadMember(const RAD_GameStartTokens_t *t, int32_t index, RAD_UnitMember_t *member);
static RAD_GameStartResult_t RAD_ReadWeapon(const RAD_GameStartTokens_t *t, int32_t index, RAD_Weapon_t *weapon);
static RAD_GameStartResult_t RAD_ReadEquipment(const RAD_GameStartTokens_t *t, int32_t index);
static RAD_GameStartResult_t RAD_ReadName(const RAD_GameStartTokens_t *t, int32_t index, char out[RAD_UNIT_NAME_MAX]);
static bool RAD_ReadValue(const RAD_GameStartTokens_t *t, int32_t index, int16_t *value);
static bool RAD_ReadBool(const RAD_GameStartTokens_t *t, int32_t index, bool *value);
static bool RAD_IsIdentifierCharacter(char c);
static int32_t RAD_SubtreeEnd(const RAD_GameStartTokens_t *t, int32_t index);
static bool RAD_IsType(const RAD_GameStartTokens_t *t, int32_t index, jsmntype_t type);
static bool RAD_KeyIs(const RAD_GameStartTokens_t *t, int32_t index, const char *key);
static int32_t RAD_StringLength(const RAD_GameStartTokens_t *t, int32_t index);
static bool RAD_CopyString(const RAD_GameStartTokens_t *t, int32_t index, char *out, size_t size, int32_t min_length);


const char* RAD_GameStartResultText(RAD_GameStartResult_t result)
{
    switch(result)
    {
        case RAD_GAME_START_OK:                   return "in Ordnung";
        case RAD_GAME_START_ERROR_NOT_FOUND:      return "Datei nicht zu oeffnen";
        case RAD_GAME_START_ERROR_UNREADABLE:     return "Datei nicht zu lesen";
        case RAD_GAME_START_ERROR_TOO_LARGE:      return "zu gross fuer einen Spielstart";
        case RAD_GAME_START_ERROR_OUT_OF_MEMORY:  return "kein Speicher fuer den Inhalt";
        case RAD_GAME_START_ERROR_SYNTAX:         return "kein gueltiges JSON";
        case RAD_GAME_START_ERROR_SCHEMA:         return "nicht der erwartete Aufbau";
        case RAD_GAME_START_ERROR_LIMIT:          return "mehr, als das Spiel halten kann";
        case RAD_GAME_START_ERROR_REJECTED:       return "vom Spiel abgelehnt";
        default:                                          return "unbekanntes Ergebnis";
    }
}

RAD_GameStart_t* RAD_CreateGameStart(void)
{
    // calloc und nicht malloc: ein frischer Spielstart ist leer, nicht zufaellig.
    return calloc(1, sizeof(RAD_GameStart_t));
}

void RAD_DestroyGameStart(RAD_GameStart_t **start)
{
    if(start == NULL)
    {
        return;
    }
    free(*start);
    *start = NULL;
}

RAD_GameStartResult_t RAD_LoadGameStart(const char *path, RAD_GameStart_t *start)
{
    if(path == NULL || start == NULL)
    {
        return RAD_GAME_START_ERROR_NOT_FOUND;
    }

    // Binaermodus, wie beim Weltleser (game/src/serialization/world_file.c): nur
    // dann ist die Zahl aus ftell die Zahl der Bytes, die fread liefert.
    FILE *file = fopen(path, "rb");
    if(file == NULL)
    {
        return RAD_GAME_START_ERROR_NOT_FOUND;
    }

    long size = -1;
    if(0 == fseek(file, 0, SEEK_END))
    {
        size = ftell(file);
    }
    if(size < 0 || 0 != fseek(file, 0, SEEK_SET))
    {
        fclose(file);
        return RAD_GAME_START_ERROR_UNREADABLE;
    }
    if(size > (long)RAD_GAME_START_FILE_MAX)
    {
        fclose(file);
        return RAD_GAME_START_ERROR_TOO_LARGE;
    }

    // Ein Byte mehr, damit auch eine leere Datei nicht malloc(0) anfordert --
    // das darf NULL liefern und saehe dann aus wie "kein Speicher".
    char *json = malloc((size_t)size + 1);
    if(json == NULL)
    {
        fclose(file);
        return RAD_GAME_START_ERROR_OUT_OF_MEMORY;
    }

    const size_t read = fread(json, 1, (size_t)size, file);
    fclose(file);

    RAD_GameStartResult_t result = RAD_GAME_START_ERROR_UNREADABLE;
    if(read == (size_t)size)
    {
        result = RAD_ParseGameStart(json, read, start);
    }

    free(json);
    return result;
}

RAD_GameStartResult_t RAD_ParseGameStart(const char *json, size_t length, RAD_GameStart_t *start)
{
    if(json == NULL || start == NULL)
    {
        return RAD_GAME_START_ERROR_SCHEMA;
    }

    // Zweimal jsmn, wie in game/src/serialization/json_reader.c: der erste Lauf
    // zaehlt nur, damit genau so viel belegt wird wie noetig.
    jsmn_parser parser;
    jsmn_init(&parser);
    const int counted = jsmn_parse(&parser, json, length, NULL, 0);
    if(counted < 1)
    {
        return RAD_GAME_START_ERROR_SYNTAX;
    }

    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * (size_t)counted);
    if(tokens == NULL)
    {
        return RAD_GAME_START_ERROR_OUT_OF_MEMORY;
    }

    jsmn_init(&parser);
    const int number_of_tokens = jsmn_parse(&parser, json, length, tokens, (unsigned int)counted);
    if(number_of_tokens != counted)
    {
        free(tokens);
        return RAD_GAME_START_ERROR_SYNTAX;
    }

    const RAD_GameStartTokens_t t = {
        .json = json,
        .tokens = tokens,
        .number_of_tokens = number_of_tokens
    };

    // Erst in einen eigenen Stand, dann nach "start": bei einem Fehler bleibt der
    // des Aufrufers, wie er war. Auf dem Heap, aus demselben Grund wie "start"
    // selbst (start_game.h).
    RAD_GameStart_t *parsed = RAD_CreateGameStart();
    if(parsed == NULL)
    {
        free(tokens);
        return RAD_GAME_START_ERROR_OUT_OF_MEMORY;
    }

    RAD_GameStartResult_t result = RAD_GAME_START_ERROR_SCHEMA;

    if(RAD_IsType(&t, 0, JSMN_OBJECT))
    {
        int32_t key = 1;
        for(int32_t i=0;i < tokens[0].size; ++i)
        {
            if(RAD_KeyIs(&t, key, "spieldaten"))
            {
                const int32_t array = key + 1;
                if(!RAD_IsType(&t, array, JSMN_ARRAY)
                   || tokens[array].size != RAD_GAME_START_NUMBER_OF_PLAYERS)
                {
                    result = RAD_GAME_START_ERROR_SCHEMA;
                    break;
                }

                int32_t element = array + 1;
                result = RAD_GAME_START_OK;
                for(int32_t p=0;(result == RAD_GAME_START_OK) && (p < tokens[array].size); ++p)
                {
                    result = RAD_ReadPlayer(&t, element, &parsed->players[p]);
                    element = RAD_SubtreeEnd(&t, element);
                }
                if(result != RAD_GAME_START_OK)
                {
                    break;
                }
            }
            key = RAD_SubtreeEnd(&t, key);
        }
    }

    if(result == RAD_GAME_START_OK)
    {
        *start = *parsed;
    }

    RAD_DestroyGameStart(&parsed);
    free(tokens);
    return result;
}

RAD_UserId_t RAD_UserIdFromIdentifier(const char *identifier)
{
    if(identifier == NULL)
    {
        return RAD_USER_NONE;
    }

    RAD_UserId_t user = 0;
    for(int32_t i=0;i < RAD_GAME_START_IDENTIFIER_LENGTH; ++i)
    {
        // Ein Zeichen ausserhalb [A-Za-z0-9] -- die Null eines zu kurzen Strings
        // eingeschlossen -- ist keine Kennung (start_game.h).
        if(!RAD_IsIdentifierCharacter(identifier[i]))
        {
            return RAD_USER_NONE;
        }
        user = (user << 8) | (RAD_UserId_t)(unsigned char)identifier[i];
    }

    if(identifier[RAD_GAME_START_IDENTIFIER_LENGTH] != '\0')
    {
        return RAD_USER_NONE;
    }
    return user;
}

bool RAD_IdentifierFromUserId(RAD_UserId_t user, char out[RAD_GAME_START_IDENTIFIER_LENGTH + 1])
{
    char identifier[RAD_GAME_START_IDENTIFIER_LENGTH + 1];
    for(int32_t i=0;i < RAD_GAME_START_IDENTIFIER_LENGTH; ++i)
    {
        const int32_t shift = 8 * (RAD_GAME_START_IDENTIFIER_LENGTH - 1 - i);
        identifier[i] = (char)((user >> shift) & 0xFF);
        if(!RAD_IsIdentifierCharacter(identifier[i]))
        {
            out[0] = '\0';
            return false;
        }
    }
    identifier[RAD_GAME_START_IDENTIFIER_LENGTH] = '\0';

    memcpy(out, identifier, sizeof(identifier));
    return true;
}

RAD_GameResult_t RAD_StartGame(RAD_Game_t *game, const RAD_GameStart_t *start)
{
    if(game == NULL || start == NULL)
    {
        return RAD_GAME_ERROR_INVALID_UNIT;
    }

    // Genau einmal: ein Spiel mit Einheiten hat seinen Start hinter sich
    // (start_game.h). Spieler darf es schon haben -- im Server tritt bei, wer sein
    // erstes Kommando schickt, und das kann vor dem Start ankommen.
    if(game->started || (RAD_UnitPoolNumberOfUnits(game->unit_pool) > 0))
    {
        return RAD_GAME_ERROR_STARTED;
    }

    RAD_UserId_t users[RAD_GAME_START_NUMBER_OF_PLAYERS];
    int32_t number_of_units = 0;
    for(int32_t p=0;p < RAD_GAME_START_NUMBER_OF_PLAYERS; ++p)
    {
        users[p] = RAD_UserIdFromIdentifier(start->players[p].identifier);
        if(users[p] == RAD_USER_NONE)
        {
            return RAD_GAME_ERROR_NO_USER;
        }
        number_of_units += start->players[p].number_of_units;
    }

    // Der Pool ist leer (oben), also ist das die ganze Frage nach dem Platz. Danach
    // kann RAD_UnitPoolAddUnit nicht mehr scheitern -- und es entsteht keine
    // Einheit, die gleich wieder weg muesste.
    if(number_of_units > RAD_MAX_UNITS)
    {
        return RAD_GAME_ERROR_FULL;
    }

    // Wer schon mitspielte, bleibt bei einer Ablehnung drin; zurueck geht nur,
    // wer erst mit diesem Start beigetreten ist.
    bool joined_here[RAD_GAME_START_NUMBER_OF_PLAYERS] = { false };
    for(int32_t p=0;p < RAD_GAME_START_NUMBER_OF_PLAYERS; ++p)
    {
        joined_here[p] = !RAD_GameIsPlaying(game, users[p]);
        const RAD_GameResult_t joined = RAD_GameAddPlayer(game, users[p]);
        if(joined != RAD_GAME_OK)
        {
            for(int32_t k=0;k < p; ++k)
            {
                if(joined_here[k])
                {
                    RAD_GameRemovePlayer(game, users[k]);
                }
            }
            return joined;
        }
    }

    for(int32_t p=0;p < RAD_GAME_START_NUMBER_OF_PLAYERS; ++p)
    {
        const RAD_GameStartPlayer_t *from = &start->players[p];
        RAD_Player_t *player = RAD_GameFindPlayer(game, users[p]);
        RAD_Reserve_t *reserve = RAD_PlayerReserve(player);

        for(int32_t u=0;u < from->number_of_units; ++u)
        {
            // Der Pool legt die Einheit an und vergibt Id, Zustand und Position;
            // aus der Datei kommen nur die Werte. Die Kopie ueberschreibt alles,
            // also wird danach zurueckgesetzt, was der Pool vergeben hat.
            RAD_Unit_t *unit = RAD_UnitPoolAddUnit(game->unit_pool);
            const RAD_Unit_t created = *unit;

            *unit = from->units[u];
            unit->id = created.id;
            unit->state = created.state;
            unit->x = created.x;
            unit->y = created.y;
            unit->owner = users[p];

            // Beide Listen fassen RAD_MAX_UNITS, eine Armee hoechstens
            // RAD_GAME_START_MAX_UNITS -- keiner der zwei Aufrufe kann scheitern.
            RAD_PlayerAddUnit(player, unit);
            RAD_ReserveAddUnit(reserve, unit);
        }
    }

    return RAD_GAME_OK;
}

RAD_GameStartResult_t RAD_StartGameFromFile(RAD_Game_t *game, const char *path, RAD_GameResult_t *game_result)
{
    if(game_result != NULL)
    {
        *game_result = RAD_GAME_OK;
    }

    // Auf dem Heap: zwei Armeen mit allen Werten sind zu gross fuer einen
    // Aufrufrahmen (start_game.h).
    RAD_GameStart_t *start = RAD_CreateGameStart();
    if(start == NULL)
    {
        return RAD_GAME_START_ERROR_OUT_OF_MEMORY;
    }

    RAD_GameStartResult_t result = RAD_LoadGameStart(path, start);
    if(result == RAD_GAME_START_OK)
    {
        const RAD_GameResult_t started = RAD_StartGame(game, start);
        if(game_result != NULL)
        {
            *game_result = started;
        }
        if(started != RAD_GAME_OK)
        {
            result = RAD_GAME_START_ERROR_REJECTED;
        }
    }

    RAD_DestroyGameStart(&start);
    return result;
}

///
/// Ein Spieler: Name, Kennung und Armee, alle drei Pflicht.
///
static RAD_GameStartResult_t RAD_ReadPlayer(const RAD_GameStartTokens_t *t, int32_t index, RAD_GameStartPlayer_t *player)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_GAME_START_ERROR_SCHEMA;
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
                return RAD_GAME_START_ERROR_SCHEMA;
            }
            name_found = true;
        }
        else if(RAD_KeyIs(t, key, "kennung"))
        {
            // Genau acht Zeichen aus [A-Za-z0-9] -- dieselbe Regel wie im Schema, und
            // die, unter der sich die Kennung in eine Id packen laesst.
            if(RAD_StringLength(t, value) != RAD_GAME_START_IDENTIFIER_LENGTH
               || !RAD_CopyString(t, value, player->identifier, sizeof(player->identifier), 1)
               || RAD_UserIdFromIdentifier(player->identifier) == RAD_USER_NONE)
            {
                return RAD_GAME_START_ERROR_SCHEMA;
            }
            identifier_found = true;
        }
        else if(RAD_KeyIs(t, key, "armee"))
        {
            const RAD_GameStartResult_t result = RAD_ReadArmy(t, value, player);
            if(result != RAD_GAME_START_OK)
            {
                return result;
            }
            army_found = true;
        }

        key = RAD_SubtreeEnd(t, key);
    }

    if(!name_found || !identifier_found || !army_found)
    {
        return RAD_GAME_START_ERROR_SCHEMA;
    }
    return RAD_GAME_START_OK;
}

///
/// Eine Armee: ihr Name und ihre Einheiten. "spezies" ist Pflicht im Schema, wird
/// aber nicht gebraucht und deshalb auch nicht verlangt -- wie ein unbekannter
/// Schluessel.
///
static RAD_GameStartResult_t RAD_ReadArmy(const RAD_GameStartTokens_t *t, int32_t index, RAD_GameStartPlayer_t *player)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_GAME_START_ERROR_SCHEMA;
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
                return RAD_GAME_START_ERROR_SCHEMA;
            }
            name_found = true;
        }
        else if(RAD_KeyIs(t, key, "einheiten"))
        {
            if(!RAD_IsType(t, value, JSMN_ARRAY) || t->tokens[value].size < 1)
            {
                return RAD_GAME_START_ERROR_SCHEMA;
            }
            if(t->tokens[value].size > RAD_GAME_START_MAX_UNITS)
            {
                return RAD_GAME_START_ERROR_LIMIT;
            }

            int32_t unit = value + 1;
            for(int32_t u=0;u < t->tokens[value].size; ++u)
            {
                const RAD_GameStartResult_t result = RAD_ReadUnit(t, unit, &player->units[u]);
                if(result != RAD_GAME_START_OK)
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
        return RAD_GAME_START_ERROR_SCHEMA;
    }
    return RAD_GAME_START_OK;
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
static RAD_GameStartResult_t RAD_ReadUnit(const RAD_GameStartTokens_t *t, int32_t index, RAD_Unit_t *unit)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_GAME_START_ERROR_SCHEMA;
    }

    // Leer anfangen: was nicht in der Datei steht, ist 0 und nicht der Rest einer
    // frueheren Einheit. Id, Zustand und Position vergibt das Spiel (start_game.h).
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
        RAD_GameStartResult_t result = RAD_GAME_START_OK;

        if(RAD_KeyIs(t, key, "typ"))
        {
            result = RAD_ReadName(t, value, unit->name);
            found |= RAD_FOUND(TYP);
        }
        else if(RAD_KeyIs(t, key, "bewegung"))
        {
            result = RAD_ReadValue(t, value, &unit->movement) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(BEWEGUNG);
        }
        else if(RAD_KeyIs(t, key, "transportkapazitaet"))
        {
            result = RAD_ReadValue(t, value, &unit->transport_capacity) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(TRANSPORT);
        }
        else if(RAD_KeyIs(t, key, "kann_ziele_einnehmen"))
        {
            result = RAD_ReadBool(t, value, &unit->can_capture) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(ZIELE);
        }
        else if(RAD_KeyIs(t, key, "entitaeten"))
        {
            if(!RAD_IsType(t, value, JSMN_ARRAY) || t->tokens[value].size < 1)
            {
                return RAD_GAME_START_ERROR_SCHEMA;
            }
            if(t->tokens[value].size > RAD_UNIT_MAX_MEMBERS)
            {
                return RAD_GAME_START_ERROR_LIMIT;
            }

            int32_t member = value + 1;
            for(int32_t m=0;(result == RAD_GAME_START_OK) && (m < t->tokens[value].size); ++m)
            {
                result = RAD_ReadMember(t, member, &unit->members[m]);
                member = RAD_SubtreeEnd(t, member);
            }
            unit->number_of_members = t->tokens[value].size;
            found |= RAD_FOUND(ENTITAETEN);
        }

        if(result != RAD_GAME_START_OK)
        {
            return result;
        }
        key = RAD_SubtreeEnd(t, key);
    }

    return (found == ALLE) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
}

///
/// Ein Mitglied -- im Schema eine "entitaet": das Profil mit seinen Werten, die
/// Waffen und die Ausruestung.
///
static RAD_GameStartResult_t RAD_ReadMember(const RAD_GameStartTokens_t *t, int32_t index, RAD_UnitMember_t *member)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_GAME_START_ERROR_SCHEMA;
    }

    enum { PROFIL, LEBEN, RUESTUNG, STAERKE, GENAUIGKEIT, WAFFEN, AUSRUESTUNG, ALLE = (1u << 7) - 1 };
    uint32_t found = 0;

    int32_t key = index + 1;
    for(int32_t i=0;i < t->tokens[index].size; ++i)
    {
        const int32_t value = key + 1;
        RAD_GameStartResult_t result = RAD_GAME_START_OK;

        if(RAD_KeyIs(t, key, "profil"))
        {
            result = RAD_ReadName(t, value, member->profile);
            found |= RAD_FOUND(PROFIL);
        }
        else if(RAD_KeyIs(t, key, "leben"))
        {
            result = RAD_ReadValue(t, value, &member->health) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(LEBEN);
        }
        else if(RAD_KeyIs(t, key, "ruestung"))
        {
            result = RAD_ReadValue(t, value, &member->armor) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(RUESTUNG);
        }
        else if(RAD_KeyIs(t, key, "staerke"))
        {
            result = RAD_ReadValue(t, value, &member->strength) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(STAERKE);
        }
        else if(RAD_KeyIs(t, key, "genauigkeit"))
        {
            result = RAD_ReadValue(t, value, &member->accuracy) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(GENAUIGKEIT);
        }
        else if(RAD_KeyIs(t, key, "waffen"))
        {
            // Leer ist erlaubt -- ein Lastwagen traegt keine.
            if(!RAD_IsType(t, value, JSMN_ARRAY))
            {
                return RAD_GAME_START_ERROR_SCHEMA;
            }
            if(t->tokens[value].size > RAD_UNIT_MAX_WEAPONS)
            {
                return RAD_GAME_START_ERROR_LIMIT;
            }

            int32_t weapon = value + 1;
            for(int32_t w=0;(result == RAD_GAME_START_OK) && (w < t->tokens[value].size); ++w)
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

        if(result != RAD_GAME_START_OK)
        {
            return result;
        }
        key = RAD_SubtreeEnd(t, key);
    }

    return (found == ALLE) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
}

static RAD_GameStartResult_t RAD_ReadWeapon(const RAD_GameStartTokens_t *t, int32_t index, RAD_Weapon_t *weapon)
{
    if(!RAD_IsType(t, index, JSMN_OBJECT))
    {
        return RAD_GAME_START_ERROR_SCHEMA;
    }

    enum { NAME, KLASSE, SCHUESSE, STAERKE, MIN, MAX, DURCHSCHLAG, ALLE = (1u << 7) - 1 };
    uint32_t found = 0;

    int32_t key = index + 1;
    for(int32_t i=0;i < t->tokens[index].size; ++i)
    {
        const int32_t value = key + 1;
        RAD_GameStartResult_t result = RAD_GAME_START_OK;

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

            result = RAD_GAME_START_ERROR_SCHEMA;
            for(size_t c=0;c < sizeof(classes) / sizeof(classes[0]); ++c)
            {
                const size_t length = strlen(classes[c].name);
                if(RAD_StringLength(t, value) == (int32_t)length
                   && 0 == memcmp(t->json + t->tokens[value].start, classes[c].name, length))
                {
                    weapon->weapon_class = classes[c].value;
                    result = RAD_GAME_START_OK;
                    break;
                }
            }
            found |= RAD_FOUND(KLASSE);
        }
        else if(RAD_KeyIs(t, key, "schuesse"))
        {
            result = RAD_ReadValue(t, value, &weapon->shots) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(SCHUESSE);
        }
        else if(RAD_KeyIs(t, key, "staerke"))
        {
            result = RAD_ReadValue(t, value, &weapon->strength) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(STAERKE);
        }
        else if(RAD_KeyIs(t, key, "min_reichweite"))
        {
            result = RAD_ReadValue(t, value, &weapon->min_range) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(MIN);
        }
        else if(RAD_KeyIs(t, key, "max_reichweite"))
        {
            result = RAD_ReadValue(t, value, &weapon->max_range) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(MAX);
        }
        else if(RAD_KeyIs(t, key, "durchschlag"))
        {
            result = RAD_ReadValue(t, value, &weapon->penetration) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
            found |= RAD_FOUND(DURCHSCHLAG);
        }

        if(result != RAD_GAME_START_OK)
        {
            return result;
        }
        key = RAD_SubtreeEnd(t, key);
    }

    return (found == ALLE) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
}

///
/// Die Ausruestung: ein Array aus Objekten mit Namen. Geprueft und nicht behalten
/// (start_game.h) -- ihr Name hat deshalb auch keine Laengengrenze.
///
static RAD_GameStartResult_t RAD_ReadEquipment(const RAD_GameStartTokens_t *t, int32_t index)
{
    if(!RAD_IsType(t, index, JSMN_ARRAY))
    {
        return RAD_GAME_START_ERROR_SCHEMA;
    }

    int32_t item = index + 1;
    for(int32_t e=0;e < t->tokens[index].size; ++e)
    {
        if(!RAD_IsType(t, item, JSMN_OBJECT))
        {
            return RAD_GAME_START_ERROR_SCHEMA;
        }

        bool name_found = false;
        int32_t key = item + 1;
        for(int32_t i=0;i < t->tokens[item].size; ++i)
        {
            if(RAD_KeyIs(t, key, "name"))
            {
                if(RAD_StringLength(t, key + 1) < 1)
                {
                    return RAD_GAME_START_ERROR_SCHEMA;
                }
                name_found = true;
            }
            key = RAD_SubtreeEnd(t, key);
        }
        if(!name_found)
        {
            return RAD_GAME_START_ERROR_SCHEMA;
        }

        item = RAD_SubtreeEnd(t, item);
    }

    return RAD_GAME_START_OK;
}

///
/// Ein Name, der ins Spiel geht -- Einheitentyp, Profil, Waffe. Leer oder kein
/// String ist ein Schemafehler, zu lang fuer RAD_UNIT_NAME_MAX eine Grenze des
/// Spiels (RAD_GAME_START_ERROR_LIMIT).
///
static RAD_GameStartResult_t RAD_ReadName(const RAD_GameStartTokens_t *t, int32_t index, char out[RAD_UNIT_NAME_MAX])
{
    const int32_t length = RAD_StringLength(t, index);
    if(length < 1)
    {
        return RAD_GAME_START_ERROR_SCHEMA;
    }
    if(length >= RAD_UNIT_NAME_MAX)
    {
        return RAD_GAME_START_ERROR_LIMIT;
    }
    return RAD_CopyString(t, index, out, RAD_UNIT_NAME_MAX, 1) ? RAD_GAME_START_OK : RAD_GAME_START_ERROR_SCHEMA;
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
