/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsOkToBlockTest.cpp */

/* Author:  Marty Kraimer Date:    09JUL2004*/

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "epicsThread.h"


typedef struct info {
    int  isOkToBlock;
}info;

extern "C" {

static void thread(void *arg)
{
    info *pinfo = (info *)arg;
    int isOkToBlock;

    printf("thread %s isOkToBlock %d\n",
        epicsThreadGetNameSelf(),pinfo->isOkToBlock);
    epicsThreadSetOkToBlock(pinfo->isOkToBlock);
    epicsThreadSleep(1.0);
    isOkToBlock = epicsThreadIsOkToBlock();
    printf("thread %s epicsThreadIsOkToBlock %d\n",
        epicsThreadGetNameSelf(),isOkToBlock);
    epicsThreadSleep(.1);
    free(pinfo);
}
void epicsOkToBlockTest(void)
{
    unsigned int stackSize;
    info *pinfo;

    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    pinfo = (info *)calloc(1,sizeof(info));
    pinfo->isOkToBlock = 0;
    epicsThreadCreate("threadA",50,stackSize,thread,pinfo);
    pinfo = (info *)calloc(1,sizeof(info));
    pinfo->isOkToBlock = 1;
    epicsThreadCreate("threadB",50,stackSize,thread,pinfo);
    epicsThreadSleep(2.0);
}
}
