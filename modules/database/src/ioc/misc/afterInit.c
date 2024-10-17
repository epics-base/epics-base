/* Copyright (C) 2020 Dirk Zimoch */
/* Copyright (C) 2020-2023 European Spallation Source, ERIC */

#include <dbAccess.h>
#include <epicsExport.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <errlog.h>
#include <errno.h>
#include <initHooks.h>
#include <iocsh.h>
#include <stdlib.h>
#include <string.h>

struct cmditem {
  struct cmditem *next;
  char *cmd;
};

struct cmditem *cmdlist, **cmdlast = &cmdlist;

void afterInitHook(initHookState state) {
  if (state != initHookAfterIocRunning) return;

  struct cmditem *item = cmdlist;
  struct cmditem *next = NULL;
  while (item) {
    printf("%s\n", item->cmd);
    if (iocshCmd(item->cmd)) {
      errlogPrintf("afterInit: Command '%s' failed to run\n", item->cmd);
    };
    next = item->next;
    free(item->cmd);
    free(item);
    item = next;
  }
}

static struct cmditem *newItem(char *cmd) {
  struct cmditem *item;
  item = malloc(sizeof(struct cmditem));
  if (item == NULL) {
    return NULL;
  }
  item->cmd = epicsStrDup(cmd);
  item->next = NULL;

  *cmdlast = item;
  cmdlast = &item->next;
  return item;
}

static const iocshFuncDef afterInitDef = {
    "afterInit", 1,
    (const iocshArg *[]){
        &(iocshArg){"commandline", iocshArgString},
    }};

static void afterInitFunc(const iocshArgBuf *args) {
  static int first_time = 1;
  char *cmd;

  if (first_time) {
    first_time = 0;
    initHookRegister(afterInitHook);
  }
  if (interruptAccept) {
    errlogPrintf("afterInit can only be used before iocInit\n");
    return;
  }

  cmd = args[0].sval;
  if (!cmd || !cmd[0]) {
    errlogPrintf("Usage: afterInit \"command\"\n");
    return;
  }
  struct cmditem *item = newItem(cmd);

  if (!item)
    errlogPrintf("afterInit: error adding command %s; %s", cmd,
                 strerror(errno));
}

static void afterInitRegister(void) {
  static int firstTime = 1;
  if (firstTime) {
    firstTime = 0;
    iocshRegister(&afterInitDef, afterInitFunc);
  }
}
epicsExportRegistrar(afterInitRegister);
