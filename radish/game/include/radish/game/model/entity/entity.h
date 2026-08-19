#ifndef __RAD_GAME_ENTITY_H__
#define __RAD_GAME_ENTITY_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/model/model.h>
#include <radish/game/user.h>

///
/// Eine Entitaet, wie sie im Pool der Welt steht.
///
/// Oeffentlich, weil eine Figur aus dem Spiel herauskommt: RAD_OnEntitySpawned_t
/// und seine Nachbarn (event_manager.h) reichen ein const RAD_Entity_t* an ihre
/// Abonnenten, und die Serialisierer bilden jedes Feld darunter ab. Der Pool, in
/// dem sie steht, bleibt privat -- wer eine Figur setzt, bewegt oder entfernt,
/// geht ueber die Welt (world.h), denn nur sie haelt x/y und
/// RAD_Tile_t.entity synchron.
///
/// Ein Kommando braucht die Struktur dagegen nicht: es benennt eine Figur mit
/// ihrer RAD_EntityId_t (model.h), ohne in ihr zu lesen.
///
/// **Der Name steht hier und nicht in model.h**, wie bei Tile und aus demselben
/// Grund: dort stehen nur die Strukturen, deren Inhalt nicht heraus soll. Wer
/// RAD_Entity_t nennt, bindet diese Datei ein.
///
/// **Und mit dem Namen der Zugriff.** Unten stehen die vier Funktionen, mit denen
/// ein Aufrufer von aussen an die Figuren kommt -- alle Figuren des Spiels und die
/// eines einzelnen Benutzers. Sie heissen RAD_Game*, weil sie ein Spiel nehmen,
/// gehoeren aber zur Figur und stehen deshalb hier und nicht in game.h: wer Figuren
/// lesen will, braucht genau eine Datei dafuer.
///
typedef struct RAD_Entity RAD_Entity_t;

struct RAD_Entity
{
    ///
    /// Eigene Id, zugleich der Slot-Index im Pool. RAD_ENTITY_NONE markiert
    /// einen freien Slot.
    ///
    RAD_EntityId_t id;
    RAD_EntityType_t type;

    ///
    /// Wem die Entitaet gehoert, RAD_USER_NONE fuer herrenlos. Sie steht damit in
    /// der Entitaet und nicht in einer Liste je Benutzer -- die Id ist der
    /// Slot-Index und wird nach einer Loeschung wiederverwendet, und eine Liste
    /// daneben muesste bei jedem Entfernen mitgezogen werden. Wird sie das einmal
    /// nicht, erbt die naechste Figur in diesem Slot still den alten Besitzer.
    /// Hier kann das nicht passieren: RAD_WorldSpawnEntityWithId schreibt den
    /// ganzen Platz, RAD_WorldRemoveEntity raeumt ihn.
    ///
    /// Herrenlos ist der Normalfall und kein Fehler -- alles, was nicht
    /// ausdruecklich zugeordnet wurde, gehoert niemandem.
    ///
    RAD_UserId_t owner;

    ///
    /// Tile, auf dem die Entitaet steht. Immer synchron zu
    /// world->tiles[y][x].entity -- beide Seiten werden ausschliesslich von
    /// RAD_WorldSpawnEntity, RAD_WorldMoveEntity und RAD_WorldRemoveEntity
    /// fortgeschrieben.
    ///
    int16_t x;
    int16_t y;

    int16_t health;
    int16_t armor;
    struct 
    {
        uint32_t movable: 1;
        uint32_t collision: 1;
        uint32_t draw: 1;
    } attributes;
    struct
    {
        uint32_t burning: 1;
        uint32_t poisoned: 1;
    } state;
};

///
/// Die Figuren eines Spiels zum Nachlesen: erst zaehlen, dann einzeln holen.
/// Dasselbe Muster wie bei den Tiles (tile.h) und in turn.h.
///
/// **Der Index ist nicht die Id.** Der Pool der Welt hat Luecken -- eine entfernte
/// Figur laesst ihren Platz frei, damit die Ids der uebrigen sich nicht
/// verschieben. Gezaehlt und geholt wird dicht ab 0 ueber die belegten Plaetze,
/// aufsteigend nach Id. Wer die Id braucht, liest sie aus der Figur
/// (RAD_Entity_t.id); wer eine bestimmte sucht, laeuft ueber die Anzahl und
/// vergleicht. Ein Index gilt nur, solange keine Figur dazukommt oder wegfaellt --
/// danach kann derselbe Index eine andere treffen.
///
/// **Herausgegeben wird kopiert.** RAD_GameEntityAt schreibt einen Stand in den
/// Speicher des Aufrufers und gibt keinen Zeiger in die Welt heraus: ein Zeiger
/// liesse sich an den Regeln vorbei beschreiben und wuerde baumeln, sobald sich
/// die Welt weiterdreht. Die ausfuehrliche Begruendung steht in tile.h.
///

///
/// Wie viele Figuren es gibt -- nur die, die es wirklich gibt: ein freier Platz im
/// Pool ist keine Figur. 0 fuer ein NULL-Spiel.
///
int32_t RAD_GameNumberOfEntities(const RAD_Game_t *game);

///
/// Holt die Figur an dieser Stelle. Liefert false, wenn "game" oder "output" NULL
/// ist oder der Index ausserhalb [0, RAD_GameNumberOfEntities) liegt. **"output"
/// bleibt dann unangetastet** -- es wird nichts halb hineingeschrieben.
///
bool RAD_GameEntityAt(const RAD_Game_t *game, int32_t index, RAD_Entity_t *output);

///
/// Dieselben zwei Fragen, eingeschraenkt auf die Figuren eines Benutzers: erst
/// zaehlen, dann einzeln holen.
///
/// RAD_GameUserEntityAt liefert die **Id** und nicht die Figur -- anders als
/// RAD_GameEntityAt oben, und das ist gewachsen und nicht entworfen: es gab die
/// Funktion, bevor es einen Getter mit Ausgabezeiger gab. Wer die Figur selbst
/// will, holt sich mit der Id ihren Platz ueber die Anzahl. Sie liefert
/// RAD_ENTITY_NONE fuer einen Index ausserhalb
/// [0, RAD_GameNumberOfUserEntities).
///
/// Gezaehlt wird auch hier in der Reihenfolge der Ids, ein Index gilt also,
/// solange keine Figur dazukommt oder wegfaellt. Beides laeuft ueber den
/// Entitaetenpool -- bei hoechstens RAD_MAX_ENTITIES Plaetzen ist jede
/// Beschleunigung teurer als die Suche, und eine Liste daneben waere ein zweites
/// Buch (siehe RAD_Entity_t.owner oben).
///
int32_t RAD_GameNumberOfUserEntities(const RAD_Game_t *game, RAD_UserId_t user);
RAD_EntityId_t RAD_GameUserEntityAt(const RAD_Game_t *game, RAD_UserId_t user, int32_t index);


#endif
