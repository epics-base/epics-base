#include <stdio.h>
#include "subRecord.h"

typedef long (*processMethod)(subRecord *precord);

long mySubInit(subRecord *precord,processMethod process)
{
    printf("%s mySubInit process %p\n",precord->name, (void*) process);
    return(0);
}

long mySubProcess(subRecord *precord)
{
    printf("%s mySubProcess\n",precord->name);
    return(0);
}
