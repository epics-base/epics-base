#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <registryFunction.h>
#include <subRecord.h>

extern "C" {
typedef long (*processMethod)(subRecord *precord);
long mySubInit(subRecord *precord,processMethod process);
long mySubProcess(subRecord *precord);
void mySubRecordFunctionsRegister(void);
}

long mySubInit(subRecord *precord,processMethod process)
{
    printf("%s mySubInit process %p\n",precord->name,process);
    return(0);
}

long mySubProcess(subRecord *precord)
{
    printf("%s mySubProcess\n",precord->name);
    return(0);
}

void mySubRecordFunctionsRegister(void)
{

    if(!registryFunctionAdd("mySubInit",(REGISTRYFUNCTION)mySubInit))
      errlogPrintf("mySubRecordFunctionsRegister registryFunctionAdd failed\n");
    if(!registryFunctionAdd("mySubProcess",(REGISTRYFUNCTION)mySubProcess))
      errlogPrintf("mySubRecordFunctionsRegister registryFunctionAdd failed\n");
}

/*
 * Register commands on application startup
 */
class mySubRegister {
  public:
    mySubRegister() { mySubRecordFunctionsRegister(); }
};
static mySubRegister mySubRegisterObj;

