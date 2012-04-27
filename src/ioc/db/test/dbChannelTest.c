/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
 *     National Laboratory.
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 \*************************************************************************/

#include "dbChannel.h"
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "epicsUnitTest.h"
#include "testMain.h"

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
#define e_report        0x00004000
#define e_close         0x00008000

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
    testOk(e & e_string, "parse_string called, val = '%.*s'", stringLen,
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
    testOk(e & e_map_key, "parse_map_key called, key = '%.*s'", stringLen, key);
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
void c_report(chFilter *filter, int level)
{
    testOk(e & e_report, "channel_report called, level = %d", level);
}
void c_close(chFilter *filter)
{
    testOk(e & e_close, "channel_close called");
}

chFilterIf testIf =
    { p_start, p_abort, p_end, p_null, p_boolean, p_integer, p_double,
      p_string, p_start_map, p_map_key, p_end_map, p_start_array, p_end_array,
      c_open, c_report, c_close };

MAIN(dbChannelTest)
{
    dbChannel ch;

    testPlan(59);

    testOk1(!dbReadDatabase(&pdbbase, "dbChannelTest.dbx", ".:..", NULL));
    testOk(!!pdbbase, "pdbbase was set");

    r = e = 0;
    testOk1(!dbChannelFind(&ch, "x.NAME$"));
    testOk1(ch.addr.no_elements> 1);

    testOk1(!dbChannelFind(&ch, "x.{}"));

    testOk1(dbChannelFind(&ch, "y"));
    testOk1(dbChannelFind(&ch, "x.{\"none\":null}"));

    dbRegisterFilter("any", &testIf);

    e = e_start;
    testOk1(dbChannelFind(&ch, "x.{\"any\":null}"));

    r = e_start;
    e = e_start | e_null | e_abort;
    testOk1(dbChannelFind(&ch, "x.{\"any\":null}"));

    r = e_start | e_null;
    e = e_start | e_null | e_end;
    testOk1(dbChannelFind(&ch, "x.{\"any\":null}"));

    r = r_any;
    e = e_start | e_null | e_end;
    testOk1(!dbChannelFind(&ch, "x.{\"any\":null}"));
    e = e_close;
    testOk1(!dbChannelClose(&ch));

    dbRegisterFilter("scalar", &testIf);

    e = e_start | e_null | e_end;
    testOk1(!dbChannelFind(&ch, "x.{\"scalar\":null}"));

    e = e_report;
    dbChannelReport(&ch, 0);

    e = e_close;
    testOk1(!dbChannelClose(&ch));

    e = e_start | e_start_array | e_boolean | e_integer | e_end_array
            | e_end;
    testOk1(!dbChannelFind(&ch, "x.{\"any\":[true,1]}"));
    e = e_close;
    testOk1(!dbChannelClose(&ch));

    e = e_start | e_start_map | e_map_key | e_double | e_string | e_end_map
            | e_end;
    testOk1(!dbChannelFind(&ch, "x.{\"any\":{\"a\":2.7183,\"b\":\"c\"}}"));

    r = r_scalar;
    e = e_close | e_start | e_start_array | e_abort;
    testOk1(dbChannelFind(&ch, "x.{\"scalar\":[null]}"));
    e = 0;
    testOk1(!dbChannelClose(&ch));

    e = e_start | e_start_map | e_abort;
    testOk1(dbChannelFind(&ch, "x.{\"scalar\":{}}"));
    e = 0;
    testOk1(!dbChannelClose(&ch));

    dbFreeBase(pdbbase);

    return testDone();
}
