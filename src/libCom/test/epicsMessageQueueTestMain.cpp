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
 *      $Id$
 *
 *      Author  W. Eric Norum
 *              norume@aps.anl.gov
 *              630 252 4793
 */

#include "epicsThread.h"

extern "C" void epicsMessageQueueTest ( void *);

int main ( int /* argc */, char /* *argv[] */ )
{
    epicsThreadCreate("messageQueueTest",epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        epicsMessageQueueTest,0);
    epicsThreadExitMain();
    return 0;
}
