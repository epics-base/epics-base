/*************************************************************************\
* Copyright (C) 2016-2020 Dirk Zimoch
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

#include "afterIocRunning.h"

static const iocshArg afterIocRunningArg0 = {"command (before iocInit)", iocshArgString};
static const iocshArg *const afterIocRunningArgs[] = {&afterIocRunningArg0};
static const iocshFuncDef afterIocRunningDef = {
    "afterIocRunning",
    1,
    afterIocRunningArgs,
    "Allows you to define commands to be run after the iocInit\n"
    "Example commands:\n"
    "  afterIocRunning \"dbpf <PV> <VAL>\"\n"
    "  afterIocRunning \"date\"\n"
    "  afterIocRunning \"dbpf $(P)EvtClkSource-Sel 'Upstream (fanout)'\"\n"
    "  afterIocRunning \"dbpf $(P)Enable-Sel Enabled\"\n"};

struct cmditem {
    ELLNODE node;
    char *cmd;
};

static ELLLIST cmdList = ELLLIST_INIT;
static int initEndFlag = 0; // Defines the end of the initialization

static void afterIocRunningHook(const initHookState state)
{
    struct cmditem *item = NULL;

    if (state != initHookAfterIocRunning)
        return;

    while ((item = (struct cmditem *)ellGet(&cmdList))) {
        printf(ANSI_GREEN("afterIocRunning:") " %s\n", item->cmd);

        if (iocshCmd(item->cmd))
            printf(ERL_ERROR " afterIocRunning: "
                             "command '%s' failed to run\n",
                   item->cmd);

        free(item);
    }

    initEndFlag = 1;
}

static struct cmditem *newItem(const char *cmd)
{
    const size_t cmd_len = strlen(cmd) + 1;
    struct cmditem *const item = mallocMustSucceed(sizeof(struct cmditem) + cmd_len, "afterIocRunning");
    item->cmd = (char *)(item + 1);
    memcpy(item->cmd, cmd, cmd_len);

    ellAdd(&cmdList, &item->node);

    return item;
}

static void afterIocRunningFunc(const iocshArgBuf *args)
{
    const char *const cmd = args[0].sval;

    if (initEndFlag) {
        printf(ERL_WARNING " afterIocRunning: "
                           "can only be used before 'iocInit'\n");
        return;
    }

    if (!cmd || !cmd[0]) {
        printf(ERL_ERROR " afterIocRunning: "
                         "received an empty 'command' argument\n");
        return;
    }

    newItem(cmd);
}

void afterIocRunningRegister(void)
{
    static int first_time = 1;
    if (first_time) {
        first_time = 0;
        iocshRegister(&afterIocRunningDef, afterIocRunningFunc);
        initHookRegister(afterIocRunningHook);
    }
}
