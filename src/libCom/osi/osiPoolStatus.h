
/*
 * $Id$
 *
 * Author: Jeff Hill
 *
 * Functions which interrogate the state of the system wide pool
 *
 */

#include <stdlib.h>
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * tests to see if there is sufficent space for a block of the requested size
 * along with whatever additional free space is necessary to keep the system running 
 * reliably
 *
 * this routine is called quite frequently so an efficent implementation is important
 */
epicsShareFunc int epicsShareAPI osiSufficentSpaceInPool ( size_t contiguousBlockSize );

#ifdef __cplusplus
}
#endif

#include "osdPoolStatus.h"

