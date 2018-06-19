/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsStdlibTest.c
 *
 *      Author  Andrew Johnson
 */

#include <float.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include "epicsTypes.h"
#include "epicsStdlib.h"
#include "epicsMath.h"
#include "epicsUnitTest.h"
#include "testMain.h"

/* Implement the epicsParseDouble() API for checking the native stdtod()
 * so we can tell the user if our epicsStrtod() wrapper is unnecessary.
 */
int
parseStrtod(const char *str, double *to, char **units)
{
    int c;
    char *endp;
    double value;

    while ((c = *str) && isspace(c))
        ++str;

    errno = 0;
    value = strtod(str, &endp);

    if (endp == str)
        return S_stdlib_noConversion;
    if (errno == ERANGE)
        return (value == 0) ? S_stdlib_underflow : S_stdlib_overflow;

    while ((c = *endp) && isspace(c))
        ++endp;
    if (c && !units)
        return S_stdlib_extraneous;

    *to = value;
    if (units)
        *units = endp;
    return 0;
}
#define scanStrtod(str, to) !parseStrtod(str, to, NULL)


MAIN(epicsStdlibTest)
{
    unsigned long u;
    long l;
    unsigned long long ull;
    long long ll;
    double d;
    float f;
    char *endp;
    epicsInt8 i8;
    epicsUInt8 u8;
    epicsInt16 i16;
    epicsUInt16 u16;
    epicsInt32 i32;
    epicsUInt32 u32;
    epicsInt64 i64;
    epicsUInt64 u64;

    testPlan(199);

    testOk(epicsParseLong("", &l, 0, NULL) == S_stdlib_noConversion,
        "Long '' => noConversion");
    testOk(epicsParseULong("", &u, 0, NULL) == S_stdlib_noConversion,
        "ULong '' => noConversion");
    testOk(epicsParseLLong("", &ll, 0, NULL) == S_stdlib_noConversion,
        "LLong '' => noConversion");
    testOk(epicsParseULLong("", &ull, 0, NULL) == S_stdlib_noConversion,
        "ULLong '' => noConversion");
    testOk(epicsParseFloat("", &f, NULL) == S_stdlib_noConversion,
        "Float '' => noConversion");
    testOk(epicsParseDouble("", &d, NULL) == S_stdlib_noConversion,
        "Double '' => noConversion");

    testOk(epicsParseLong("\t \n", &l, 0, NULL) == S_stdlib_noConversion,
        "Long '\\t 1\\n' => noConversion");
    testOk(epicsParseULong("\t \n", &u, 0, NULL) == S_stdlib_noConversion,
        "ULong '\\t 1\\n' => noConversion");
    testOk(epicsParseLLong("\t \n", &ll, 0, NULL) == S_stdlib_noConversion,
        "LLong '\\t 1\\n' => noConversion");
    testOk(epicsParseULLong("\t \n", &ull, 0, NULL) == S_stdlib_noConversion,
        "ULLong '\\t 1\\n' => noConversion");
    testOk(epicsParseFloat("\t \n", &f, NULL) == S_stdlib_noConversion,
        "Float '\\t 1\\n' => noConversion");
    testOk(epicsParseDouble("\t \n", &d, NULL) == S_stdlib_noConversion,
        "Double '\\t 1\\n' => noConversion");

    testOk(epicsParseLong("!", &l, 0, NULL) == S_stdlib_noConversion,
        "Long '!' => noConversion");
    testOk(epicsParseULong("!", &u, 0, NULL) == S_stdlib_noConversion,
        "ULong '!' => noConversion");
    testOk(epicsParseLLong("!", &ll, 0, NULL) == S_stdlib_noConversion,
        "LLong '!' => noConversion");
    testOk(epicsParseULLong("!", &ull, 0, NULL) == S_stdlib_noConversion,
        "ULLong '!' => noConversion");
    testOk(epicsParseFloat("!", &f, NULL) == S_stdlib_noConversion,
        "Float '!' => noConversion");
    testOk(epicsParseDouble("!", &d, NULL) == S_stdlib_noConversion,
        "Double '!' => noConversion");

    testOk(epicsScanLong("0", &l, 0) && l == 0, "Long '0'");
    testOk(epicsScanULong("0", &u, 0) && u == 0, "ULong '0'");
    testOk(epicsScanLLong("0", &ll, 0) && ll == 0, "LLong '0'");
    testOk(epicsScanULLong("0", &ull, 0) && ull == 0, "ULLong '0'");
    testOk(epicsScanFloat("0", &f) && f == 0, "Float '0'");
    testOk(epicsScanDouble("0", &d) && d == 0, "Double '0'");

    testOk(epicsScanLong("\t 1\n", &l, 0) && l == 1, "Long '\\t 1\\n'");
    testOk(epicsScanULong("\t 1\n", &u, 0) && u == 1, "ULong '\\t 1\\n'");
    testOk(epicsScanLLong("\t 1\n", &ll, 0) && ll == 1, "LLong '\\t 1\\n'");
    testOk(epicsScanULLong("\t 1\n", &ull, 0) && ull == 1, "ULLong '\\t 1\\n'");
    testOk(epicsScanFloat("\t 1\n", &f) && f == 1, "Float '\\t 1\\n'");
    testOk(epicsScanDouble("\t 1\n", &d) && d == 1, "Double '\\t 1\\n'");

    testOk(epicsScanLong("-1", &l, 0) && l == -1, "Long '-1'");
    testOk(epicsScanULong("-1", &u, 0) && u + 1 == 0, "ULong '-1'");
    testOk(epicsScanLLong("-1", &ll, 0) && ll == -1, "LLong '-1'");
    testOk(epicsScanULLong("-1", &ull, 0) && ull + 1 == 0, "ULLong '-1'");
    testOk(epicsScanFloat("-1", &f) && f == -1, "Float '-1'");
    testOk(epicsScanDouble("-1", &d) && d == -1, "Double '-1'");

    testOk(epicsParseLong("2!", &l, 0, NULL) == S_stdlib_extraneous,
        "Long '2!' => extraneous");
    testOk(epicsParseULong("2!", &u, 0, NULL) == S_stdlib_extraneous,
        "ULong '2!' => extraneous");
    testOk(epicsParseLLong("2!", &ll, 0, NULL) == S_stdlib_extraneous,
        "LLong '2!' => extraneous");
    testOk(epicsParseULLong("2!", &ull, 0, NULL) == S_stdlib_extraneous,
        "ULLong '2!' => extraneous");
    testOk(epicsParseFloat("2!", &f, NULL) == S_stdlib_extraneous,
        "Float '2!' => extraneous");
    testOk(epicsParseDouble("2!", &d, NULL) == S_stdlib_extraneous,
        "Double '2!' => extraneous");

    testOk(epicsParseLong("3 \n\t!", &l, 0, NULL) == S_stdlib_extraneous,
        "Long '3 \\n\\t!' => extraneous");
    testOk(epicsParseULong("3 \n\t!", &u, 0, NULL) == S_stdlib_extraneous,
        "ULong '3 \\n\\t!' => extraneous");
    testOk(epicsParseLLong("3 \n\t!", &ll, 0, NULL) == S_stdlib_extraneous,
        "LLong '3 \\n\\t!' => extraneous");
    testOk(epicsParseULLong("3 \n\t!", &ull, 0, NULL) == S_stdlib_extraneous,
        "ULLong '3 \\n\\t!' => extraneous");
    testOk(epicsParseFloat("3 \n\t!", &f, NULL) == S_stdlib_extraneous,
        "Float '3 \\n\\t!' => extraneous");
    testOk(epicsParseDouble("3 \n\t!", &d, NULL) == S_stdlib_extraneous,
        "Double '3 \\n\\t!' => extraneous");

    testOk(!epicsParseLong("2!", &l, 0, &endp) && *endp == '!',
        "Long '2!' => units='!'");
    testOk(!epicsParseULong("2!", &u, 0, &endp) && *endp == '!',
        "ULong '2!' => units='!'");
    testOk(!epicsParseLLong("2!", &ll, 0, &endp) && *endp == '!',
        "LLong '2!' => units='!'");
    testOk(!epicsParseULLong("2!", &ull, 0, &endp) && *endp == '!',
        "ULLong '2!' => units='!'");
    testOk(!epicsParseFloat("2!", &f, &endp) && *endp == '!',
        "Float '2!' => units='!'");
    testOk(!epicsParseDouble("2!", &d, &endp) && *endp == '!',
        "Double '2!' => units='!'");

    testOk(!epicsParseLong("3 \n\t!", &l, 0, &endp) && *endp == '!',
        "Long '3 \\n\\t!' => units='!'");
    testOk(!epicsParseULong("3 \n\t!", &u, 0, &endp) && *endp == '!',
        "ULong '3 \\n\\t!' => units='!'");
    testOk(!epicsParseLLong("3 \n\t!", &ll, 0, &endp) && *endp == '!',
        "LLong '3 \\n\\t!' => units='!'");
    testOk(!epicsParseULLong("3 \n\t!", &ull, 0, &endp) && *endp == '!',
        "ULLong '3 \\n\\t!' => units='!'");
    testOk(!epicsParseFloat("3 \n\t!", &f, &endp) && *endp == '!',
        "Float '3 \\n\\t!' => units='!'");
    testOk(!epicsParseDouble("3 \n\t!", &d, &endp) && *endp == '!',
        "Double '3 \\n\\t!' => units='!'");

    testOk(epicsScanLong("0x0", &l, 0) && l == 0, "Long '0x0'");
    testOk(epicsScanULong("0x0", &u, 0) && u == 0, "ULong '0x0'");
    testOk(epicsScanLLong("0x0", &ll, 0) && ll == 0, "LLong '0x0'");
    testOk(epicsScanULLong("0x0", &ull, 0) && ull == 0, "ULLong '0x0'");
    testOk(epicsScanFloat("0x0", &f) && f == 0, "Float '0x0'");
    testOk(epicsScanDouble("0x0", &d) && d == 0, "Double '0x0'");

    testOk(epicsScanLong("0x1", &l, 0) && l == 1, "Long '0x1'");
    testOk(epicsScanULong("0x1", &u, 0) && u == 1, "ULong '0x1'");
    testOk(epicsScanLLong("0x1", &ll, 0) && ll == 1, "LLong '0x1'");
    testOk(epicsScanULLong("0x1", &ull, 0) && ull == 1, "ULLong '0x1'");
    testOk(epicsScanFloat("0x1", &f) && f == 1, "Float '0x1'");
    testOk(epicsScanDouble("0x1", &d) && d == 1, "Double '0x1'");

    testOk(epicsScanLong("+0x1", &l, 0) && l == 1, "Long '+0x1'");
    testOk(epicsScanULong("+0x1", &u, 0) && u == 1, "ULong '+0x1'");
    testOk(epicsScanLLong("+0x1", &ll, 0) && ll == 1, "LLong '+0x1'");
    testOk(epicsScanULLong("+0x1", &ull, 0) && ull == 1, "ULLong '+0x1'");
    testOk(epicsScanFloat("+0x1", &f) && f == 1, "Float '+0x1'");
    testOk(epicsScanDouble("+0x1", &d) && d == 1, "Double '+0x1'");

    testOk(epicsScanLong("-0x1", &l, 0) && l == -1, "Long '-0x1'");
    testOk(epicsScanULong("-0x1", &u, 0) && u == -1, "ULong '-0x1'");
    testOk(epicsScanLLong("-0x1", &ll, 0) && ll == -1, "LLong '-0x1'");
    testOk(epicsScanULLong("-0x1", &ull, 0) && ull == -1, "ULLong '-0x1'");
    testOk(epicsScanFloat("-0x1", &f) && f == -1, "Float '-0x1'");
    testOk(epicsScanDouble("-0x1", &d) && d == -1, "Double '-0x1'");

    testOk(epicsScanLong("0xf", &l, 0) && l == 15, "Long '0xf'");
    testOk(epicsScanULong("0xf", &u, 0) && u == 15, "ULong '0xf'");
    testOk(epicsScanLLong("0xf", &ll, 0) && ll == 15, "LLong '0xf'");
    testOk(epicsScanULLong("0xf", &ull, 0) && ull == 15, "ULLong '0xf'");
    testOk(epicsScanFloat("0xf", &f) && f == 15, "Float '0xf'");
    testOk(epicsScanDouble("0xf", &d) && d == 15, "Double '0xf'");

    testOk(epicsScanLong("0XF", &l, 0) && l == 15, "Long '0XF'");
    testOk(epicsScanULong("0XF", &u, 0) && u == 15, "ULong '0XF'");
    testOk(epicsScanLLong("0XF", &ll, 0) && ll == 15, "LLong '0XF'");
    testOk(epicsScanULLong("0XF", &ull, 0) && ull == 15, "ULLong '0XF'");
    testOk(epicsScanFloat("0XF", &f) && f == 15, "Float '0XF'");
    testOk(epicsScanDouble("0XF", &d) && d == 15, "Double '0XF'");

    testOk(epicsParseLong("0x0", &l, 10, NULL) == S_stdlib_extraneous,
        "Long '0x0' in base 10 => extraneous");
    testOk(epicsParseULong("0x0", &u, 10, NULL) == S_stdlib_extraneous,
        "ULong '0x0' in base 10 => extraneous");
    testOk(epicsParseLLong("0x0", &ll, 10, NULL) == S_stdlib_extraneous,
        "LLong '0x0' in base 10 => extraneous");
    testOk(epicsParseULLong("0x0", &ull, 10, NULL) == S_stdlib_extraneous,
        "ULLong '0x0' in base 10 => extraneous");

    testOk(epicsScanLong("0x10", &l, 0) && l == 0x10,
        "Long '0x10' in base 0");
    testOk(epicsScanULong("0x10", &u, 0) && u == 0x10,
        "ULong '0x10' in base 0");
    testOk(epicsScanLong("0x10", &l, 16) && l == 0x10,
        "Long '0x10' in base 16");
    testOk(epicsScanULong("0x10", &u, 16) && u == 0x10,
        "ULong '0x10' in base 16");
    testOk(epicsScanLong("10", &l, 16) && l == 0x10,
        "Long '10' in base 16");
    testOk(epicsScanULong("10", &u, 16) && u == 0x10,
        "ULong '10' in base 16");

    testOk(epicsScanLong("0x7fffffff", &l, 0) && l == 0x7fffffff,
        "Long '0x7fffffff'");
    testOk(epicsScanULong("0xffffffff", &u, 0) && u == 0xffffffff,
        "ULong '0xffffffff'");
    testOk(epicsScanLLong("0x7fffffffffffffff", &ll, 0) &&
        ll == 0x7fffffffffffffffLL, "LLong '0x7fffffffffffffff'");
    testOk(epicsScanULLong("0xffffffffffffffff", &ull, 0) &&
        ull == 0xffffffffffffffffULL, "ULLong '0xffffffffffffffff'");
    testOk(epicsScanFloat("0xffffff", &f) && f == 0xffffff,
        "Float '0xffffff'");
    testOk(epicsScanDouble("0xffffffff", &d) && d == 0xffffffff,
        "Double '0xffffffff'");

    testOk(epicsScanLong("-0x7fffffff", &l, 0) && l == -0x7fffffff,
        "Long '-0x7fffffff'");
    testOk(epicsScanULong("-0x7fffffff", &u, 0) && u == -0x7fffffff,
        "ULong '-0x7fffffff'");
    testOk(epicsScanLLong("-0x7fffffffffffffff", &ll, 0)
        && ll == -0x7fffffffffffffffLL, "LLong '-0x7fffffffffffffff'");
    testOk(epicsScanULLong("-0x7fffffffffffffff", &ull, 0) &&
        ull == -0x7fffffffffffffffULL, "ULLong '-0x7fffffffffffffff'");
    testOk(epicsScanFloat("-0xffffff", &f) && f == -0xffffff,
        "Float '-0xffffff'");
    testOk(epicsScanDouble("-0x7fffffff", &d) && d == -0x7fffffff,
        "Double '-0x7fffffff'");

    testOk(!epicsParseInt8("0x7f", &i8, 0, NULL) && i8 == 0x7f,
        "Int8 '0x7f'");
    testOk(!epicsParseInt8("-0x80", &i8, 0, NULL) && ((i8 + 0x80) & 0xff) == 0,
        "Int8 '-0x80'");
    testOk(!epicsParseUInt8("0xff", &u8, 0, NULL) && u8 == 0xff,
        "UInt8 '0xff'");
    testOk(!epicsParseUInt8("-1", &u8, 0, NULL) && u8 == 0xff,
        "UInt8 '-1'");
    testOk(epicsParseInt8("0x80", &i8, 0, NULL) == S_stdlib_overflow,
        "Int8 '0x80' => overflow");
    testOk(epicsParseInt8("-0x81", &i8, 0, NULL) == S_stdlib_overflow,
        "Int8 '-0x81' => overflow");
    testOk(epicsParseUInt8("0x100", &u8, 0, NULL) == S_stdlib_overflow,
        "UInt8 '0x100' => overflow");
    testOk(epicsParseUInt8("-0x100", &u8, 0, NULL) == S_stdlib_overflow,
        "UInt8 '-0x100' => overflow");

    testOk(!epicsParseInt16("0x7fff", &i16, 0, NULL) && i16 == 0x7fff,
        "Int16 '0x7fff'");
    testOk(!epicsParseInt16("-0x8000", &i16, 0, NULL) && i16 == -0x8000,
        "Int16 '-0x8000'");
    testOk(!epicsParseUInt16("0xffff", &u16, 0, NULL) && u16 == 0xffff,
        "UInt16 '0xffff'");
    testOk(!epicsParseUInt16("-1", &u16, 0, NULL) && u16 == 0xffff,
        "UInt16 '-1'");
    testOk(epicsParseInt16("0x8000", &i16, 0, NULL) == S_stdlib_overflow,
        "Int16 '0x8000' => overflow");
    testOk(epicsParseInt16("-0x8001", &i16, 0, NULL) == S_stdlib_overflow,
        "Int16 '-0x8001' => overflow");
    testOk(epicsParseUInt16("0x10000", &u16, 0, NULL) == S_stdlib_overflow,
        "UInt16 '0x10000' => overflow");
    testOk(epicsParseUInt16("-0x10000", &u16, 0, NULL) == S_stdlib_overflow,
        "UInt16 '-0x10000' => overflow");

    testOk(!epicsParseInt32("0x7fffffff", &i32, 0, NULL) && i32 == 0x7fffffff,
        "Int32 '0x7fffffff'");
    testOk(!epicsParseInt32("-0x80000000", &i32, 0, NULL) && i32 == -0x80000000L,
        "Int32 '-0x80000000'");
    testOk(!epicsParseUInt32("0xffffffff", &u32, 0, NULL) && u32 == 0xffffffff,
        "UInt32 '0xffffffff'");
    testOk(!epicsParseUInt32("-1", &u32, 0, NULL) && u32 == 0xffffffffU,
        "UInt32 '-1'");
    testOk(epicsParseInt32("0x80000000", &i32, 0, NULL) == S_stdlib_overflow,
        "Int32 '0x80000000' => overflow");
    testOk(epicsParseInt32("-0x80000001", &i32, 0, NULL) == S_stdlib_overflow,
        "Int32 '-0x80000001' => overflow");
    testOk(epicsParseUInt32("0x100000000", &u32, 0, NULL) == S_stdlib_overflow,
        "UInt32 '0x100000000' => overflow");
    testOk(epicsParseUInt32("-0x100000000", &u32, 0, NULL) == S_stdlib_overflow,
        "UInt32 '-0x100000000' => overflow");

    testOk(!epicsParseInt64("0x7fffffffffffffff", &i64, 0, NULL) &&
        i64 == 0x7fffffffffffffffLL, "Int64 '0x7fffffffffffffff'");
    testOk(!epicsParseInt64("-0x8000000000000000", &i64, 0, NULL) &&
        i64 == -0x8000000000000000LL, "Int64 '-0x8000000000000000'");
    testOk(!epicsParseUInt64("0xffffffffffffffff", &u64, 0, NULL) &&
        u64 == 0xffffffffffffffffULL, "UInt64 '0xffffffffffffffff'");
    testOk(!epicsParseUInt64("-1", &u64, 0, NULL) && u64 == 0xffffffffffffffffULL,
        "UInt64 '-1'");
    testOk(epicsParseInt64("0x8000000000000000", &i64, 0, NULL) == S_stdlib_overflow,
        "Int64 '0x8000000000000000' => overflow");
    testOk(epicsParseInt64("-0x8000000000000001", &i64, 0, NULL) == S_stdlib_overflow,
        "Int64 '-0x8000000000000001' => overflow");
    testOk(epicsParseUInt64("0x10000000000000000", &u64, 0, NULL) == S_stdlib_overflow,
        "UInt64 '0x10000000000000000' => overflow");
    testOk(epicsParseUInt64("-0x10000000000000000", &u64, 0, NULL) == S_stdlib_overflow,
        "UInt64 '-0x10000000000000000' => overflow");

    testOk(epicsScanFloat(".1", &f) && fabs(f - 0.1) < 1e-7,
        "Float '.1'");
    testOk(epicsScanDouble(".1", &d) && fabs(d - 0.1) < 1e-15,
        "Double '.1'");

    testOk(epicsScanFloat("0.1", &f) && fabs(f - 0.1) < 1e-7,
        "Float '0.1'");
    testOk(epicsScanDouble("0.1", &d) && fabs(d - 0.1) < 1e-15,
        "Double '0.1'");

    testOk(epicsScanFloat("1e-1", &f) && fabs(f - 1e-1) < 1e-7,
        "Float '1e-1'");
    testOk(epicsScanDouble("1e-1", &d) && fabs(d - 1e-1) < 1e-15,
        "Double '1e-1'");

    testOk(epicsScanFloat("-.1", &f) && fabs(f + 0.1) < 1e-7,
        "Float '-.1'");
    testOk(epicsScanDouble("-.1", &d) && fabs(d + 0.1) < 1e-15,
        "Double '-.1'");

    testOk(epicsScanFloat("-0.1", &f) && fabs(f + 0.1) < 1e-7,
        "Float '-0.1'");
    testOk(epicsScanDouble("-0.1", &d) && fabs(d + 0.1) < 1e-15,
        "Double '-0.1'");

    testOk(epicsScanFloat("-1e-1", &f) && fabs(f + 1e-1) < 1e-7,
        "Float '-1e-1'");
    testOk(epicsScanDouble("-1e-1", &d) && fabs(d + 1e-1) < 1e-15,
        "Double '-1e-1'");

    testOk(epicsScanFloat("1e-30", &f) && fabs(f - 1e-30) < 1e-36,
        "Float '1e-30'");
    testOk(epicsScanDouble("1e-300", &d) && fabs(d - 1e-300) < 1e-306,
        "Double '1e-300'");

    testOk(epicsParseFloat("1e-40", &f, NULL) == S_stdlib_underflow,
        "Float '1e-40' => underflow");
    testOk(epicsParseDouble("1e-330", &d, NULL) == S_stdlib_underflow,
        "Double '1e-330' => underflow");

    testOk(epicsScanFloat("1e30", &f) && fabs(f - 1e30) < 1e24,
        "Float '1e30'");
    testOk(epicsScanDouble("1e300", &d) && fabs(d - 1e300) < 1e285,
        "Double '1e300'");

    testOk(epicsParseFloat("1e40", &f, NULL) == S_stdlib_overflow,
        "Float '1e40' => overflow");
    testOk(epicsParseDouble("1e310", &d, NULL) == S_stdlib_overflow,
        "Double '1e330' => overflow");

    testOk(epicsScanLong("2147483647", &l, 0) && l == 2147483647,
        "Long '2147483647'");
    testOk(epicsScanLong("-2147483647", &l, 0) && l == -2147483647,
        "Long '-2147483647'");
    testOk(epicsScanULong("4294967295", &u, 0) && u == 4294967295u,
        "ULong '4294967295'");
    testOk(epicsScanFloat("16777214", &f) && f == 16777214.0,
        "Float '16777214'");
    testOk(epicsScanFloat("16777215", &f) && f == 16777215.0,
        "Float '16777215'");
    testOk(epicsScanFloat("-16777215", &f) && f == -16777215.0,
        "Float '-16777215'");
    testOk(epicsScanFloat("-16777216", &f) && f == -16777216.0,
        "Float '-16777216'");
    testOk(epicsScanDouble("4294967294", &d) && d == 4294967294.0,
        "Double '4294967294'");
    testOk(epicsScanDouble("4294967295", &d) && d == 4294967295.0,
        "Double '4294967295'");
    testOk(epicsScanDouble("-4294967295", &d) && d == -4294967295.0,
        "Double '-4294967295'");
    testOk(epicsScanDouble("-4294967296", &d) && d == -4294967296.0,
        "Double '-4294967296'");

    testOk(epicsScanFloat("NAN", &f) && isnan(f), "Float 'NAN'");
    testOk(epicsScanDouble("NAN", &d) && isnan(d), "Double 'NAN'");
    testOk(epicsScanFloat("Nan", &f) && isnan(f), "Float 'Nan'");
    testOk(epicsScanDouble("Nan", &d) && isnan(d), "Double 'Nan'");
#ifdef __rtems__
    testTodoBegin("RTEMS (newlib) parser doesn't recognise 'nan()'");
#endif
    testOk(epicsScanFloat("nan()", &f) && isnan(f), "Float 'nan()'");
    testOk(epicsScanDouble("nan()", &d) && isnan(d), "Double 'nan()'");
#ifdef __rtems__
    testTodoEnd();
#endif

    testOk(epicsScanFloat("INF", &f) && f == epicsINF,
        "Float 'INF'");
    testOk(epicsScanDouble("INF", &d) && d == epicsINF,
        "Double 'INF'");
    testOk(epicsScanFloat("Infinity", &f) && f == epicsINF,
        "Float 'Infinity'");
    testOk(epicsScanDouble("Infinity", &d) && d == epicsINF,
        "Double 'Infinity'");

    testOk(epicsScanFloat("+INF", &f) && f == epicsINF,
        "Float '+INF'");
    testOk(epicsScanDouble("+INF", &d) && d == epicsINF,
        "Double '+INF'");
    testOk(epicsScanFloat("+Infinity", &f) && f == epicsINF,
        "Float '+Infinity'");
    testOk(epicsScanDouble("+Infinity", &d) && d == epicsINF,
        "Double '+Infinity'");

    testOk(epicsScanFloat("-INF", &f) && f == -epicsINF,
        "Float '-INF'");
    testOk(epicsScanDouble("-INF", &d) && d == -epicsINF,
        "Double '-INF'");
    testOk(epicsScanFloat("-Infinity", &f) && f == -epicsINF,
        "Float '-Infinity'");
    testOk(epicsScanDouble("-Infinity", &d) && d == -epicsINF,
        "Double '-Infinity'");

#ifdef epicsStrtod
#define CHECK_STRTOD epicsStrtod != strtod
    if (epicsStrtod == strtod)
        testDiag("This target defines epicsStrtod as strtod");
    else
        testDiag("This target defines its own epicsStrtod macro");
#else
#define CHECK_STRTOD 1
    testDiag("This target compiles epicsStrtod()");
#endif

    if (CHECK_STRTOD) {
        int pass = 0, fail = 0;

#define CHECK(ok, msg) if (ok) pass++; else fail++, testDiag(msg)

        testDiag("Checking the native strtod(), only failures shown:");

        CHECK(parseStrtod("", &d, NULL) == S_stdlib_noConversion,
            "   not ok - strtod('') => noConversion");
        CHECK(parseStrtod("\t \n", &d, NULL) == S_stdlib_noConversion,
            "   not ok - strtod('\\t 1\\n') => noConversion");
        CHECK(parseStrtod("!", &d, NULL) == S_stdlib_noConversion,
            "   not ok - strtod('!') => noConversion");
        CHECK(scanStrtod("0", &d) && d == 0,
            "   not ok - strtod('0')");
        CHECK(scanStrtod("\t 1\n", &d) && d == 1,
            "   not ok - strtod('\\t 1\\n')");
        CHECK(parseStrtod("2!", &d, NULL) == S_stdlib_extraneous,
            "   not ok - strtod('2!') => extraneous");
        CHECK(parseStrtod("3 !", &d, NULL) == S_stdlib_extraneous,
            "   not ok - strtod('3 !') => extraneous");
        CHECK(!parseStrtod("2!", &d, &endp) && *endp == '!',
            "   not ok - strtod('2!') => units='!'");
        CHECK(!parseStrtod("3 \n\t!", &d, &endp) && *endp == '!',
            "   not ok - strtod('3 \\n\\t!') => units='!'");
        CHECK(scanStrtod("0x0", &d) && d == 0,
            "   not ok - strtod('0x0')");
        CHECK(scanStrtod("0x1", &d) && d == 1,
            "   not ok - strtod('0x1')");
        CHECK(scanStrtod("+0x1", &d) && d == 1,
            "   not ok - strtod('+0x1')");
        CHECK(scanStrtod("-0x1", &d) && d == -1,
            "   not ok - strtod('-0x1')");
        CHECK(scanStrtod("0xf", &d) && d == 15,
            "   not ok - strtod('0xf')");
        CHECK(scanStrtod("0XF", &d) && d == 15,
            "   not ok - strtod('0XF')");
        CHECK(scanStrtod("0xffffffff", &d) && d == 0xffffffff,
            "   not ok - strtod('0xffffffff')");
        CHECK(scanStrtod("-0x7fffffff", &d) && d == -0x7fffffffl,
            "   not ok - strtod('-0x7fffffff')");
        CHECK(scanStrtod(".1", &d) && fabs(d - 0.1) < 1e-15,
            "   not ok - strtod('.1')");
        CHECK(scanStrtod("0.1", &d) && fabs(d - 0.1) < 1e-15,
            "   not ok - strtod('0.1')");
        CHECK(scanStrtod("1e-1", &d) && fabs(d - 1e-1) < 1e-15,
            "   not ok - strtod('1e-1')");
        CHECK(scanStrtod("-.1", &d) && fabs(d + 0.1) < 1e-15,
            "   not ok - strtod('-.1')");
        CHECK(scanStrtod("-0.1", &d) && fabs(d + 0.1) < 1e-15,
            "   not ok - strtod('-0.1')");
        CHECK(scanStrtod("-1e-1", &d) && fabs(d + 1e-1) < 1e-15,
            "   not ok - strtod('-1e-1')");
        CHECK(scanStrtod("1e-300", &d) && fabs(d - 1e-300) < 1e-306,
            "   not ok - strtod('1e-300')");
        CHECK(parseStrtod("1e-400", &d, NULL) == S_stdlib_underflow,
            "   not ok - strtod('1e-400') => underflow");
        CHECK(scanStrtod("4294967294", &d) && d == 4294967294.0,
            "   not ok - strtod('4294967294')");
        CHECK(scanStrtod("4294967295", &d) && d == 4294967295.0,
            "   not ok - strtod('4294967295')");
        CHECK(scanStrtod("-4294967295", &d) && d == -4294967295.0,
            "   not ok - strtod('-4294967295')");
        CHECK(scanStrtod("-4294967296", &d) && d == -4294967296.0,
            "   not ok - strtod('-4294967296')");
        CHECK(scanStrtod("NAN", &d) && isnan(d),
            "   not ok - strtod('NAN')");
        CHECK(scanStrtod("Nan", &d) && isnan(d),
            "   not ok - strtod('Nan')");
        CHECK(scanStrtod("nan()", &d) && isnan(d),
            "   not ok - strtod('nan()')");
        CHECK(scanStrtod("INF", &d) && d == epicsINF,
            "   not ok - strtod('INF')");
        CHECK(scanStrtod("Infinity", &d) && d == epicsINF,
            "   not ok - strtod('Infinity')");
        CHECK(scanStrtod("+INF", &d) && d == epicsINF,
            "   not ok - strtod('+INF')");
        CHECK(scanStrtod("+Infinity", &d) && d == epicsINF,
            "   not ok - strtod('+Infinity')");
        CHECK(scanStrtod("-INF", &d) && d == -epicsINF,
            "   not ok - strtod('-INF')");
        CHECK(scanStrtod("-Infinity", &d) && d == -epicsINF,
            "   not ok - strtod('-Infinity')");

        if (fail)
            testDiag("The native strtod() passed %d and failed %d tests",
                pass, fail);
        else {
            testDiag("The native strtod() passed all %d tests", pass);
#ifndef epicsStrtod
            testDiag("The compiled epicsStrtod() should not be needed");
#endif
        }
    }

    return testDone();
}
