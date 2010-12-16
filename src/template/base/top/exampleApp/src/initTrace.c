/* initTrace.c */

/*
 * An initHook routine to trace the iocInit() process.
 * Prints out the name of each state as it is reached.
 */

#include <stdio.h>

#include "initHooks.h"
#include "epicsExport.h"
#include "iocsh.h"


static void trace(initHookState state) {
    printf("iocInit: Reached %s\n", initHookName(state));
}

int traceIocInit(void) {
    static int done = 0;
    if (done)
        return -1;
    done = 1;

    initHookRegister(trace);
    puts("iocInit will be traced");
    return 0;
}


static const iocshFuncDef traceInitFuncDef = {"traceIocInit", 0, NULL};
static void traceInitFunc(const iocshArgBuf *args) {
    traceIocInit();
}

static void initTraceRegister(void) {
    iocshRegister(&traceInitFuncDef, traceInitFunc);
}
epicsExportRegistrar(initTraceRegister);
