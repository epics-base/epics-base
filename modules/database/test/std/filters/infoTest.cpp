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

static int fl_equals_array(short type, const db_field_log *pfl1, void *p2) {
    for (int i = 0; i < pfl1->no_elements; i++) {
        switch (type) {
        case DBR_DOUBLE:
            if (((epicsFloat64*)pfl1->u.r.field)[i] != ((epicsInt32*)p2)[i]) {
                testDiag("at index=%d: field log has %g, should be %d",
                         i, ((epicsFloat64*)pfl1->u.r.field)[i], ((epicsInt32*)p2)[i]);
                return 0;
            }
            break;
        case DBR_LONG:
            if (((epicsInt32*)pfl1->u.r.field)[i] != ((epicsInt32*)p2)[i]) {
                testDiag("at index=%d: field log has %d, should be %d",
                         i, ((epicsInt32*)pfl1->u.r.field)[i], ((epicsInt32*)p2)[i]);
                return 0;
            }
            break;
        case DBR_STRING:
            if (strtol(&((const char*)pfl1->u.r.field)[i*MAX_STRING_SIZE], NULL, 0) != ((epicsInt32*)p2)[i]) {
                testDiag("at index=%d: field log has '%s', should be '%d'",
                         i, &((const char*)pfl1->u.r.field)[i*MAX_STRING_SIZE], ((epicsInt32*)p2)[i]);
                return 0;
            }
            break;
        default:
            return 0;
        }
    }
    return 1;
}

static void createAndOpen(const char *chan, const char *json, const char *type, dbChannel**pch, short no) {
    ELLNODE *node;
    char name[80];

    strncpy(name, chan, sizeof(name)-1);
    strncat(name, json, sizeof(name)-strlen(name)-1);

    testOk(!!(*pch = dbChannelCreate(name)), "dbChannel with plugin arr %s created", type);
    testOk((ellCount(&(*pch)->filters) == no), "channel has %d filter(s) in filter list", no);

    testOk(!(dbChannelOpen(*pch)), "dbChannel with plugin arr opened");

    node = ellFirst(&(*pch)->pre_chain);
    (void) CONTAINER(node, chFilter, pre_node);
    testOk((ellCount(&(*pch)->pre_chain) == 0), "arr has no filter in pre chain");

    node = ellFirst(&(*pch)->post_chain);
    (void) CONTAINER(node, chFilter, post_node);
    testOk((ellCount(&(*pch)->post_chain) == no),
           "arr has %d filter(s) in post chain", no);
}

#define DEBUG(fmt, x) printf("DEBUG (%d): %s: " fmt "\n", __LINE__, #x, x)

MAIN(arrTest)
{
    dbChannel *pch;
    chFilter *filter;
    const chFilterPlugin *plug;
    char info[] = "info";
    ELLNODE *node;
    chPostEventFunc *cb_out = NULL;
    void *arg_out = NULL;
    db_field_log fl;
    db_field_log *pfl;
    dbEventCtx evtctx;
    DBENTRY dbentry;
    const char test_info_str[] = "xxxxxxxxxxxx";
    const char test_info_str_long[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    const char test_info_str_truncated[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

    testPlan(24);

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
    testOk(!!(pch = dbChannelCreate("x.{info:{name:\"a\"}}")), "dbChannel with plugin {info:{name:\"a\"}} successful");
    testOk((ellCount(&pch->filters) == 1), "Channel has one plugin");

    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin info opened");
    testOk(pch->final_type == DBF_STRING, "Channel type should be DBF_STRING");

    memset(&fl, 0, sizeof(fl));
    pfl = dbChannelRunPreChain(pch, &fl);
    testOk(pfl->field_type == DBF_STRING, "Field type is DBF_STRING");
    testOk(pfl->u.r.field && strcmp(test_info_str, (const char *)pfl->u.r.field) == 0, "Info string matches");

    dbChannelDelete(pch);

    testDiag("Testing long string");
    testOk(!!(pch = dbChannelCreate("x.{info:{name:\"a\",l:\"on\"}}")), "dbChannel requesting a long string successful");
    
    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin info opened");
    testOk(pch->final_type == DBF_CHAR, "Channel type should be DBF_CHAR");

    memset(&fl, 0, sizeof(fl));
    pfl = dbChannelRunPreChain(pch, &fl);
    testOk(pfl->field_type == DBF_CHAR, "Field type is DBF_CHAR");
    dbChannelDelete(pch);

    testDiag("Testing long string, full");

    testOk(!!(pch = dbChannelCreate("x.{info:{name:\"b\",l:\"auto\"}}")), "dbChannel requesting long info");
    testOk(!(dbChannelOpen(pch)), "dbChannelOpen with long string opened");

    memset(&fl, 0, sizeof(fl));
    pfl = dbChannelRunPreChain(pch, &fl);
    testOk(pfl->field_type == DBF_CHAR, "auto long string should be DBF_CHAR for long strings");
    testOk(pfl->u.r.field && strcmp(test_info_str_long, (const char *)pfl->u.r.field) == 0, "Info string matches");
    dbChannelDelete(pch);

    testDiag("Testing long string, truncated");

    testOk(!!(pch = dbChannelCreate("x.{info:{name:\"b\",l:\"off\"}}")), "dbChannel requesting long info");
    testOk(!(dbChannelOpen(pch)), "dbChannelOpen with long string opened");

    memset(&fl, 0, sizeof(fl));
    pfl = dbChannelRunPreChain(pch, &fl);
    testOk(pfl->field_type == DBF_STRING, "Field type should be DBF_STRING");
    testOk(pfl->u.r.field && strcmp(test_info_str_truncated, (const char *)pfl->u.r.field) == 0, "Info string matches");

    dbChannelDelete(pch);

    db_close_events(evtctx);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
