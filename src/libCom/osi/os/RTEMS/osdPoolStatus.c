#include <rtems/libcsupport.h>

#define epicsExportSharedSymbols
#include "osiPoolStatus.h"

/*
 * osiSufficentSpaceInPool ()
 */
epicsShareFunc int epicsShareAPI osiSufficentSpaceInPool ( size_t contiguousBlockSize )
{
    return (malloc_free_space() > 100000 + contiguousBlockSize);
}
