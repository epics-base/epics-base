
#include <memLib.h>

#define epicsExportSharedSymbols
#include "osiPoolStatus.h"

/*
 * osiSufficentSpaceInPool ()
 */
epicsShareFunc int epicsShareAPI osiSufficentSpaceInPool ()
{
    if (memFindMax () > 100000) {
        return 1;
    }
    else {
        return 0;
    }
}
