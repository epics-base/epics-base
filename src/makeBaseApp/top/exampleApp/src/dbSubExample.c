#include <stdio.h>

#include <dbDefs.h>
#include <registryFunction.h>
#include <subRecord.h>
#include <epicsExport.h>

int mySubDebug;

typedef long (*processMethod)(subRecord *precord);

static long mySubInit(subRecord *precord,processMethod process)
{
    if (mySubDebug)
        printf("Record %s called mySubInit(%p, %p)\n",
               precord->name, (void*) precord, (void*) process);
    return(0);
}

static long mySubProcess(subRecord *precord)
{
    if (mySubDebug)
        printf("Record %s called mySubProcess(%p)\n",
               precord->name, (void*) precord);
    return(0);
}

/* Register these symbols for use by IOC code: */

epicsExportAddress(int, mySubDebug);
epicsRegisterFunction(mySubInit);
epicsRegisterFunction(mySubProcess);
