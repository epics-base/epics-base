#include <stdio.h>

#include <dbDefs.h>
#include <registryFunction.h>
#include <subRecord.h>
#define epicsExportSharedSymbols
#include "shareLib.h"

typedef long (*processMethod)(subRecord *precord);

static long mySubInit(subRecord *precord,processMethod process)
{
    printf("Record %s called mySubInit(%p, %p)\n",
	   precord->name, (void*) precord, (void*) process);
    return(0);
}

static long mySubProcess(subRecord *precord)
{
    printf("Record %s called mySubProcess(%p)\n",
	   precord->name, (void*) precord);
    return(0);
}

static registryFunctionRef mySubRef[] = {
    {"mySubInit",(REGISTRYFUNCTION)mySubInit},
    {"mySubProcess",(REGISTRYFUNCTION)mySubProcess}
};

epicsShareFunc void epicsShareAPI mySub(void)
{
    registryFunctionRefAdd(mySubRef,NELEMENTS(mySubRef));
}

