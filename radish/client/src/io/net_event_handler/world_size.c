#include "common.h"

#include <stdio.h>

void RAD_IoNetOnWorldSize(void *user_argument, uint32_t width, uint32_t height)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    RAD_ClientWorldSetSize(context->world, width, height);
    RAD_IoNetPrintDiscoverAnswerPrefix(context);
    printf("Spielfeld = %u x %u Felder\n", (unsigned)width, (unsigned)height);
}
