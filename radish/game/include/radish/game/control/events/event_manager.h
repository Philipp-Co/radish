#ifndef __RAD_CONTROL_EVENT_MANAGER_H__
#define __RAD_CONTROL_EVENT_MANAGER_H__

#include <radish/game/model/model.h>
#include <radish/game/model/tile/tile.h>
#include <radish/game/model/entity/entity.h>
#include <radish/game/model/path/path.h>

///
/// control/events/ -- was aus dem Spiel herauskommt.
///
/// Es liegt unter control/, weil dort steht, was mit dem Spielzustand geschieht:
/// control/command/ ist der Weg hinein -- eine Absicht in Datenform --, und
/// dieses hier der Weg hinaus. Beides zusammen ist die Grenze des Spiels nach
/// aussen; was dazwischen liegt (world.h, turn.h), sind die Regeln selbst und
/// kennt weder Absender noch Abonnenten.
///
/// Ein Abonnent je Gruppe, und die Gruppe wird als Ganzes gesetzt. Die
/// Voreinstellung sind Callbacks, die nichts tun -- deshalb prueft keine
/// Publish-Funktion auf NULL, und deshalb muss ein Abonnent alle Zeiger seiner
/// Gruppe fuellen.
///
/// **Was oeffentlich ist, sind die Signaturen -- nicht der Manager.** Wer
/// abonniert, muss beschreiben koennen, was er abonniert; wie die Gruppen
/// gehalten und verteilt werden, geht ihn nichts an. RAD_EventManager_t ist
/// deshalb ein unvollstaendiger Typ, wie RAD_Game_t und RAD_Control_t: die
/// Struktur steht in event_manager.c und sonst nirgends.
///
/// Das kostet die Anlegefunktion ihren Rueckgabewert: einen Manager gibt es nur
/// noch als Zeiger aus RAD_CreateEventManager, nicht mehr als Wert auf dem
/// Stapel. Wer ihn haelt, gibt ihn mit RAD_DestroyEventManager wieder her.
///
/// **Warum oben tile.h, entity.h und path.h stehen.** Die Signaturen hier nennen
/// RAD_Tile_t, RAD_Entity_t und RAD_EntityPath_t, und deren Namen stehen bei
/// ihren Strukturen und nicht in model.h (die Begruendung dort). Diese Datei
/// zieht damit alle drei Definitionen herein, und weil game.h sie einbindet,
/// bekommt sie jeder Konsument des Spielmoduls -- auch einer, der nie ein
/// Ereignis abonniert.
///
/// Das ist der bewusst gezahlte Preis dafuer, dass jeder dieser drei Namen genau
/// eine Quelle hat. Er ist klein, weil alle drei Strukturen klein sind und keine
/// weiteren Header nachziehen: tile.h kommt mit model.h aus, entity.h mit model.h
/// und user.h, path.h mit stdint allein. Ein zweiter typedef an dieser Stelle
/// waere der Ausweg gewesen -- in C99 ist er nicht erlaubt, und ein Name mit zwei
/// Quellen laeuft irgendwann auseinander.
///
/// **Der Pfad stand bis hierher in dieser Datei**, mit einem Define, das nach dem
/// Callback hiess, dem er gehoerte (RAD_ONENTITYMOVED_MAX_STEPS). Seit ein
/// Kommando eine Bewegung als Pfad traegt, gehoert er nicht mehr einem allein: er
/// steht in model/path/path.h, und diese Datei nennt ihn nur noch.
///

typedef void (*RAD_OnTileAddedToGame_t)(void *user_argument, const RAD_Tile_t *tile);
typedef void (*RAD_OnTileRemovedFromGame_t)(void *user_argument, const RAD_Tile_t *tile);
typedef void (*RAD_OnTileStateChanged_t)(void *user_argument, const RAD_Tile_t *tile);

typedef struct 
{
    void *user_argument;
    RAD_OnTileAddedToGame_t added;
    RAD_OnTileRemovedFromGame_t removed;
    RAD_OnTileStateChanged_t changed;
} RAD_EventsTileChangedCallback_t;

typedef void (*RAD_OnEntitySpawned_t)(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y);
typedef void (*RAD_OnEntityDestroyed_t)(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y);

typedef void (*RAD_OnEntityMoved_t)(void *user_argument, const RAD_Entity_t *entity, const RAD_EntityPath_t *path, int32_t result);

