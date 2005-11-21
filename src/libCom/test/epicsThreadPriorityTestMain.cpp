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
 *      Author  Marty Kraimer
 */

#include "epicsThread.h"

extern "C" void epicsThreadPriorityTest(void *arg);

int main ( int argc , char  *argv[] )
{
    epicsThreadCreate("threadPriorityTest",epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        epicsThreadPriorityTest,0);
    epicsThreadExitMain();
    return 0;
}
