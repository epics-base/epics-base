/* Copyright (C) 2020 Dirk Zimoch */
/* Copyright (C) 2020-2025 European Spallation Source, ERIC */

#include <cantProceed.h>
#include <dbAccess.h>
#include <ellLib.h>
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
  "atInit version 2.0.1\n"
  "Allows you to define commands to be run after the iocInit\n"
  "Example commands:\n"
  "  atInit \"dbpf <PV> <VAL>\"\n"
  "  atInit \"date\"\n";

struct cmditem
{
  ELLNODE node;
  char* cmd;
};

static ELLLIST cmdlist;

static void atInitHook(initHookState state)
{
  if(state != initHookAfterIocRunning)
    return;

  struct cmditem* item = NULL;

  while(item = (struct cmditem*)ellGet(&cmdlist))
  {
    epicsStdoutPrintf("%s\n", item->cmd);

    if(iocshCmd(item->cmd))
      epicsStdoutPrintf("ERROR atInit command '%s' failed to run\n", item->cmd);

    free(item->cmd);
    free(item);
  }
}

static struct cmditem* newItem(char* cmd)
{
  struct cmditem* item = mallocMustSucceed(sizeof(struct cmditem) + strlen(cmd) + 1, "ERROR Failed to allocate memory for cmditem");
  item->cmd = (char*)(item + 1);
  strcpy(item->cmd, cmd);

  if(item->cmd == NULL)
  {
    free(item);
    errno = ENOMEM;
    return NULL;
  }

  ellAdd(&cmdlist, &item->node);

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

  if(interruptAccept)
  {
    epicsStdoutPrintf("WARNING atInit can only be used before iocInit (check help)\n");
    return;
  }

  if(!cmd || !cmd[0])
  {
    epicsStdoutPrintf("WARNING atInit received an empty argument (check help)\n");
    return;
  }

  if(first_time)
  {
    first_time = 0;
    if(initHookRegister(atInitHook) < 0)
    {
      errno = ENOMEM;
      epicsStdoutPrintf("ERROR initHookRegister memory allocation failure %s\n", strerror(errno));
    }
  }

  struct cmditem* item = newItem(cmd);

  if(!item)
    epicsStdoutPrintf("ERROR atInit failed to add the command '%s' %s\n", cmd, strerror(errno));
}

static void atInitRegister(void)
{
  static int first_time = 1;
  if(first_time)
  {
    first_time = 0;
    iocshRegister(&atInitDef, atInitFunc);
  }
}

epicsExportRegistrar(atInitRegister);
