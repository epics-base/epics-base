#include <stdio.h>
#include "subRecord.h"

typedef long (*processMethod)(subRecord *precord);

long mySubInit(subRecord *precord,processMethod process)
{
    printf("Record %s called mySubInit(%p, %p)\n",
	   precord->name, (void*) precord, (void*) process);
    return(0);
}

long mySubProcess(subRecord *precord)
{
    printf("Record %s called mySubProcess(%p)\n",
	   precord->name, (void*) precord);
    return(0);
}