typedef struct
{
    int16_t x;
    int16_t y;
} RAD_EntityShoot_t;

typedef void (*RAD_OnEntityShoot_t)(void *user_argument, const RAD_Entity_t *entity, const RAD_EntityShoot_t *shoot);

typedef struct
{
    int16_t x;
    int16_t y; 
} RAD_Use_t;

typedef struct
{
    void *user_argument;
    RAD_OnEntitySpawned_t spawned;
    RAD_OnEntityDestroyed_t destroyed;
    RAD_OnEntityMoved_t moved;
} RAD_EventsEntityChangedCallback_t;

///
/// Die Zeigereignisse: was der Benutzer mit der Maus tut, nicht was im Spiel
/// geschieht. Sie melden keine Aenderung am Zustand, und deshalb veroeffentlicht
/// sie niemand aus dem Modell heraus -- der Client tut es aus seiner Schleife, und
/// das Rendering hoert zu.
///
/// Sie stehen trotzdem hier, weil der Manager die einzige Stelle ist, an der ein
/// Erzeuger und ein Abonnent sich finden, ohne einander zu kennen. Beim Umzug
/// dieser Datei von events/ nach control/events/ waren sie eine Weile nur noch in
/// event_manager.c erklaert; ein Name mit keiner Quelle ist schlimmer als einer
/// mit zwei.
///
typedef void (*RAD_OnMouseMove_t)(void *user_argument, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
typedef void (*RAD_OnMousePressed_t)(void *user_argument, int32_t x, int32_t y);
typedef void (*RAD_OnMouseReleased_t)(void *user_argument, int32_t x, int32_t y);

typedef struct 
{
    void *user_argument;
    RAD_OnMouseMove_t move;
    RAD_OnMousePressed_t pressed;
    RAD_OnMouseReleased_t released;
} RAD_EventsMouseCallbacks_t;

///
/// Der Manager selbst: nur ein Name. Was er haelt -- je eine Gruppe von oben --,
/// steht in event_manager.c.
///
typedef struct RAD_EventManager RAD_EventManager_t;

///
/// Legt einen Manager an, mit Callbacks, die nichts tun; NULL, wenn kein Speicher
/// da ist. Der einzige Weg an einen: die Struktur ist unvollstaendig, ein
/// Aufrufer kann sie weder anlegen noch ihre Groesse erfragen.
///
/// RAD_DestroyEventManager nullt den Zeiger des Aufrufers, wie RAD_DestroyGame.
/// Er muss laenger leben als alles, was ihn hinterlegt hat -- ein Spiel haelt ihn
/// geliehen und baut ihn nicht ab.
///
RAD_EventManager_t* RAD_CreateEventManager(void);
void RAD_DestroyEventManager(RAD_EventManager_t **manager);

void RAD_EventManagerSubscribeToTileEvents(RAD_EventManager_t *manager, RAD_EventsTileChangedCallback_t callbacks);
void RAD_EventManagerPublishTileAddedToGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);
void RAD_EventManagerPublishTileRemovedFromGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);
void RAD_EventManagerPublishTileStateChangeEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);

void RAD_EventManagerSubscribeToEntityEvents(RAD_EventManager_t *manager, RAD_EventsEntityChangedCallback_t callbacks);
void RAD_EventManagerPublishEntitySpawned(RAD_EventManager_t *manager, const RAD_Entity_t *entity, int32_t x, int32_t y);
void RAD_EventManagerPublishEntityDestroyed(RAD_EventManager_t *manager, const RAD_Entity_t *entity, int32_t x, int32_t y);
void RAD_EventManagerPublishEntityMoved(RAD_EventManager_t *manager, const RAD_Entity_t *entity, const RAD_EntityPath_t *path, int32_t result);

void RAD_EventManagerSubscribeToMouseEvents(RAD_EventManager_t *manager, RAD_EventsMouseCallbacks_t callbacks);
void RAD_EventManagerPublishMouseMoved(RAD_EventManager_t *manager, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
void RAD_EventManagerPublishMousePressed(RAD_EventManager_t *manager, int32_t x, int32_t y);
void RAD_EventManagerPublishMouseReleased(RAD_EventManager_t *manager, int32_t x, int32_t y);

#endif
