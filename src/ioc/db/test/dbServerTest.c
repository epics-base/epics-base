/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Andrew Johnson <anj@aps.anl.gov>
 */

#include <string.h>

#include "dbServer.h"

#include <epicsUnitTest.h>
#include <testMain.h>

enum {
    NOTHING_CALLED,
    REPORT_CALLED,
    CLIENT_CALLED_UNKNOWN,
    CLIENT_CALLED_KNOWN,
    STATS_CALLED,
    INIT_CALLED,
    RUN_CALLED,
    PAUSE_CALLED,
    STOP_CALLED
} oneState;

char *oneSim;

void oneReport(unsigned level)
{
    oneState = REPORT_CALLED;
}

void oneStats(unsigned *channels, unsigned *clients)
{
    oneState = STATS_CALLED;
}

int oneClient(char *pbuf, size_t len)
{
    if (oneSim) {
        strncpy(pbuf, oneSim, len);
        oneState = CLIENT_CALLED_KNOWN;
        oneSim = NULL;
        return 0;
    }
    oneState = CLIENT_CALLED_UNKNOWN;
    return -1;
}

void oneInit(void)
{
    oneState = INIT_CALLED;
}

void oneRun(void)
{
    oneState = RUN_CALLED;
}

void onePause(void)
{
    oneState = PAUSE_CALLED;
}

void oneStop(void)
{
    oneState = STOP_CALLED;
}

dbServer one = {
    ELLNODE_INIT,
    "one",
    oneReport, oneStats, oneClient,
    oneInit, oneRun, onePause, oneStop
};

dbServer no_routines = {
    ELLNODE_INIT,
    "no-routines",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

MAIN(dbServerTest)
{
    char name[16];
    char *theName = "The One";
    int status;

    testPlan(0);

    testDiag("Registering dbServer 'one'");
    dbRegisterServer(&one);
    testOk1(oneState == NOTHING_CALLED);

    testDiag("Registering dbServer 'no-routines'");
    dbRegisterServer(&no_routines);

    dbInitServers();
    testOk(oneState == INIT_CALLED, "dbInitServers");

    dbRunServers();
    testOk(oneState == RUN_CALLED, "dbRunServers");

    dbPauseServers();
    testOk(oneState == PAUSE_CALLED, "dbPauseServers");

    dbStopServers();
    testOk(oneState == STOP_CALLED, "dbStopServers");

    dbsr(0);
    testOk(oneState == REPORT_CALLED, "dbsr");

    oneSim = NULL;
    name[0] = 0;
    status = dbServerClient(name, sizeof(name));
    testOk(status == -1 && name[0] == 0,
        "dbServerClient mismatch");

    oneSim = theName;
    name[0] = 0;
    status = dbServerClient(name, sizeof(name));
    testOk(status == 0 && strcmp(name, theName) == 0,
        "dbServerClient match");

    return testDone();
}
