/*************************************************************************\
* Copyright (C) 2020 Dirk Zimoch
* Copyright (C) 2020-2025 European Spallation Source, ERIC
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <cantProceed.h>
#include <ellLib.h>
#include <errlog.h>
#include <errno.h>
#include <initHooks.h>
#include <iocsh.h>
#include <string.h>

#include "atInit.h"

// Version within the message
static const char helpMessage[] =
    "Allows you to define commands to be run after the iocInit\n"
    "Example commands:\n"
    "  atInit \"dbpf <PV> <VAL>\"\n"
    "  atInit \"date\"\n";

struct cmditem {
    ELLNODE node;
    char* cmd;
};

static ELLLIST s_cmdlist = {};
static int s_initendflag = 0; // Defines the end of the initialization

static void atInitHook(initHookState state)
{
    if (state != initHookAfterIocRunning)
        return;

    struct cmditem* item = NULL;

    while (item = (struct cmditem*)ellGet(&s_cmdlist)) {
        printf("%s\n", item->cmd);

        if (iocshCmd(item->cmd))
            printf(ERL_ERROR " atInit: "
                             "command '%s' failed to run\n",
                   item->cmd);

        free(item);
    }

    s_initendflag = 1;
}

static struct cmditem* newItem(char* cmd)
{
    struct cmditem* item = mallocMustSucceed(sizeof(struct cmditem) + strlen(cmd) + 1,
                                             ERL_ERROR " atInit: "
                                                       "failed to allocate memory for cmditem");
    item->cmd = (char*)(item + 1);
    memcpy(item->cmd, cmd, strlen(cmd) + 1);

    if (item->cmd == NULL) {
        free(item);
        errno = ENOMEM;
        return NULL;
    }

    ellAdd(&s_cmdlist, &item->node);

    return item;
}

static const iocshFuncDef atInitDef = {
    "atInit",
    1,
    (const iocshArg*[]){&(iocshArg){"command (before iocInit)", iocshArgString}},
    helpMessage};

static void atInitFunc(const iocshArgBuf* args)
{
    static int first_time = 1;
    char* cmd = args[0].sval;

    if (s_initendflag) {
        printf(ERL_WARNING " atInit: "
                           "can only be used before iocInit (check help)\n");
        return;
    }

    if (!cmd || !cmd[0]) {
        printf(ERL_WARNING " atInit: "
                           "received an empty argument (check help)\n");
        return;
    }

    if (first_time) {
        first_time = 0;
        if (initHookRegister(atInitHook) < 0) {
            errno = ENOMEM;
            printf(ERL_ERROR " atInit: "
                             "initHookRegister memory allocation failure %s\n",
                   strerror(errno));
        }
    }

    struct cmditem* item = newItem(cmd);

    if (!item)
        printf(ERL_ERROR " atInit: "
                         "failed to add the command '%s' %s\n",
               cmd, strerror(errno));
}

void atInitRegister(void)
{
    static int first_time = 1;
    if (first_time) {
        first_time = 0;
        iocshRegister(&atInitDef, atInitFunc);
    }
}
