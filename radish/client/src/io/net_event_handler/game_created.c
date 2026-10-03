#include <radish/io/net_event_handler.h>

#include <stdio.h>

void RAD_IoNetOnGameCreated(void *user_argument)
{
    (void)user_argument;
    printf("<- Spiel erstellt\n");
}
