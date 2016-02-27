/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2003 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to the Software License Agreement
* found in the file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Authors: Ralph Lange <ralph.lange@gmx.de>,
 *           Andrew Johnson <anj@aps.anl.gov>
 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "registryFunction.h"
#include "epicsThread.h"
#include "epicsExit.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "envDefs.h"
#include "dbStaticLib.h"
#include "dbmf.h"
#include "registry.h"
#include "dbAddr.h"
#include "dbAccess.h"
#include "asDbLib.h"
#include "iocInit.h"
#include "iocsh.h"
#include "dbChannel.h"
#include "epicsUnitTest.h"
#include "testMain.h"
#include "osiFileName.h"

extern "C" {
    int dbChArrTest_registerRecordDeviceDriver(struct dbBase *pdbbase);
}

#define CA_SERVER_PORT "65535"

const char *server_port = CA_SERVER_PORT;

static void createAndOpen(const char *name, dbChannel**pch)
{
    testOk(!!(*pch = dbChannelCreate(name)), "dbChannel %s created", name);
    testOk(!(dbChannelOpen(*pch)), "dbChannel opened");
    testOk((ellCount(&(*pch)->pre_chain) == 0), "no filters in pre chain");
    testOk((ellCount(&(*pch)->post_chain) == 0), "no filters in post chain");
}

extern "C" {
static void freeArray(db_field_log *pfl) {
    if (pfl->type == dbfl_type_ref) {
        free(pfl->u.r.field);
    }
}
}

static void testHead (const char *title, const char *typ = "") {
    const char *line = "------------------------------------------------------------------------------";
    testDiag("%s", line);
    testDiag(title, typ);
    testDiag("%s", line);
}

