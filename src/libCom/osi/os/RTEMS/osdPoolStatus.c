#include <rtems/libcsupport.h>

#define epicsExportSharedSymbols
#include "osiPoolStatus.h"

/*
 * osiSufficentSpaceInPool ()
 */
epicsShareFunc int epicsShareAPI osiSufficentSpaceInPool ()
{
    return (malloc_free_space() > 100000);
}
