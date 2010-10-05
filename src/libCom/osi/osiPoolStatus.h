/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * $Revision-Id$
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

