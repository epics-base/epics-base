/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>
#include <math.h>

#include "fdmgr.h"
#include "epicsTime.h"
#include "epicsAssert.h"
#include "cadef.h"

#define verify(exp) ((exp) ? (void)0 : \
    epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

static const unsigned uSecPerSec = 1000000;

typedef struct cbStructCreateDestroyFD {
    fdctx *pfdm;
    int trig;
} cbStructCreateDestroyFD;

void fdHandler (void *pArg)
{
    cbStructCreateDestroyFD *pCBFD = (cbStructCreateDestroyFD *) pArg;

    printf ("triggered\n");
    pCBFD->trig = 1;
}

void fdCreateDestroyHandler (void *pArg, int fd, int open)
{
    cbStructCreateDestroyFD *pCBFD = (cbStructCreateDestroyFD *) pArg;
    int status;

    if (open) {
        printf ("new fd = %d\n", fd);
        status = fdmgr_add_callback (pCBFD->pfdm, fd, fdi_read, fdHandler, pArg);
        verify (status==0);
    }
    else {
        printf ("terminated fd = %d\n", fd);
        status = fdmgr_clear_callback (pCBFD->pfdm, fd, fdi_read);
        verify (status==0);
    }
}

typedef struct cbStuctTimer {
    epicsTimeStamp time;
    int done;
} cbStruct;

void alarmCB (void *parg)
{
    cbStruct *pCBS = (cbStruct *) parg;
    epicsTimeGetCurrent (&pCBS->time);
    pCBS->done = 1;
}

void testTimer (fdctx *pfdm, double delay)
{
    int status;
    fdmgrAlarmId aid;
    struct timeval tmo;
    epicsTimeStamp begin;
    cbStruct cbs;
    double measuredDelay;
    double measuredError;

    epicsTimeGetCurrent (&begin);
    cbs.done = 0;
    tmo.tv_sec = (unsigned long) delay;
    tmo.tv_usec = (unsigned long) ((delay - tmo.tv_sec) * uSecPerSec);
    aid = fdmgr_add_timeout (pfdm, &tmo, alarmCB, &cbs);
    verify (aid!=fdmgrNoAlarm);

    while (!cbs.done) {
        tmo.tv_sec = (unsigned long) delay;
        tmo.tv_usec = (unsigned long) ((delay - tmo.tv_sec) * uSecPerSec);
        status = fdmgr_pend_event (pfdm, &tmo);
        verify (status==0);
    }

    measuredDelay = epicsTimeDiffInSeconds (&cbs.time, &begin);
    measuredError = fabs (measuredDelay-delay);
    printf ("measured delay for %lf sec was off by %lf sec (%lf %%)\n", 
        delay, measuredError, 100.0*measuredError/delay);
}

int main (int argc, char **argv)
{
    int status;
    fdctx *pfdm;
    cbStructCreateDestroyFD cbsfd;
    struct timeval tmo;
    chid chan;

    pfdm = fdmgr_init ();
    verify (pfdm);

    SEVCHK (ca_task_initialize(), NULL);
    cbsfd.pfdm = pfdm;
    SEVCHK (ca_add_fd_registration (fdCreateDestroyHandler, &cbsfd), NULL);

    /*
     * timer test
     */
    testTimer (pfdm, 0.001);
    testTimer (pfdm, 0.01);
    testTimer (pfdm, 0.1);
    testTimer (pfdm, 1.0);

    if (argc==2) {
         SEVCHK(ca_search (argv[1], &chan), NULL);
    }

    while (1) {
        tmo.tv_sec = 0;
        tmo.tv_usec = 100000;
        cbsfd.trig = 0;
        status = fdmgr_pend_event (pfdm, &tmo);
        verify (status==0);
        ca_poll ();
    }

    status = fdmgr_delete (pfdm);
    verify (status==0);
    
    printf ( "Test Complete\n" );
    
    return 0;
}

