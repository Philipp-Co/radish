#ifndef __RAD_GAME_MODEL_H__
#define __RAD_GAME_MODEL_H__

#include <stdint.h>

///
/// Das Gesicht des Modells: seine Namen, und von seinem Inhalt nur das
/// Vokabular.
///
/// Der Spielzustand liegt privat (src/include/radish/game/model/) -- wer von
/// aussen an ihn will, geht durch die Fassade in game.h. Diese Datei ist, was
/// dafuer trotzdem oeffentlich sein muss, und sie zerfaellt in zwei Haelften:
///
/// **Das Vokabular.** Ids und Aufzaehlungen, die ein Kommando ueber die Strecke
/// traegt und deren Wire-Nummern der Codec kennt (control/command/codec.h). Sie
/// sind Protokoll und nicht Zustand: wer ein Kommando baut, muss sie hinschreiben
/// koennen, ohne eine Welt zu haben.
///
/// **Die Namen ohne Inhalt.** Drei unvollstaendige Typen -- RAD_World_t,
/// RAD_Turn_t und RAD_Game_t. Ein Zeiger darauf ist in C vollkommen zulaessig,
/// nur lesen laesst sich darin nicht, und mehr braucht die oeffentliche Seite von
/// ihnen nicht: game.h reicht RAD_Game_t* durch, ohne je ein Feld anzufassen.
///
/// Wer diese drei Strukturen wirklich braucht, uebersetzt innerhalb von
/// radish_game und bekommt sie ueber den privaten Suchpfad.
///
/// **Tile und Entitaet kommen hier nicht mehr vor, und zwar begruendet.** Ihre
/// Strukturen stehen vollstaendig im oeffentlichen Baum (model/tile/tile.h,
/// model/entity/entity.h) -- und mit ihnen ihre Namen. Der Grund sind die
/// Ereignisse: event_manager.h gibt seinen Abonnenten const RAD_Tile_t* und const
/// RAD_Entity_t* in die Hand, und ein Zeiger, den der Empfaenger nicht
/// dereferenzieren kann, ist kein Ereignis -- wer auf ein neues Tile hin zeichnen
/// soll, muss wissen, wo es liegt. Dasselbe verlangen die Serialisierer, die jedes
/// Feld beider Strukturen abbilden.
///
/// Wer eine dieser zwei braucht, bindet also ihren Header ein und nicht diesen.
/// Das ist der Unterschied zu den drei darunter: bei ihnen sind Name und Inhalt
/// getrennt, weil der Inhalt nicht heraus soll; bei Tile und Entitaet gibt es
/// nichts zu trennen.
///
/// Die Grenze verlaeuft damit nicht um das Modell herum, sondern durch es
/// hindurch, und sie folgt der Frage, was das Spiel nach draussen erzaehlt: ein
/// Tile und eine Figur sind das, was aus ihm herauskommt. Welt, Zug und Spiel
/// sind, was es fuehrt -- die kommen nie heraus, und deshalb bleiben sie Namen.
///

///
/// Handle auf eine Entitaet: der Slot-Index im Entity-Pool der Welt. Der Index
/// bleibt ueber die Lebensdauer der Entitaet stabil, anders als ein Zeiger kann
/// er aber nicht baumeln und laesst sich unveraendert uebertragen.
///
typedef int32_t RAD_EntityId_t;

#define RAD_ENTITY_NONE ((RAD_EntityId_t)-1)

typedef enum
{
    RAD_ENTITY_TYPE_NONE = 0,
    RAD_ENTITY_TYPE_PLAYER,
    RAD_ENTITY_TYPE_NPC
} RAD_EntityType_t;

typedef enum
{
    RAD_TILE_TYPE_VOID = 0,
    RAD_TILE_TYPE_GROUND,
    RAD_TILE_TYPE_WATER
} RAD_TileType_t;

///
/// Die drei Strukturen, die nur Namen bleiben: angekuendigt, definiert je einmal
/// hinter dem privaten Suchpfad (world/world.h, turn/turn.h, game.h unter model/).
///
/// **Hier steht nur, was unvollstaendig bleibt.** Wer seinen Namen und seinen
/// Inhalt an derselben Stelle fuehrt, fuehrt ihn dort -- RAD_Tile_t und
/// RAD_Entity_t stehen deshalb bei ihren Strukturen (model/tile/tile.h,
/// model/entity/entity.h) und nicht mehr hier. Ein Name ohne Inhalt braucht
/// dagegen eine Datei, in der er allein stehen kann, und das ist diese.
///
/// Das kostet einen Preis, und er ist sichtbar: wer RAD_Tile_t oder RAD_Entity_t
/// nennt, muss ihren Header einbinden und bekommt damit die ganze Struktur --
/// auch wenn er nur einen Zeiger weiterreicht. event_manager.h ist der Fall, an
/// dem sich das ablesen laesst. Ein zweiter typedef daneben waere der Ausweg und
/// ist keiner: in C99 ist er nicht erlaubt, und ein Name mit zwei Quellen ist
/// schlimmer als ein Header zu viel.
///
/// Mit Tag und getrenntem typedef, damit sie sich hier ankuendigen lassen: eine
/// namenlose Struktur laesst sich nicht vorwaerts deklarieren. Jeder typedef
/// steht genau einmal im Projekt -- diese drei hier, die anderen zwei bei ihrer
/// Struktur.
///
typedef struct RAD_World RAD_World_t;
typedef struct RAD_Turn RAD_Turn_t;
typedef struct RAD_Game RAD_Game_t;

#endif
