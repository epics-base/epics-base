/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
 *     National Laboratory.
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 \*************************************************************************/

#include "dbChannel.h"
#include "epicsUnitTest.h"
#include "testMain.h"

/* Expected call bit definitions */
#define e_init          0x00000001
#define e_exit          0x00000002
#define e_start         0x00000004
#define e_abort         0x00000008
#define e_end           0x00000010
#define e_null          0x00000020
#define e_boolean       0x00000040
#define e_integer       0x00000080
#define e_double        0x00000100
#define e_string        0x00000200
#define e_start_map     0x00000400
#define e_map_key       0x00000800
#define e_end_map       0x00001000
#define e_start_array   0x00002000
#define e_end_array     0x00004000
#define e_open          0x00008000
#define e_report        0x00010000
#define e_close         0x00020000

unsigned int e;


void f_init(void)
{
    testOk(e & e_init, "plugin_init expected");
}
void f_exit(void)
{
    testOk(e & e_exit, "plugin_exit expected");
}

int p_start(chFilter *filter)
{
    testOk(e & e_start, "parse_start expected");
    return parse_continue;
}

void p_abort(chFilter *filter)
{
    testOk(e & e_abort, "parse_abort expected");
}
int p_end(chFilter *filter)
{
    testOk(e & e_end, "parse_end expected");
    return parse_continue;
}

int p_null(chFilter *filter)
{
    testOk(e & e_null, "parse_null expected");
    return parse_continue;
}
int p_boolean(chFilter *filter, int boolVal)
{
    testOk(e & e_boolean, "parse_boolean expected, val = %d", boolVal);
    return parse_continue;
}
int p_integer(chFilter *filter, long integerVal)
{
    testOk(e & e_integer, "parse_integer expected, val = %ld", integerVal);
    return parse_continue;
}
int p_double(chFilter *filter, double doubleVal)
{
    testOk(e & e_double, "parse_double expected, val = %g", doubleVal);
    return parse_continue;
}
int p_string(chFilter *filter, const char *stringVal, size_t stringLen)
{
    testOk(e & e_string, "parse_string expected, val = '%.*s'", stringLen, stringVal);
    return parse_continue;
}

int p_start_map(chFilter *filter)
{
    testOk(e & e_start_map, "parse_start_map expected");
    return parse_continue;
}
int p_map_key(chFilter *filter, const char *key, size_t stringLen)
{
    testOk(e & e_map_key, "parse_map_key expected, key = '%.*s'", stringLen, key);
    return parse_continue;
}
int p_end_map(chFilter *filter)
{
    testOk(e & e_end_map, "parse_end_map expected");
    return parse_continue;
}

int p_start_array(chFilter *filter)
{
    testOk(e & e_start_array, "parse_start_array expected");
    return parse_continue;
}
int p_end_array(chFilter *filter)
{
    testOk(e & e_end_array, "parse_end_array expected");
    return parse_continue;
}

long c_open(chFilter *filter)
{
    testOk(e & e_open, "channel_open expected");
    return 0;
}
void c_report(chFilter *filter, int level)
{
    testOk(e & e_report, "channel_report expected, level = %d", level);
}
void c_close(chFilter *filter)
{
    testOk(e & e_close, "channel_close expected");
}

chFilterIf testIf =
    { f_init, f_exit, p_start, p_abort, p_end, p_null, p_boolean,
      p_integer, p_double, p_string, p_start_map, p_map_key, p_end_map,
      p_start_array, p_end_array, c_open, c_report, c_close };

chFilterIf scalarIf =
    { f_init, f_exit, p_start, p_abort, p_end, p_null, p_boolean,
      p_integer, p_double, p_string, NULL, NULL, NULL, NULL, NULL, c_open,
      c_report, c_close };

MAIN(dbChannelTest)
{
    dbChannel ch;

    testPlan(33);
    dbChannelInit(&ch);

    e = 0;
    testOk1(dbChannelFind(&ch, "{}"));

    e = e_init;
    dbRegisterFilter("test", &testIf);

    e = e_start | e_null | e_end;
    testOk1(dbChannelFind(&ch, "{\"test\":null}"));

    e = e_start | e_start_array | e_boolean | e_integer | e_end_array | e_end;
    testOk1(dbChannelFind(&ch, "{\"test\":[true,1]}"));

    e = e_start | e_start_map | e_map_key | e_double | e_string | e_end_map | e_end;
    testOk1(dbChannelFind(&ch, "{\"test\":{\"a\":2.7183,\"b\":\"c\"}}"));

    e = e_init;
    dbRegisterFilter("scalar", &scalarIf);

    e = e_start | e_null | e_end;
    testOk1(dbChannelFind(&ch, "{\"scalar\":null}"));

    e = e_start | e_abort;
    testOk1(!dbChannelFind(&ch, "{\"scalar\":[null]}"));
    testOk1(!dbChannelFind(&ch, "{\"scalar\":{}}"));

    return testDone();
}
