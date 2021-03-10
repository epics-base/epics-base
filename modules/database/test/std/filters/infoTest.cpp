/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2003 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to the Software License Agreement
* found in the file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

/* using stuff from softIoc.cpp by Andrew Johnson */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "registryFunction.h"
#include "epicsThread.h"
#include "epicsExit.h"
#include "epicsStdio.h"
#include "envDefs.h"
#include "dbStaticLib.h"
#include "dbmf.h"
#include "errlog.h"
#include "registry.h"
#include "dbAddr.h"
#include "dbAccess.h"
#include "asDbLib.h"
#include "iocInit.h"
#include "iocsh.h"
#include "dbChannel.h"
#include "epicsUnitTest.h"
#include "dbUnitTest.h"
#include "testMain.h"
#include "osiFileName.h"

extern "C" {
    void filterTest_registerRecordDeviceDriver(struct dbBase *);
}

#define CA_SERVER_PORT "65535"

const char *server_port = CA_SERVER_PORT;

#define DEBUG(fmt, x) printf("DEBUG (%d): %s: " fmt "\n", __LINE__, #x, x)

static void runSingleTest(const char *json, dbfType dbf_type, const char *type, const char *expected) {
    dbChannel *pch;
    db_field_log fl;
    db_field_log *pfl;
    char name[80] = "x.";

    strncat(name, json, sizeof(name)-strlen(name)-1);

    testOk(!!(pch = dbChannelCreate(name)), "dbChannel with plugin %s successful", name);
    testOk((ellCount(&pch->filters) == 1), "Channel has one plugin");

    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin info opened");
    testOk(pch->final_type == dbf_type, "Channel type should be %s", type);

    memset(&fl, 0, sizeof(fl));
    pfl = dbChannelRunPreChain(pch, &fl);
    testOk(pfl->field_type == dbf_type, "Field type should be %s", type);
    testOk(pfl->u.r.field && strcmp(expected, (const char *)pfl->u.r.field) == 0, "Info string matches expected '%s'", expected);

    dbChannelDelete(pch);
}

MAIN(arrTest)
{
    dbChannel *pch;
    const chFilterPlugin *plug;
    char info[] = "info";
    dbEventCtx evtctx;
    DBENTRY dbentry;
    const char test_info_str[] = "xxxxxxxxxxxx";
    const char test_info_str_long[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    const char test_info_str_truncated[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

    testPlan(30);

    /* Prepare the IOC */

    epicsEnvSet("EPICS_CA_SERVER_PORT", server_port);

    testdbPrepare();

    testdbReadDatabase("filterTest.dbd", NULL, NULL);

    filterTest_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    /* Start the IOC */

    evtctx = db_init_events();

    testOk(!!(plug = dbFindFilter(info, strlen(info))), "plugin 'info' registered correctly");


    testDiag("Testing failing info tag reads");
    testOk(!dbChannelCreate("x.{info:{}}"), "dbChannel with plugin {info:{}} failed");
    testOk(!dbChannelCreate("x.{info:{name:\"\"}}"), "dbChannel with plugin {info:{name:\"\"}} failed");
    testOk(!dbChannelCreate("x.{info:{name:\"0123456789a123456789b123456789c123456789d123456789e\"}}"), "dbChannel with plugin {info:{name:\"<too long name>\"}} failed");


    testDiag("Testing non-existent tags");
    testOk(!!(pch = dbChannelCreate("x.{info:{name:\"non-existent\"}}")), "dbChannel with plugin {info:{name:\"non-existent\"}} created");
    testOk(!!(dbChannelOpen(pch)), "dbChannel with non-existent info tag did not open");

    /*
     * Update the record to add the correct info record
     */
    dbInitEntry(pdbbase, &dbentry);
    dbFindRecord(&dbentry,"x");
    dbPutInfo(&dbentry, "a", test_info_str);
    dbPutInfo(&dbentry, "b", test_info_str_long);


    testDiag("Testing valid info tag");
    runSingleTest("{info:{name:\"a\"}}", DBF_STRING, "DBF_STRING", test_info_str);

    testDiag("Testing long string");
    runSingleTest("{info:{name:\"a\", l:\"on\"}}", DBF_CHAR, "DBF_CHAR", test_info_str);

    testDiag("Testing long string, full");
    runSingleTest("{info:{name:\"b\", l:\"auto\"}}", DBF_CHAR, "DBF_CHAR", test_info_str_long);

    testDiag("Testing long string, truncated");
    runSingleTest("{info:{name:\"b\", l:\"off\"}}", DBF_STRING, "DBF_STRING", test_info_str_truncated);

    db_close_events(evtctx);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
