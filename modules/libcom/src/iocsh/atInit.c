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

#include "atInit.h"

static const iocshArg atInitArg0 = {"command (before iocInit)", iocshArgString};
static const iocshArg *const atInitArgs[] = {&atInitArg0};
static const iocshFuncDef atInitDef = {
    "atInit",
    1,
    atInitArgs,
    "Allows you to define commands to be run after the iocInit\n"
    "Example commands:\n"
    "  atInit \"dbpf <PV> <VAL>\"\n"
    "  atInit \"date\"\n"};

struct cmditem {
    ELLNODE node;
    char *cmd;
};

static ELLLIST cmdList = ELLLIST_INIT;
static int initEndFlag = 0; // Defines the end of the initialization

static void atInitHook(const initHookState state)
{
    struct cmditem *item = NULL;

    if (state != initHookAfterIocRunning)
        return;

    while ((item = (struct cmditem *)ellGet(&cmdList))) {
        printf(ANSI_GREEN("atInit:") " %s\n", item->cmd);

        if (iocshCmd(item->cmd))
            printf(ERL_ERROR " atInit: "
                             "command '%s' failed to run\n",
                   item->cmd);

        free(item);
    }

    initEndFlag = 1;
}

static struct cmditem *newItem(const char *cmd)
{
    const size_t cmd_len = strnlen(cmd, 32768 - 1) + 1;
    struct cmditem *const item = mallocMustSucceed(sizeof(struct cmditem) + cmd_len, "atInit");
    item->cmd = (char *)(item + 1);
    memcpy(item->cmd, cmd, cmd_len);

    ellAdd(&cmdList, &item->node);

    return item;
}

static void atInitFunc(const iocshArgBuf *args)
{
    const char *const cmd = args[0].sval;

    if (initEndFlag) {
        printf(ERL_WARNING " atInit: "
                           "can only be used before 'iocInit'\n");
        return;
    }

    if (!cmd || !cmd[0]) {
        printf(ERL_ERROR " atInit: "
                         "received an empty 'command' argument\n");
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
