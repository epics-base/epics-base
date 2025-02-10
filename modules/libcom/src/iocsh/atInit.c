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
#include <initHooks.h>
#include <iocsh.h>
#include <string.h>

#include "atInit.h"

static const char helpMessage[] =
    "Allows you to define commands to be run after the iocInit\n"
    "Example commands:\n"
    "  atInit \"dbpf <PV> <VAL>\"\n"
    "  atInit \"date\"\n";

struct cmditem {
    ELLNODE node;
    char *cmd;
};

static ELLLIST s_cmdlist = ELLLIST_INIT;
static int s_initendflag = 0; // Defines the end of the initialization

static void atInitHook(initHookState state)
{
    if (state != initHookAfterIocRunning)
        return;

    struct cmditem *item = NULL;

    while ((item = (struct cmditem *)ellGet(&s_cmdlist))) {
        printf("%s\n", item->cmd);

        if (iocshCmd(item->cmd))
            printf(ERL_ERROR " atInit: "
                             "command '%s' failed to run\n",
                   item->cmd);

        free(item);
    }

    s_initendflag = 1;
}

static struct cmditem *newItem(const char *cmd)
{
    const size_t cmd_len = strlen(cmd) + 1;

    struct cmditem *item = mallocMustSucceed(sizeof(struct cmditem) + cmd_len, "atInit");
    item->cmd = (char *)(item + 1);
    memcpy(item->cmd, cmd, cmd_len);

    ellAdd(&s_cmdlist, &item->node);

    return item;
}

static const iocshFuncDef atInitDef = {
    "atInit",
    1,
    (const iocshArg *[]){&(iocshArg){"command (before iocInit)", iocshArgString}},
    helpMessage};

static void atInitFunc(const iocshArgBuf *args)
{
    char *cmd = args[0].sval;

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

    newItem(cmd);
}

void atInitRegister(void)
{
    static int first_time = 1;
    if (first_time) {
        first_time = 0;
        iocshRegister(&atInitDef, atInitFunc);
        initHookRegister(atInitHook);
    }
}
