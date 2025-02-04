/* Copyright (C) 2020 Dirk Zimoch */
/* Copyright (C) 2020-2025 European Spallation Source, ERIC
 * Maintainer: Jerzy Jamroz
 */

#include <cantProceed.h>
#include <ellLib.h>
#include <epicsStdio.h>
#include <errlog.h>
#include <errno.h>
#include <initHooks.h>
#include <iocsh.h>
#include <string.h>

#include "atInit.h"

#define __AT_INIT_LOG(svr) svr " atInit: "

// Version within the message
static const char helpMessage[] =
  "atInit version 2.1.1\n"
  "Allows you to define commands to be run after the iocInit\n"
  "Example commands:\n"
  "  atInit \"dbpf <PV> <VAL>\"\n"
  "  atInit \"date\"\n";

struct cmditem
{
  ELLNODE node;
  char* cmd;
};

static ELLLIST s_cmdlist = {};
static int s_initendflag = 0; // Defines the end of the initialization

static void atInitHook(initHookState state)
{
  if(state != initHookAfterIocRunning)
    return;

  struct cmditem* item = NULL;

  while(item = (struct cmditem*)ellGet(&s_cmdlist))
  {
    epicsStdoutPrintf("%s\n", item->cmd);

    if(iocshCmd(item->cmd))
      epicsStdoutPrintf(__AT_INIT_LOG(ERL_ERROR) "command '%s' failed to run\n", item->cmd);

    free(item);
  }

  s_initendflag = 1;
}

static struct cmditem* newItem(char* cmd)
{
  struct cmditem* item = mallocMustSucceed(sizeof(struct cmditem) + strlen(cmd) + 1,
                                           __AT_INIT_LOG(ERL_ERROR) "failed to allocate memory for cmditem");
  item->cmd = (char*)(item + 1);
  strcpy(item->cmd, cmd);

  if(item->cmd == NULL)
  {
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

  if(s_initendflag)
  {
    epicsStdoutPrintf(__AT_INIT_LOG(ERL_WARNING) "can only be used before iocInit (check help)\n");
    return;
  }

  if(!cmd || !cmd[0])
  {
    epicsStdoutPrintf(__AT_INIT_LOG(ERL_WARNING) "received an empty argument (check help)\n");
    return;
  }

  if(first_time)
  {
    first_time = 0;
    if(initHookRegister(atInitHook) < 0)
    {
      errno = ENOMEM;
      epicsStdoutPrintf(__AT_INIT_LOG(ERL_ERROR) "initHookRegister memory allocation failure %s\n", strerror(errno));
    }
  }

  struct cmditem* item = newItem(cmd);

  if(!item)
    epicsStdoutPrintf(__AT_INIT_LOG(ERL_ERROR) "failed to add the command '%s' %s\n", cmd, strerror(errno));
}

void atInitRegister(void)
{
  static int first_time = 1;
  if(first_time)
  {
    first_time = 0;
    iocshRegister(&atInitDef, atInitFunc);
  }
}

#undef __AT_INIT_LOG
