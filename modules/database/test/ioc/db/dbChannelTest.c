/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Andrew Johnson <anj@aps.anl.gov>
 *          Ralph Lange <Ralph.Lange@bessy.de>
 */

#include "dbChannel.h"
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "registry.h"
#include "recSup.h"
#include "dbUnitTest.h"
#include "testMain.h"
#include "osiFileName.h"
#include "errlog.h"

/* Expected call bit definitions */
#define e_start         0x00000001
#define e_abort         0x00000002
#define e_end           0x00000004
#define e_null          0x00000008
#define e_boolean       0x00000010
#define e_integer       0x00000020
#define e_double        0x00000040
#define e_string        0x00000080
#define e_start_map     0x00000100
#define e_map_key       0x00000200
#define e_end_map       0x00000400
#define e_start_array   0x00000800
#define e_end_array     0x00001000
#define e_open          0x00002000
#define e_reg_pre       0x00004000
#define e_reg_post      0x00008000
#define e_report        0x00010000
#define e_close         0x00020000

#define r_any (e_start | e_abort | e_end | \
        e_null | e_boolean | e_integer | e_double | e_string | \
        e_start_map | e_map_key | e_end_map | e_start_array | e_end_array)
#define r_scalar (e_start | e_abort | e_end | \
        e_null | e_boolean | e_integer | e_double | e_string)

unsigned int e, r;
#define p_ret(x) return r & x ? parse_continue : parse_stop

parse_result p_start(chFilter *filter)
{
    testOk(e & e_start, "parse_start called");
    p_ret(e_start);
}

void p_abort(chFilter *filter)
{
    testOk(e & e_abort, "parse_abort called");
}

parse_result p_end(chFilter *filter)
{
    testOk(e & e_end, "parse_end called");
    p_ret(e_end);
}

parse_result p_null(chFilter *filter)
{
    testOk(e & e_null, "parse_null called");
    p_ret(e_null);
}
parse_result p_boolean(chFilter *filter, int boolVal)
{
    testOk(e & e_boolean, "parse_boolean called, val = %d", boolVal);
    p_ret(e_boolean);
}
parse_result p_integer(chFilter *filter, long integerVal)
{
    testOk(e & e_integer, "parse_integer called, val = %ld", integerVal);
    p_ret(e_integer);
}
parse_result p_double(chFilter *filter, double doubleVal)
{
    testOk(e & e_double, "parse_double called, val = %g", doubleVal);
    p_ret(e_double);
}
parse_result p_string(chFilter *filter, const char *stringVal, size_t stringLen)
{
    testOk(e & e_string, "parse_string called, val = '%.*s'", (int) stringLen,
            stringVal);
    p_ret(e_string);
}

parse_result p_start_map(chFilter *filter)
{
    testOk(e & e_start_map, "parse_start_map called");
    p_ret(e_start_map);
}
parse_result p_map_key(chFilter *filter, const char *key, size_t stringLen)
{
    testOk(e & e_map_key, "parse_map_key called, key = '%.*s'", (int) stringLen, key);
    p_ret(e_map_key);
}
parse_result p_end_map(chFilter *filter)
{
    testOk(e & e_end_map, "parse_end_map called");
    p_ret(e_end_map);
}

parse_result p_start_array(chFilter *filter)
{
    testOk(e & e_start_array, "parse_start_array called");
    p_ret(e_start_array);
}
parse_result p_end_array(chFilter *filter)
{
    testOk(e & e_end_array, "parse_end_array called");
    p_ret(e_end_array);
}

long c_open(chFilter *filter)
{
    testOk(e & e_open, "channel_open called");
    return 0;
}
void c_reg_pre(chFilter *filter, chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    testOk(e & e_reg_pre, "channel_register_pre called");
}
void c_reg_post(chFilter *filter, chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    testOk(e & e_reg_post, "channel_register_post called");
}
void c_report(chFilter *filter, int level, const unsigned short indent)
{
    testOk(e & e_report, "channel_report called, level = %d", level);
}
void c_close(chFilter *filter)
{
    testOk(e & e_close, "channel_close called");
}

