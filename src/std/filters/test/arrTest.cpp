/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2003 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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

#include "arrRecord.h"

extern "C" {
    void filterTest_registerRecordDeviceDriver(struct dbBase *);
}

#define CA_SERVER_PORT "65535"

#define PATTERN 0x55

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

static void testHead (const char *title, const char *typ = "") {
    const char *line = "------------------------------------------------------------------------------";
    testDiag("%s", line);
    testDiag(title, typ);
    testDiag("%s", line);
}

#define TEST1(Size, Offset, Incr, Text) \
    testDiag("Offset: %d (%s)", Offset, Text); \
    off = Offset; \
    (void) dbPutField(&offaddr, DBR_LONG, &off, 1); \
    pfl = db_create_read_log(pch); \
    testOk(pfl->type == dbfl_type_rec, "original field log has type rec"); \
    pfl2 = dbChannelRunPostChain(pch, pfl); \
    testOk(pfl2 == pfl, "call does not drop or replace field_log"); \
    testOk(pfl->type == dbfl_type_ref, "filtered field log has type ref"); \
    testOk(fl_equals_array(dbr_type, pfl2, ar##Size##_##Offset##_##Incr), "array data correct"); \
    db_delete_field_log(pfl);

static void check(short dbr_type) {
    dbChannel *pch;
    db_field_log *pfl, *pfl2;
    dbAddr valaddr;
    dbAddr offaddr;
    const char *offname = NULL, *valname = NULL, *typname = NULL;
    epicsInt32 ar[10] = {10,11,12,13,14,15,16,17,18,19};
    epicsInt32 *ar10_0_1 = ar;
    epicsInt32 ar10_4_1[10] = {14,15,16,17,18,19,10,11,12,13};
    epicsInt32 ar5_0_1[10] = {12,13,14,15,16};
    epicsInt32 ar5_3_1[10] = {15,16,17,18,19};
    epicsInt32 ar5_5_1[10] = {17,18,19,10,11};
    epicsInt32 ar5_9_1[10] = {11,12,13,14,15};
    epicsInt32 ar5_0_2[10] = {12,14,16};
    epicsInt32 ar5_3_2[10] = {15,17,19};
    epicsInt32 ar5_5_2[10] = {17,19,11};
    epicsInt32 ar5_9_2[10] = {11,13,15};
    epicsInt32 ar5_0_3[10] = {12,15};
    epicsInt32 ar5_3_3[10] = {15,18};
    epicsInt32 ar5_5_3[10] = {17,10};
    epicsInt32 ar5_9_3[10] = {11,14};
    epicsInt32 off = 0;

    switch (dbr_type) {
    case DBR_LONG:
        offname = "x.OFF";
        valname = "x.VAL";
        typname = "long";
        break;
    case DBR_DOUBLE:
        offname = "y.OFF";
        valname = "y.VAL";
        typname = "double";
        break;
    case DBR_STRING:
        offname = "z.OFF";
        valname = "z.VAL";
        typname = "string";
        break;
    default:
        testDiag("Invalid data type %d", dbr_type);
    }

    (void) dbNameToAddr(offname, &offaddr);

    (void) dbNameToAddr(valname, &valaddr);
    (void) dbPutField(&valaddr, DBR_LONG, ar, 10);

    /* Default: should not change anything */

    testHead("Ten %s elements from rec, increment 1, full size (default)", typname);
    createAndOpen(valname, "{\"arr\":{}}", "(default)", &pch, 1);
    testOk(pch->final_type == valaddr.field_type,
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type);
    testOk(pch->final_no_elements == valaddr.no_elements,
           "final no_elements unchanged (%ld->%ld)", valaddr.no_elements, pch->final_no_elements);
    TEST1(10, 0, 1, "no offset");
    TEST1(10, 4, 1, "wrapped");
    dbChannelDelete(pch);

    testHead("Ten %s elements from rec, increment 1, out-of-bound start parameter", typname);
    createAndOpen(valname, "{\"arr\":{\"s\":-500}}", "out-of-bound start", &pch, 1);
    testOk(pch->final_type == valaddr.field_type,
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type);
    testOk(pch->final_no_elements == valaddr.no_elements,
           "final no_elements unchanged (%ld->%ld)", valaddr.no_elements, pch->final_no_elements);
    TEST1(10, 4, 1, "wrapped");
    dbChannelDelete(pch);

    testHead("Ten %s elements from rec, increment 1, out-of-bound end parameter", typname);
    createAndOpen(valname, "{\"arr\":{\"e\":500}}", "out-of-bound end", &pch, 1);
    testOk(pch->final_type == valaddr.field_type,
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type);
    testOk(pch->final_no_elements == valaddr.no_elements,
           "final no_elements unchanged (%ld->%ld)", valaddr.no_elements, pch->final_no_elements);
    TEST1(10, 4, 1, "wrapped");
    dbChannelDelete(pch);

    testHead("Ten %s elements from rec, increment 1, zero increment parameter", typname);
    createAndOpen(valname, "{\"arr\":{\"i\":0}}", "zero increment", &pch, 1);
    testOk(pch->final_type == valaddr.field_type,
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type);
    testOk(pch->final_no_elements == valaddr.no_elements,
           "final no_elements unchanged (%ld->%ld)", valaddr.no_elements, pch->final_no_elements);
    TEST1(10, 4, 1, "wrapped");
    dbChannelDelete(pch);

    testHead("Ten %s elements from rec, increment 1, invalid increment parameter", typname);
    createAndOpen(valname, "{\"arr\":{\"i\":-30}}", "invalid increment", &pch, 1);
    testOk(pch->final_type == valaddr.field_type,
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type);
    testOk(pch->final_no_elements == valaddr.no_elements,
           "final no_elements unchanged (%ld->%ld)", valaddr.no_elements, pch->final_no_elements);
    TEST1(10, 4, 1, "wrapped");
    dbChannelDelete(pch);

#define TEST5(Incr, Left, Right, Type) \
    testHead("Five %s elements from rec, increment " #Incr ", " Type " addressing", typname); \
    createAndOpen(valname, "{\"arr\":{\"s\":" #Left ",\"e\":" #Right ",\"i\":" #Incr "}}", \
                  "(" #Left ":" #Incr ":" #Right ")", &pch, 1); \
    testOk(pch->final_type == valaddr.field_type, \
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type); \
    testOk(pch->final_no_elements == 4 / Incr + 1, \
           "final no_elements correct (%ld->%ld)", valaddr.no_elements, pch->final_no_elements); \
    TEST1(5, 0, Incr, "no offset"); \
    TEST1(5, 3, Incr, "from upper block"); \
    TEST1(5, 5, Incr, "wrapped"); \
    TEST1(5, 9, Incr, "from lower block"); \
    dbChannelDelete(pch);

    /* Contiguous block of 5 */

    TEST5(1,  2,  6, "regular");
    TEST5(1, -8,  6, "left side from-end");
    TEST5(1,  2, -4, "right side from-end");
    TEST5(1, -8, -4, "both sides from-end");

    /* 5 elements with increment 2 */

    TEST5(2,  2,  6, "regular");
    TEST5(2, -8,  6, "left side from-end");
    TEST5(2,  2, -4, "right side from-end");
    TEST5(2, -8, -4, "both sides from-end");

    /* 5 elements with increment 3 */

    TEST5(3,  2,  6, "regular");
    TEST5(3, -8,  6, "left side from-end");
    TEST5(3,  2, -4, "right side from-end");
    TEST5(3, -8, -4, "both sides from-end");

    /* From buffer (plugin chain) */

#define TEST5B(Incr, Left, Right, Type) \
    testHead("Five %s elements from buffer, increment " #Incr ", " Type " addressing", typname); \
    createAndOpen(valname, "{\"arr\":{},\"arr\":{\"s\":" #Left ",\"e\":" #Right ",\"i\":" #Incr "}}", \
                  "(" #Left ":" #Incr ":" #Right ")", &pch, 2); \
    testOk(pch->final_type == valaddr.field_type, \
           "final type unchanged (%d->%d)", valaddr.field_type, pch->final_type); \
    testOk(pch->final_no_elements == 4 / Incr + 1, \
           "final no_elements correct (%ld->%ld)", valaddr.no_elements, pch->final_no_elements); \
    TEST1(5, 0, Incr, "no offset"); \
    dbChannelDelete(pch);

    /* Contiguous block of 5 */

    TEST5B(1,  2,  6, "regular");
    TEST5B(1, -8,  6, "left side from-end");
    TEST5B(1,  2, -4, "right side from-end");
    TEST5B(1, -8, -4, "both sides from-end");

    /* 5 elements with increment 2 */

    TEST5B(2,  2,  6, "regular");
    TEST5B(2, -8,  6, "left side from-end");
    TEST5B(2,  2, -4, "right side from-end");
    TEST5B(2, -8, -4, "both sides from-end");

    /* 5 elements with increment 3 */

    TEST5B(3,  2,  6, "regular");
    TEST5B(3, -8,  6, "left side from-end");
    TEST5B(3,  2, -4, "right side from-end");
    TEST5B(3, -8, -4, "both sides from-end");
}

MAIN(arrTest)
{
    dbEventCtx evtctx;
    const chFilterPlugin *plug;
    char arr[] = "arr";

    testPlan(1402);

    /* Prepare the IOC */

    epicsEnvSet("EPICS_CA_SERVER_PORT", server_port);

    testdbPrepare();

    testdbReadDatabase("filterTest.dbd", NULL, NULL);

    filterTest_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("arrTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    /* Start the IOC */

    evtctx = db_init_events();

    testOk(!!(plug = dbFindFilter(arr, strlen(arr))), "plugin arr registered correctly");

    check(DBR_LONG);
    check(DBR_DOUBLE);
    check(DBR_STRING);

    db_close_events(evtctx);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
