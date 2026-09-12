#ifndef __RAD_ISO_ENTITY_H__
#define __RAD_ISO_ENTITY_H__

#include <radish/game/model/model.h>

///
/// Was die Darstellung von einer Figur weiss.
///
/// **Die Id sagt, welche Figur das hier ist.** Ohne sie waere ein Iso-Objekt nur
/// etwas, das auf einem Feld liegt, und wiederzufinden allein ueber dieses Feld --
/// also nur so lange, wie es stimmt.
///
/// Fuer eine Bewegung braucht es sie nicht mehr. Der Pfad im Ereignis traegt beide
/// Enden, seit sein erstes Feld der Standort ist (model/path/path.h): das
/// Umhaengen ist ein Zugriff, keine Suche. Zwei Gruende bleiben trotzdem:
///
/// **Als Rueckfall**, wenn das Startfeld nichts hergibt. Ein leeres Startfeld heisst
/// nicht, dass diese Map die Figur nicht hat -- sie kann ein frueheres Ereignis
/// verpasst haben, etwa weil sie erst nach dem Laden eines Spielstands abonniert
/// hat. Dann wird sie an der Id gesucht.
///
/// **Fuers Aufraeumen**, denn RAD_OnEntityDestroyed_t nennt eine Figur und ein Feld,
/// und ob auf diesem Feld noch dieselbe liegt, ist eine Frage, die nur die Id
/// beantwortet.
///
typedef struct
{
    RAD_EntityId_t id;
} RAD_IsoEntity_t;


#endif
