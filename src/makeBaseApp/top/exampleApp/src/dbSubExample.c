#include <stdio.h>

#include <dbDefs.h>
#include <registryFunction.h>
#include <subRecord.h>
#include <epicsExport.h>

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

void mySub(void)
{
    registryFunctionRefAdd(mySubRef,NELEMENTS(mySubRef));
}
epicsExportRegistrar(mySub);

