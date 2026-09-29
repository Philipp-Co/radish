#ifndef __RAD_ISO_ENTITY_H__
#define __RAD_ISO_ENTITY_H__

#include <radish/io/net_types.h>

///
/// Was die Darstellung von einer Figur weiss.
///
/// **Die Id sagt, welche Figur das hier ist.** Ohne sie waere ein Iso-Objekt nur
/// etwas, das auf einem Feld liegt, und wiederzufinden allein ueber dieses Feld --
/// also nur so lange, wie es stimmt.
///
/// Fuer eine Bewegung braucht es sie nicht mehr. Der Pfad traegt beide Enden,
/// sein erstes Feld ist der Standort (io/net_types.h): das Umhaengen ist ein
/// Zugriff, keine Suche. Zwei Gruende bleiben trotzdem:
///
/// **Als Rueckfall**, wenn das Startfeld nichts hergibt. Ein leeres Startfeld heisst
/// nicht, dass diese Map die Figur nicht hat -- sie kann ein frueheres Ereignis
/// verpasst haben. Dann wird sie an der Id gesucht.
///
/// **Fuers Aufraeumen**: ob auf einem Feld noch dieselbe Figur liegt, ist eine
/// Frage, die nur die Id beantwortet.
///
typedef struct
{
    RAD_NetEntityId_t id;
} RAD_IsoEntity_t;


#endif
