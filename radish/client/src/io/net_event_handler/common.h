#ifndef __RAD_IO_NET_EVENT_HANDLER_COMMON_H__
#define __RAD_IO_NET_EVENT_HANDLER_COMMON_H__

#include <radish/io/net_event_handler.h>

///
/// Was sich mehrere Handler in io/net_event_handler/ teilen. Nicht unter
/// include/: ausserhalb dieses Verzeichnisses braucht es niemand.
///

///
/// Der Anfang jeder Logzeile einer Discover-Antwort. Ohne eigene Anfrage steht
/// das dabei, statt einen Ausschnitt zu nennen, nach dem niemand gefragt hat.
///
void RAD_IoNetPrintDiscoverAnswerPrefix(const RAD_IoNetEventHandlerContext_t *context);

///
/// Die Attribute eines Feldes in einer Form, ohne Zeilenende -- fuer die
/// Discover-Antwort und die Tile-Ereignisse gleich, damit ein Feld im Log immer
/// gleich aussieht.
///
void RAD_IoNetPrintTile(const RAD_NetTile_t *tile);

///
/// Ein Feld, wie es der Server schickt, in Weltzustand und Darstellung.
///
void RAD_IoNetApplyTile(RAD_IoNetEventHandlerContext_t *context, const RAD_NetTile_t *tile);

#endif
