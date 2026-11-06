#include <radish/io/net_inbox.h>
#include <stdlib.h>
#include <string.h>


///
/// Eine Nachricht: Kopf und Bytes in einem Block, so dass Anlegen und Freigeben
/// je ein Aufruf sind.
///
typedef struct RAD_IoNetInboxEntry
{
    struct RAD_IoNetInboxEntry *next;
    size_t length;
    uint8_t data[];
} RAD_IoNetInboxEntry_t;

///
/// Eine einfach verkettete Liste: vorne wird abgeholt, hinten angehaengt.
/// "head" und "tail" sind beide NULL, solange sie leer ist.
///
struct RAD_IoNetInbox
{
    RAD_IoNetInboxEntry_t *head;
    RAD_IoNetInboxEntry_t *tail;
    size_t length;
};


RAD_IoNetInbox_t* RAD_CreateIoNetInbox(void)
{
    RAD_IoNetInbox_t *inbox = malloc(sizeof(struct RAD_IoNetInbox));
    if(inbox == NULL)
    {
        return NULL;
    }

    *inbox = (struct RAD_IoNetInbox){
        .head = NULL,
        .tail = NULL,
        .length = 0
    };

    return inbox;
}

void RAD_DestroyIoNetInbox(RAD_IoNetInbox_t **inbox)
{
    while(!RAD_IoNetInboxIsEmpty(*inbox))
    {
        RAD_IoNetInboxPop(*inbox);
    }

    free(*inbox);
    *inbox = NULL;
}

bool RAD_IoNetInboxPush(RAD_IoNetInbox_t *inbox, const uint8_t *data, size_t length)
{
    RAD_IoNetInboxEntry_t *entry = malloc(sizeof(RAD_IoNetInboxEntry_t) + length);
    if(entry == NULL)
    {
        return false;
    }

    entry->next = NULL;
    entry->length = length;
    if(length > 0)
    {
        memcpy(entry->data, data, length);
    }

    if(inbox->tail == NULL)
    {
        inbox->head = entry;
    }
    else
    {
        inbox->tail->next = entry;
    }
    inbox->tail = entry;
    inbox->length++;

    return true;
}

bool RAD_IoNetInboxIsEmpty(const RAD_IoNetInbox_t *inbox)
{
    return inbox->head == NULL;
}

size_t RAD_IoNetInboxLength(const RAD_IoNetInbox_t *inbox)
{
    return inbox->length;
}

const uint8_t* RAD_IoNetInboxFront(const RAD_IoNetInbox_t *inbox, size_t *length)
{
    if(inbox->head == NULL)
    {
        return NULL;
    }

    *length = inbox->head->length;
    return inbox->head->data;
}

void RAD_IoNetInboxPop(RAD_IoNetInbox_t *inbox)
{
    RAD_IoNetInboxEntry_t *entry = inbox->head;
    if(entry == NULL)
    {
        return;
    }

    inbox->head = entry->next;
    if(inbox->head == NULL)
    {
        inbox->tail = NULL;
    }
    inbox->length--;

    free(entry);
}
