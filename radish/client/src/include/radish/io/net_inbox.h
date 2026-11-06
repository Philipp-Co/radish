#ifndef __RAD_IO_NET_INBOX_H__
#define __RAD_IO_NET_INBOX_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///
/// io/net_inbox -- die Nachrichten vom Server, wie sie ankommen, in ihrer
/// Reihenfolge und noch nicht gelesen. Hineingelegt werden sie, sobald sie
/// eintreffen (RAD_NetEnqueueServerMessage, io/net_codec.h); abgeholt, wenn die
/// Anwendung so weit ist (RAD_NetDispatchServerMessage) -- etwa erst, wenn die
/// Darstellung abgespielt hat, was die vorige ausgeloest hat (view/view.h,
/// RAD_ViewIsBusy).
///
/// **Gespeichert werden Kopien der Bytes**: der Puffer des Aufrufers gilt nur,
/// solange RAD_OnMessageReceived laeuft (main.c, forwardToWasm).
///
/// **Verworfen wird nichts**, und eine Obergrenze gibt es nicht: eine fehlende
/// Nachricht liesse die Welt des Clients von der des Servers abweichen.
///

///
/// Die Inbox selbst: nur ein Name. Was sie haelt, steht in io/net_inbox.c.
///
typedef struct RAD_IoNetInbox RAD_IoNetInbox_t;

///
/// Legt eine leere Inbox an; NULL, wenn kein Speicher da ist.
/// RAD_DestroyIoNetInbox gibt alle Nachrichten frei, die noch darin liegen, und
/// nullt den Zeiger des Aufrufers.
///
RAD_IoNetInbox_t* RAD_CreateIoNetInbox(void);
void RAD_DestroyIoNetInbox(RAD_IoNetInbox_t **inbox);

///
/// Haengt eine Kopie der "length" Bytes ab "data" hinten an. false, wenn kein
/// Speicher da ist; dann bleibt die Inbox, wie sie war.
///
bool RAD_IoNetInboxPush(RAD_IoNetInbox_t *inbox, const uint8_t *data, size_t length);

bool RAD_IoNetInboxIsEmpty(const RAD_IoNetInbox_t *inbox);
size_t RAD_IoNetInboxLength(const RAD_IoNetInbox_t *inbox);

///
/// Die aelteste Nachricht, ihre Laenge in "length"; NULL, wenn die Inbox leer
/// ist -- "length" bleibt dann unberuehrt. Der Zeiger gilt bis zum naechsten
/// RAD_IoNetInboxPop.
///
const uint8_t* RAD_IoNetInboxFront(const RAD_IoNetInbox_t *inbox, size_t *length);

///
/// Nimmt die aelteste Nachricht heraus und gibt sie frei. Auf einer leeren
/// Inbox geschieht nichts.
///
void RAD_IoNetInboxPop(RAD_IoNetInbox_t *inbox);

#endif