static void check(short dbr_type) {
    dbChannel *pch;
    db_field_log *pfl;
    dbAddr valaddr;
    dbAddr offaddr;
    const char *offname = NULL, *valname = NULL, *typname = NULL;
    epicsInt32 buf[26];
    long off, req;
    int i;

    switch (dbr_type) {
    case DBR_LONG:
        offname = "i32.OFF";
        valname = "i32.VAL";
        typname = "long";
        break;
    case DBR_DOUBLE:
        offname = "f64.OFF";
        valname = "f64.VAL";
        typname = "double";
        break;
    case DBR_STRING:
        offname = "c40.OFF";
        valname = "c40.VAL";
        typname = "string";
        break;
    default:
        testDiag("Invalid data type %d", dbr_type);
    }

    (void) dbNameToAddr(offname, &offaddr);
    (void) dbNameToAddr(valname, &valaddr);

    testHead("Ten %s elements", typname);

    /* Fill the record's array field with data, 10..19 */

    epicsInt32 ar[10] = {10,11,12,13,14,15,16,17,18,19};
    (void) dbPutField(&valaddr, DBR_LONG, ar, 10);

    /* Open a channel to it, make sure no filters present */

    createAndOpen(valname, &pch);
    testOk(pch->final_type == valaddr.field_type,
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type);
    testOk(pch->final_no_elements == valaddr.no_elements,
           "final no_elements unchanged (%ld->%ld)", valaddr.no_elements, pch->final_no_elements);

    /* TEST1 sets the record's OFF field, then requests 10 elements from the channel,
     * passing in a transparent db_field_log and converting the data to LONG on the way in.
     * It checks that it got back the expected data and the right number of elements.
     */

#define TEST1(Size, Offset, Text, Expected) \
    testDiag("Reading from offset = %d (%s)", Offset, Text); \
    off = Offset; req = 10; \
    memset(buf, 0, sizeof(buf)); \
    (void) dbPutField(&offaddr, DBR_LONG, &off, 1); \
    pfl = db_create_read_log(pch); \
    testOk(pfl && pfl->type == dbfl_type_rec, "Valid pfl, type = rec"); \
    testOk(!dbChannelGetField(pch, DBR_LONG, buf, NULL, &req, pfl), "Got Field value"); \
    testOk(req == Size, "Got %ld elements (expected %d)", req, Size); \
    if (!testOk(!memcmp(buf, Expected, sizeof(Expected)), "Data correct")) \
        for (i=0; i<Size; i++) \
            testDiag("Element %d expected %d got %d", i, Expected[i], buf[i]); \
    db_delete_field_log(pfl);

    const epicsInt32 res_10_0[] = {10,11,12,13,14,15,16,17,18,19};
    TEST1(10, 0, "no offset", res_10_0);

    const epicsInt32 res_10_4[] = {14,15,16,17,18,19,10,11,12,13};
    TEST1(10, 4, "wrapped", res_10_4);

    /* Partial array */

    testHead("Five %s elements", typname);
    off = 0;    /* Reset offset for writing the next buffer */
    (void) dbPutField(&offaddr, DBR_LONG, &off, 1);
    (void) dbPutField(&valaddr, DBR_LONG, &ar[5], 5);

    createAndOpen(valname, &pch);
    testOk(pch->final_type == valaddr.field_type,
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type);
    testOk(pch->final_no_elements == valaddr.no_elements,
           "final no_elements unchanged (%ld->%ld)", valaddr.no_elements, pch->final_no_elements);

    const epicsInt32 res_5_0[] = {15,16,17,18,19};
    TEST1(5, 0, "no offset", res_5_0);

    const epicsInt32 res_5_3[] = {18,19,15,16,17};
    TEST1(5, 3, "wrapped", res_5_3);

    /* TEST2 sets the record's OFF field, then requests 15 elements from the channel
     * but passes in a db_field_log with alternate data, converting that data to LONG.
     * It checks that it got back the expected data and the right number of elements.
     */

#define TEST2(Size, Offset, Text, Expected) \
    testDiag("Reading from offset = %d (%s)", Offset, Text); \
    off = Offset; req = 15; \
    memset(buf, 0, sizeof(buf)); \
    (void) dbPutField(&offaddr, DBR_LONG, &off, 1); \
    pfl = db_create_read_log(pch); \
    pfl->type = dbfl_type_ref; \
    pfl->field_type = DBF_CHAR; \
    pfl->field_size = 1; \
    pfl->no_elements = 26; \
    pfl->u.r.dtor = freeArray; \
    pfl->u.r.field = epicsStrDup("abcdefghijklmnopqrsstuvwxyz"); \
    testOk(!dbChannelGetField(pch, DBR_LONG, buf, NULL, &req, pfl), "Got Field value"); \
    testOk(req == Size, "Got %ld elements (expected %d)", req, Size); \
    if (!testOk(!memcmp(buf, Expected, sizeof(Expected)), "Data correct")) \
        for (i=0; i<Size; i++) \
            testDiag("Element %d expected '%c' got '%c'", i, Expected[i], buf[i]); \
    db_delete_field_log(pfl);

    testHead("Fifteen letters from field-log instead of %s", typname);

    const epicsInt32 res_15[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
        'm', 'n', 'o'};
    TEST2(15, 0, "no offset", res_15);
    TEST2(15, 10, "ignored", res_15);

    dbChannelDelete(pch);
}

static dbEventCtx evtctx;

extern "C" {
static void dbChArrTestCleanup(void* junk)
{
    dbFreeBase(pdbbase);
    registryFree();
    pdbbase=0;

    db_close_events(evtctx);

    dbmfFreeChunks();
}
}

MAIN(dbChArrTest)
{
    testPlan(102);

    /* Prepare the IOC */

    epicsEnvSet("EPICS_CA_SERVER_PORT", server_port);

    if (dbReadDatabase(&pdbbase, "dbChArrTest.dbd",
            "." OSI_PATH_LIST_SEPARATOR ".." OSI_PATH_LIST_SEPARATOR
            "../O.Common" OSI_PATH_LIST_SEPARATOR "O.Common", NULL))
        testAbort("Database description not loaded");

    dbChArrTest_registerRecordDeviceDriver(pdbbase);

    if (dbReadDatabase(&pdbbase, "dbChArrTest.db",
            "." OSI_PATH_LIST_SEPARATOR "..", NULL))
        testAbort("Test database not loaded");

    epicsAtExit(&dbChArrTestCleanup,NULL);

    /* Start the IOC */

    iocInit();
    evtctx = db_init_events();

    check(DBR_LONG);
    check(DBR_DOUBLE);
    check(DBR_STRING);

    return testDone();
}