chFilterIf testIf =
    { NULL, p_start, p_abort, p_end, p_null, p_boolean, p_integer, p_double,
      p_string, p_start_map, p_map_key, p_end_map, p_start_array, p_end_array,
      c_open, c_reg_pre, c_reg_post, c_report, c_close };

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(testDbChannel)     /* dbChannelTest is an API routine... */
{
    dbChannel *pch;

    testPlan(76);

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    r = e = 0;
    /* dbChannelTest() checks record and field names */
    testOk1(!dbChannelTest("x.NAME"));
    testOk1(!dbChannelTest("x.INP"));
    testOk1(!dbChannelTest("x.VAL"));
    testOk1(!dbChannelTest("x."));
    testOk1(!dbChannelTest("x"));
    testOk(dbChannelTest("y"), "Test, nonexistent record");
    testOk(dbChannelTest("x.NOFIELD"), "Test, nonexistent field");

    /* dbChannelTest() allows but ignores field modifiers */
    testOk1(!dbChannelTest("x.NAME$"));
    testOk1(!dbChannelTest("x.{}"));
    testOk1(!dbChannelTest("x.VAL{\"json\":true}"));

    /* dbChannelCreate() accepts field modifiers */
    testOk1(!!(pch = dbChannelCreate("x.{}")));
    if (pch) dbChannelDelete(pch);
    testOk1(!!(pch = dbChannelCreate("x.VAL{}")));
    testOk1(pch && dbChannelElements(pch) == 1);
    if (pch) dbChannelDelete(pch);
    testOk1(!!(pch = dbChannelCreate("x.NAME$")));
    testOk1(pch && dbChannelFieldType(pch) == DBF_CHAR);
    testOk1(pch && dbChannelExportType(pch) == DBR_CHAR);
    testOk1(pch && dbChannelElements(pch) == PVNAME_STRINGSZ);
    if (pch) dbChannelDelete(pch);
    testOk1(!!(pch = dbChannelCreate("x.INP$")));
    testOk1(pch && dbChannelFieldType(pch) == DBF_INLINK);
    testOk1(pch && dbChannelExportType(pch) == DBR_CHAR);
    testOk1(pch && dbChannelElements(pch) > PVNAME_STRINGSZ);
    if (pch) dbChannelDelete(pch);
    testOk1(!!(pch = dbChannelCreate("x.NAME${}")));
    testOk1(pch && dbChannelFieldType(pch) == DBF_CHAR);
    testOk1(pch && dbChannelExportType(pch) == DBR_CHAR);
    testOk1(pch && dbChannelElements(pch) == PVNAME_STRINGSZ);
    if (pch) dbChannelDelete(pch);

    /* dbChannelCreate() rejects bad PVs */
    testOk(!dbChannelCreate("y"), "Create, bad record");
    testOk(!dbChannelCreate("x.NOFIELD"), "Create, bad field");
    testOk(!dbChannelCreate("x.{not-json}"), "Create, bad JSON");
    eltc(0);
    testOk(!dbChannelCreate("x.{\"none\":null}"), "Create, bad filter");
    eltc(1);

    dbRegisterFilter("any", &testIf, NULL);

    /* Parser event rejection by filter */
    e = e_start;
    testOk1(!dbChannelCreate("x.{\"any\":null}"));

    r = e_start;
    e = e_start | e_null | e_abort;
    testOk1(!dbChannelCreate("x.{\"any\":null}"));

    r = e_start | e_null;
    e = e_start | e_null | e_end;
    testOk1(!dbChannelCreate("x.{\"any\":null}"));

    /* Successful parsing... */
    r = r_any;
    e = e_start | e_null | e_end;
    testOk1(!!(pch = dbChannelCreate("x.{\"any\":null}")));
    e = e_close;
    if (pch) dbChannelDelete(pch);

    dbRegisterFilter("scalar", &testIf, NULL);

    e = e_start | e_null | e_end;
    testOk1(!!(pch = dbChannelCreate("x.{\"scalar\":null}")));

    e = e_report;
    dbChannelShow(pch, 2, 2);

    e = e_close;
    if (pch) dbChannelDelete(pch);

    e = e_start | e_start_array | e_boolean | e_integer | e_end_array
            | e_end;
    testOk1(!!(pch = dbChannelCreate("x.{\"any\":[true,1]}")));
    e = e_close;
    if (pch) dbChannelDelete(pch);

    e = e_start | e_start_map | e_map_key | e_double | e_string | e_end_map
            | e_end;
    testOk1(!!(pch = dbChannelCreate("x.{\"any\":{\"a\":2.7183,\"b\":\"c\"}}")));
    e = e_close;
    if (pch) dbChannelDelete(pch);

    /* More event rejection */
    r = r_scalar;
    e = e_start | e_start_array | e_abort;
    testOk1(!dbChannelCreate("x.{\"scalar\":[null]}"));

    e = e_start | e_start_map | e_abort;
    testOk1(!dbChannelCreate("x.{\"scalar\":{}}"));

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
