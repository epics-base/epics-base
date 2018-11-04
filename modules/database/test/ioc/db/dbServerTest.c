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

#include <envDefs.h>
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
    ELLNODE_INIT, "one",
    oneReport, oneStats, oneClient,
    oneInit, oneRun, onePause, oneStop
};

dbServer one2 = {
    ELLNODE_INIT, "one",
    oneReport, oneStats, oneClient,
    oneInit, oneRun, onePause, oneStop
};

/* Server layer for testing NULL methods */

dbServer no_routines = {
    ELLNODE_INIT, "no-routines",
    NULL, NULL, NULL,
    NULL, NULL, NULL, NULL
};


/* Server layer which should be disabled */

int disInitialized = 0;

void disInit(void)
{
    disInitialized = 1;
}

dbServer disabled = {
    ELLNODE_INIT, "disabled",
    NULL, NULL, NULL,
    disInit, NULL, NULL, NULL
};

dbServer illegal = {
    ELLNODE_INIT, "bad name",
    NULL, NULL, NULL,
    disInit, NULL, NULL, NULL
};

dbServer toolate = {
    ELLNODE_INIT, "toolate",
    NULL, NULL, NULL,
    disInit, NULL, NULL, NULL
};


MAIN(dbServerTest)
{
    char name[16];
    char *theName = "The One";
    int status;

    testPlan(25);

    /* Prove that we handle substring names properly */
    epicsEnvSet("EPICS_IOC_IGNORE_SERVERS", "none ones");

    testDiag("Registering dbServer 'one'");
    testOk(dbRegisterServer(&one) == 0, "Registered 'one'");
    testOk1(oneState == NOTHING_CALLED);

    testOk(dbRegisterServer(&one) == 0, "Duplicate registration accepted");
    testOk(dbRegisterServer(&one2) != 0, "change registration rejected");
    testOk(dbRegisterServer(&illegal) != 0, "Illegal registration rejected");

    testDiag("Registering dbServer 'no-routines'");
    testOk(dbRegisterServer(&no_routines) == 0, "Registered 'no-routines'");
    testOk(dbUnregisterServer(&no_routines) == 0, "'no-routines' unreg'd");
    testOk(dbRegisterServer(&no_routines) == 0, "Re-registered 'no-routines'");

    epicsEnvSet("EPICS_IOC_IGNORE_SERVERS", "disabled nonexistent");
    testDiag("Registering dbServer 'disabled'");
    testOk(dbRegisterServer(&disabled) == 0, "Registration accepted");

    testDiag("Changing server state");
    dbInitServers();
    testOk(oneState == INIT_CALLED, "dbInitServers");
    testOk(disInitialized == 0, "Disabled server not initialized");
    testOk(dbRegisterServer(&toolate) != 0, "No registration while active");

    dbRunServers();
    testOk(oneState == RUN_CALLED, "dbRunServers");
    testOk(dbUnregisterServer(&one) != 0, "No unregistration while active");

    testDiag("Checking server methods called");
    dbsr(0);
    testOk(oneState == REPORT_CALLED, "dbsr called report()");

    oneSim = NULL;
    name[0] = 0;
    status = dbServerClient(name, sizeof(name));
    testOk(oneState == CLIENT_CALLED_UNKNOWN, "Client unknown");
    testOk(status == -1 && name[0] == 0,
        "dbServerClient mismatch");

    oneSim = theName;
    name[0] = 0;
    status = dbServerClient(name, sizeof(name));
    testOk(oneState == CLIENT_CALLED_KNOWN, "Client known");
    testOk(status == 0 && strcmp(name, theName) == 0,
        "dbServerClient match");

    dbPauseServers();
    testOk(oneState == PAUSE_CALLED, "dbPauseServers");

    dbsr(0);
    testOk(oneState != REPORT_CALLED, "No call to report() when paused");

    status = dbServerClient(name, sizeof(name));
    testOk(oneState != CLIENT_CALLED_KNOWN, "No call to client() when paused");

    dbStopServers();
    testOk(oneState == STOP_CALLED, "dbStopServers");
    testOk(dbUnregisterServer(&toolate) != 0, "No unreg' if not reg'ed");
    testOk(dbUnregisterServer(&no_routines) != 0, "No unreg' of 'no-routines'");

    return testDone();
}
