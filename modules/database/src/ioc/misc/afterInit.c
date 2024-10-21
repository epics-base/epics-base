/* Copyright (C) 2020 Dirk Zimoch */
/* Copyright (C) 2020-2024 European Spallation Source, ERIC */

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

// Version within the message
static const char helpMessage[] =
  "afterInit version 1.0.1\n"
  "Allows you to define commands to be run after the iocInit\n"
  "Example commands:\n"
  "  afterInit \"dbpf <PV> <VAL>\"\n"
  "  afterInit \"date\"\n";

struct cmditem
{
  struct cmditem *next;
  char *cmd;
};

static struct cmditem *cmdlist, **cmdlast = &cmdlist;

static void afterInitHook(initHookState state)
{
  if(state != initHookAfterIocRunning)
    return;

  struct cmditem *item = cmdlist;
  struct cmditem *next = NULL;
  while(item)
  {
    epicsStdoutPrintf("%s\n", item->cmd);

    if(iocshCmd(item->cmd))
      errlogPrintf("ERROR afterInit command '%s' failed to run\n", item->cmd);

    next = item->next;
    free(item->cmd);
    free(item);
    item = next;
  }
}

static struct cmditem *newItem(char *cmd)
{
  struct cmditem *item = malloc(sizeof(struct cmditem));

  if(item == NULL)
  {
    errno = ENOMEM;
    return NULL;
  }

  item->cmd = epicsStrDup(cmd);

  if(item->cmd == NULL)
  {
    free(item);
    errno = ENOMEM;
    return NULL;
  }

  item->next = NULL;
  *cmdlast = item;
  cmdlast = &item->next;
  return item;
}

static const iocshFuncDef afterInitDef = {
  "afterInit",
  1,
  (const iocshArg *[]){&(iocshArg){"command (before iocInit)", iocshArgString}},
  helpMessage};

static void afterInitFunc(const iocshArgBuf *args)
{
  static int first_time = 1;
  char *cmd = args[0].sval;

  if(interruptAccept)
  {
    errlogPrintf("WARNING afterInit can only be used before iocInit (check help)\n");
    return;
  }

  if(!cmd || !cmd[0])
  {
    errlogPrintf("WARNING afterInit received an empty argument (check help)\n");
    return;
  }

  if(first_time)
  {
    first_time = 0;
    if(initHookRegister(afterInitHook) < 0)
    {
      errno = ENOMEM;
      errlogPrintf("ERROR initHookRegister memory allocation failure %s\n", strerror(errno));
    }
  }

  struct cmditem *item = newItem(cmd);

  if(!item)
    errlogPrintf("ERROR afterInit failed to add the command '%s' %s\n", cmd, strerror(errno));
}

static void afterInitRegister(void)
{
  static int first_time = 1;
  if(first_time)
  {
    first_time = 0;
    iocshRegister(&afterInitDef, afterInitFunc);
  }
}

epicsExportRegistrar(afterInitRegister);
