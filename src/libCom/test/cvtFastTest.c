/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* cvtFastTest.c
 *
 * Test the string converters in libCom/cvtFast/cvtFast.c
 *
 * To Do: Test Char/Uchar/Short/Ushort/Long/Ulong versions
 */

#include <math.h>
#include <float.h>
#include <stdio.h>

#include "epicsUnitTest.h"
#include "cvtFast.h"
#include "epicsStdlib.h"
#include "testMain.h"

#define tryIString(typ, lit, siz) \
    len = cvt##typ##ToString(lit, buf); \
    testOk(len == siz, "cvt"#typ"ToString(" #lit ") == " #siz " (%u) -> \"%s\"", (unsigned)len, buf); \
    status = epicsParse##typ(buf, &val_##typ, 10, NULL); \
    testOk(!status, "epicsParse"#typ"('%s') OK", buf); \
    testOk(val_##typ == lit, #lit " => '%s'", buf);

#define tryFString(typ, lit, prec, siz) \
    len = cvt##typ##ToString(lit, buf, prec); \
    testOk(len == siz, "cvt"#typ"ToString(" #lit ", %d) == " #siz " (%u) -> \"%s\"", prec, (unsigned)len, buf); \
    status = epicsParse##typ(buf, &val_##typ, NULL); \
    testOk(!status, "epicsParse"#typ"('%s') OK", buf); \
    testOk(fabs(val_##typ - lit) < 0.5 * pow(10, -prec), #lit " => '%s'", buf);


MAIN(cvtFastTest)
{
    char buf[80];
    size_t len;
    long status;
    epicsUInt32 val_UInt32;
    epicsInt32 val_Int32;
    epicsUInt64 val_UInt64;
    epicsInt64 val_Int64;
    epicsFloat32 val_Float;
    epicsFloat64 val_Double;

#ifdef _WIN32
#if (defined(_MSC_VER) && _MSC_VER < 1900) || \
    (defined(_MINGW) && defined(_TWO_DIGIT_EXPONENT))
    _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
#endif

    testPlan(1062);

    /* Arguments: type, value, num chars */
    testDiag("------------------------------------------------------");
    testDiag("** Positive Int32 **");
    tryIString(Int32, 0, 1);
    tryIString(Int32, 1, 1);
    tryIString(Int32, 10, 2);
    tryIString(Int32, 100, 3);
    tryIString(Int32, 254, 3);
    tryIString(Int32, 255, 3);
    tryIString(Int32, 256, 3);
    tryIString(Int32, 257, 3);
    tryIString(Int32, 1000, 4);
    tryIString(Int32, 10000, 5);
    tryIString(Int32, 32766, 5);
    tryIString(Int32, 32767, 5);
    tryIString(Int32, 32768, 5);
    tryIString(Int32, 32769, 5);
    tryIString(Int32, 65534, 5);
    tryIString(Int32, 65535, 5);
    tryIString(Int32, 65536, 5);
    tryIString(Int32, 65537, 5);
    tryIString(Int32, 2147483646, 10);
    tryIString(Int32, 2147483647, 10);

    testDiag("------------------------------------------------------");
    testDiag("** Negative Int32 **");
    tryIString(Int32, -1, 2);
    tryIString(Int32, -10, 3);
    tryIString(Int32, -100, 4);
    tryIString(Int32, -254, 4);
    tryIString(Int32, -255, 4);
    tryIString(Int32, -256, 4);
    tryIString(Int32, -257, 4);
    tryIString(Int32, -1000, 5);
    tryIString(Int32, -10000, 6);
    tryIString(Int32, -32766, 6);
    tryIString(Int32, -32767, 6);
    tryIString(Int32, -32768, 6);
    tryIString(Int32, -32769, 6);
    tryIString(Int32, -65534, 6);
    tryIString(Int32, -65535, 6);
    tryIString(Int32, -65536, 6);
    tryIString(Int32, -65537, 6);
    tryIString(Int32, -2147483647, 11);
    tryIString(Int32, -2147483648LL, 11);

    testDiag("------------------------------------------------------");
    testDiag("** UInt32 **");
    tryIString(UInt32, 0, 1);
    tryIString(UInt32, 1, 1);
    tryIString(UInt32, 10, 2);
    tryIString(UInt32, 100, 3);
    tryIString(UInt32, 254, 3);
    tryIString(UInt32, 255, 3);
    tryIString(UInt32, 256, 3);
    tryIString(UInt32, 257, 3);
    tryIString(UInt32, 1000, 4);
    tryIString(UInt32, 10000, 5);
    tryIString(UInt32, 32766, 5);
    tryIString(UInt32, 32767, 5);
    tryIString(UInt32, 32768, 5);
    tryIString(UInt32, 32769, 5);
    tryIString(UInt32, 65534, 5);
    tryIString(UInt32, 65535, 5);
    tryIString(UInt32, 65536, 5);
    tryIString(UInt32, 65537, 5);
    tryIString(UInt32, 2147483646ULL, 10);
    tryIString(UInt32, 2147483647ULL, 10);
    tryIString(UInt32, 2147483648ULL, 10);
    tryIString(UInt32, 4294967294ULL, 10);
    tryIString(UInt32, 4294967295ULL, 10);

    testDiag("------------------------------------------------------");
    testDiag("** Positive Int64 **");
    tryIString(Int64, 0, 1);
    tryIString(Int64, 1, 1);
    tryIString(Int64, 10, 2);
    tryIString(Int64, 100, 3);
    tryIString(Int64, 254, 3);
    tryIString(Int64, 255, 3);
    tryIString(Int64, 256, 3);
    tryIString(Int64, 257, 3);
    tryIString(Int64, 1000, 4);
    tryIString(Int64, 10000, 5);
    tryIString(Int64, 32766, 5);
    tryIString(Int64, 32767, 5);
    tryIString(Int64, 32768, 5);
    tryIString(Int64, 32769, 5);
    tryIString(Int64, 65534, 5);
    tryIString(Int64, 65535, 5);
    tryIString(Int64, 65536, 5);
    tryIString(Int64, 65537, 5);
    tryIString(Int64, 2147483646, 10);
    tryIString(Int64, 2147483647, 10);
    tryIString(Int64, 2147483648LL, 10);
    tryIString(Int64, 9223372036854775806LL, 19);
    tryIString(Int64, 9223372036854775807LL, 19);

    testDiag("------------------------------------------------------");
    testDiag("** Negative Int64 **");
    tryIString(Int64, -1, 2);
    tryIString(Int64, -10, 3);
    tryIString(Int64, -100, 4);
    tryIString(Int64, -254, 4);
    tryIString(Int64, -255, 4);
    tryIString(Int64, -256, 4);
    tryIString(Int64, -257, 4);
    tryIString(Int64, -1000, 5);
    tryIString(Int64, -10000, 6);
    tryIString(Int64, -32766, 6);
    tryIString(Int64, -32767, 6);
    tryIString(Int64, -32768, 6);
    tryIString(Int64, -32769, 6);
    tryIString(Int64, -65534, 6);
    tryIString(Int64, -65535, 6);
    tryIString(Int64, -65536, 6);
    tryIString(Int64, -65537, 6);
    tryIString(Int64, -2147483647, 11);
    tryIString(Int64, -2147483648LL, 11);
    tryIString(Int64, -2147483649LL, 11);
    tryIString(Int64, -9223372036854775806LL, 20);
    tryIString(Int64, -9223372036854775807LL, 20);
    tryIString(Int64, -9223372036854775807LL-1, 20);

    testDiag("------------------------------------------------------");
    testDiag("** UInt64 **");
    tryIString(UInt64, 0, 1);
    tryIString(UInt64, 1, 1);
    tryIString(UInt64, 10, 2);
    tryIString(UInt64, 100, 3);
    tryIString(UInt64, 254, 3);
    tryIString(UInt64, 255, 3);
    tryIString(UInt64, 256, 3);
    tryIString(UInt64, 257, 3);
    tryIString(UInt64, 1000, 4);
    tryIString(UInt64, 10000, 5);
    tryIString(UInt64, 32766, 5);
    tryIString(UInt64, 32767, 5);
    tryIString(UInt64, 32768, 5);
    tryIString(UInt64, 32769, 5);
    tryIString(UInt64, 65534, 5);
    tryIString(UInt64, 65535, 5);
    tryIString(UInt64, 65536, 5);
    tryIString(UInt64, 65537, 5);
    tryIString(UInt64, 2147483646, 10);
    tryIString(UInt64, 2147483647, 10);
    tryIString(UInt64, 2147483648U, 10);
    tryIString(UInt64, 2147483649U, 10);
    tryIString(UInt64, 4294967294U, 10);
    tryIString(UInt64, 4294967295U, 10);
    tryIString(UInt64, 4294967296ULL, 10);
    tryIString(UInt64, 4294967297ULL, 10);
    tryIString(UInt64, 9223372036854775806ULL, 19);
    tryIString(UInt64, 9223372036854775807ULL, 19);
    tryIString(UInt64, 9223372036854775808ULL, 19);
    tryIString(UInt64, 18446744073709551614ULL, 20);
    tryIString(UInt64, 18446744073709551615ULL, 20);

    /* Arguments: type, value, precision, num chars */
    testDiag("------------------------------------------------------");
    testDiag("** Positive Float fixed-point **");
    tryFString(Float, 0, 0, 1);
    tryFString(Float, 0, 1, 3);
    tryFString(Float, 0, 2, 4);
    tryFString(Float, 0, 3, 5);
    tryFString(Float, 0, 4, 6);
    tryFString(Float, 0, 5, 7);
    tryFString(Float, 0, 6, 8);
    tryFString(Float, 0, 7, 9);
    tryFString(Float, 0, 8, 10);
    tryFString(Float, FLT_MIN, 0, 1);
    tryFString(Float, FLT_MIN, 1, 3);
    tryFString(Float, FLT_MIN, 2, 4);
    tryFString(Float, FLT_MIN, 3, 5);
    tryFString(Float, FLT_MIN, 4, 6);
    tryFString(Float, FLT_MIN, 5, 7);
    tryFString(Float, FLT_MIN, 6, 8);
    tryFString(Float, FLT_MIN, 7, 9);
    tryFString(Float, FLT_MIN, 8, 10);
    tryFString(Float, 0.000000004999999, 8, 10);
    tryFString(Float, 0.000000005000001, 8, 10);
    tryFString(Float, 0.00000004999999, 7, 9);
    tryFString(Float, 0.00000005000001, 7, 9);
    tryFString(Float, 0.0000004999999, 6, 8);
    tryFString(Float, 0.0000005000001, 6, 8);
    tryFString(Float, 0.000004999999, 5, 7);
    tryFString(Float, 0.000005000001, 5, 7);
    tryFString(Float, 0.00004999999, 4, 6);
    tryFString(Float, 0.00005000001, 4, 6);
    tryFString(Float, 0.0004999999, 3, 5);
    tryFString(Float, 0.0005000001, 3, 5);
    tryFString(Float, 0.004999999, 2, 4);
    tryFString(Float, 0.005000001, 2, 4);
    tryFString(Float, 0.04999999, 1, 3);
    tryFString(Float, 0.05000001, 1, 3);
    tryFString(Float, 0.4999999, 0, 1);
    tryFString(Float, 0.5000001, 0, 1);
    tryFString(Float, 1, 0, 1);
    tryFString(Float, 1, 1, 3);
    tryFString(Float, 1, 2, 4);
    tryFString(Float, 1, 3, 5);
    tryFString(Float, 1, 4, 6);
    tryFString(Float, 1, 5, 7);
    tryFString(Float, 1, 6, 8);
    tryFString(Float, 1, 7, 9);
    tryFString(Float, 1, 8, 10);
    tryFString(Float, 1.0500001, 1, 3);
    tryFString(Float, 1.1,       1, 3);
    tryFString(Float, 1.1499999, 1, 3);
    tryFString(Float, 9.5000001, 0, 2);
    tryFString(Float, 10, 0, 2);
    tryFString(Float, 10, 1, 4);
    tryFString(Float, 10, 8, 11);
    tryFString(Float, 100, 0, 3);
    tryFString(Float, 100, 1, 5);
    tryFString(Float, 100, 8, 12);
    tryFString(Float, 1000, 0, 4);
    tryFString(Float, 1000, 1, 6);
    tryFString(Float, 1000, 8, 13);
    tryFString(Float, 10000, 0, 5);
    tryFString(Float, 10000, 1, 7);
    tryFString(Float, 10000, 8, 14);
    tryFString(Float, 100000, 0, 6);
    tryFString(Float, 100000, 1, 8);
    tryFString(Float, 100000, 8, 15);
    tryFString(Float, 1000000, 0, 7);
    tryFString(Float, 1000000, 1, 9);
    tryFString(Float, 1000000, 8, 16);
    tryFString(Float, 10000000, 0, 8);
    tryFString(Float, 10000000, 1, 10);
    tryFString(Float, 10000000, 8, 17);

    testDiag("------------------------------------------------------");
    testDiag("** Negative Float fixed-point **");
    tryFString(Float, -FLT_MIN, 0, 2);
    tryFString(Float, -FLT_MIN, 1, 4);
    tryFString(Float, -FLT_MIN, 8, 11);
    tryFString(Float, -1, 0, 2);
    tryFString(Float, -1, 1, 4);
    tryFString(Float, -1, 8, 11);
    tryFString(Float, -1.0500001, 1, 4);
    tryFString(Float, -1.1,       1, 4);
    tryFString(Float, -1.1499999, 1, 4);
    tryFString(Float, -9.5000001, 0, 3);
    tryFString(Float, -10, 0, 3);
    tryFString(Float, -10, 1, 5);
    tryFString(Float, -10, 8, 12);
    tryFString(Float, -100, 0, 4);
    tryFString(Float, -100, 1, 6);
    tryFString(Float, -100, 8, 13);
    tryFString(Float, -1000, 0, 5);
    tryFString(Float, -1000, 1, 7);
    tryFString(Float, -1000, 8, 14);
    tryFString(Float, -10000, 0, 6);
    tryFString(Float, -10000, 1, 8);
    tryFString(Float, -10000, 8, 15);
    tryFString(Float, -100000, 0, 7);
    tryFString(Float, -100000, 1, 9);
    tryFString(Float, -100000, 8, 16);
    tryFString(Float, -1000000, 0, 8);
    tryFString(Float, -1000000, 1, 10);
    tryFString(Float, -1000000, 8, 17);
    tryFString(Float, -10000000, 0, 9);
    tryFString(Float, -10000000, 1, 11);
    tryFString(Float, -10000000, 8, 18);

    /*
     * Values > 1e7 trigger the %e format.
     */
    testDiag("------------------------------------------------------");
    testDiag("** Positive Float scientific **");
    tryFString(Float, 1e+08, 0, 6);
    tryFString(Float, 1e+08, 1, 7);
    tryFString(Float, 1e+08, 2, 8);
    tryFString(Float, 1e+08, 3, 9);
    tryFString(Float, 1e+08, 4, 10);
    tryFString(Float, 1e+08, 5, 11);
    tryFString(Float, 1e+08, 6, 12);

    testDiag("------------------------------------------------------");
    testDiag("** Positive Double fixed-point **");
    tryFString(Double, 0, 0, 1);
    tryFString(Double, 0, 1, 3);
    tryFString(Double, 0, 2, 4);
    tryFString(Double, 0, 3, 5);
    tryFString(Double, 0, 4, 6);
    tryFString(Double, 0, 5, 7);
    tryFString(Double, 0, 6, 8);
    tryFString(Double, 0, 7, 9);
    tryFString(Double, 0, 8, 10);
    tryFString(Double, DBL_MIN, 0, 1);
    tryFString(Double, DBL_MIN, 1, 3);
    tryFString(Double, DBL_MIN, 2, 4);
    tryFString(Double, DBL_MIN, 3, 5);
    tryFString(Double, DBL_MIN, 4, 6);
    tryFString(Double, DBL_MIN, 5, 7);
    tryFString(Double, DBL_MIN, 6, 8);
    tryFString(Double, DBL_MIN, 7, 9);
    tryFString(Double, DBL_MIN, 8, 10);
    tryFString(Double, 0.000000004999999, 8, 10);
    tryFString(Double, 0.000000005000001, 8, 10);
    tryFString(Double, 0.00000004999999, 7, 9);
    tryFString(Double, 0.00000005000001, 7, 9);
    tryFString(Double, 0.0000004999999, 6, 8);
    tryFString(Double, 0.0000005000001, 6, 8);
    tryFString(Double, 0.000004999999, 5, 7);
    tryFString(Double, 0.000005000001, 5, 7);
    tryFString(Double, 0.00004999999, 4, 6);
    tryFString(Double, 0.00005000001, 4, 6);
    tryFString(Double, 0.0004999999, 3, 5);
    tryFString(Double, 0.0005000001, 3, 5);
    tryFString(Double, 0.004999999, 2, 4);
    tryFString(Double, 0.005000001, 2, 4);
    tryFString(Double, 0.04999999, 1, 3);
    tryFString(Double, 0.05000001, 1, 3);
    tryFString(Double, 0.4999999, 0, 1);
    tryFString(Double, 0.5000001, 0, 1);
    tryFString(Double, 1, 0, 1);
    tryFString(Double, 1, 1, 3);
    tryFString(Double, 1, 2, 4);
    tryFString(Double, 1, 3, 5);
    tryFString(Double, 1, 4, 6);
    tryFString(Double, 1, 5, 7);
    tryFString(Double, 1, 6, 8);
    tryFString(Double, 1, 7, 9);
    tryFString(Double, 1, 8, 10);
    tryFString(Double, 1.0500001, 1, 3);
    tryFString(Double, 1.1,       1, 3);
    tryFString(Double, 1.1499999, 1, 3);
    tryFString(Double, 9.5000001, 0, 2);
    tryFString(Double, 10, 0, 2);
    tryFString(Double, 10, 1, 4);
    tryFString(Double, 10, 8, 11);
    tryFString(Double, 100, 0, 3);
    tryFString(Double, 100, 1, 5);
    tryFString(Double, 100, 8, 12);
    tryFString(Double, 1000, 0, 4);
    tryFString(Double, 1000, 1, 6);
    tryFString(Double, 1000, 8, 13);
    tryFString(Double, 10000, 0, 5);
    tryFString(Double, 10000, 1, 7);
    tryFString(Double, 10000, 8, 14);
    tryFString(Double, 100000, 0, 6);
    tryFString(Double, 100000, 1, 8);
    tryFString(Double, 100000, 8, 15);
    tryFString(Double, 1000000, 0, 7);
    tryFString(Double, 1000000, 1, 9);
    tryFString(Double, 1000000, 8, 16);
    tryFString(Double, 10000000, 0, 8);
    tryFString(Double, 10000000, 1, 10);
    tryFString(Double, 10000000, 8, 17);

    testDiag("------------------------------------------------------");
    testDiag("** Negative Double fixed-point **");
    tryFString(Double, -DBL_MIN, 0, 2);
    tryFString(Double, -DBL_MIN, 1, 4);
    tryFString(Double, -DBL_MIN, 8, 11);
    tryFString(Double, -1, 0, 2);
    tryFString(Double, -1, 1, 4);
    tryFString(Double, -1, 8, 11);
    tryFString(Double, -1.0500001, 1, 4);
    tryFString(Double, -1.1,       1, 4);
    tryFString(Double, -1.1499999, 1, 4);
    tryFString(Double, -9.5000001, 0, 3);
    tryFString(Double, -10, 0, 3);
    tryFString(Double, -10, 1, 5);
    tryFString(Double, -10, 8, 12);
    tryFString(Double, -100, 0, 4);
    tryFString(Double, -100, 1, 6);
    tryFString(Double, -100, 8, 13);
    tryFString(Double, -1000, 0, 5);
    tryFString(Double, -1000, 1, 7);
    tryFString(Double, -1000, 8, 14);
    tryFString(Double, -10000, 0, 6);
    tryFString(Double, -10000, 1, 8);
    tryFString(Double, -10000, 8, 15);
    tryFString(Double, -100000, 0, 7);
    tryFString(Double, -100000, 1, 9);
    tryFString(Double, -100000, 8, 16);
    tryFString(Double, -1000000, 0, 8);
    tryFString(Double, -1000000, 1, 10);
    tryFString(Double, -1000000, 8, 17);
    tryFString(Double, -10000000, 0, 9);
    tryFString(Double, -10000000, 1, 11);
    tryFString(Double, -10000000, 8, 18);

    /*
     * Values > 1e7 trigger the %e format.
     * Windows may print 3 digit exponents...
     */
    testDiag("------------------------------------------------------");
    testDiag("** Positive Double scientific **");
    tryFString(Double, 1e+17, 0, 7);
    tryFString(Double, 1e+17, 1, 8);
    tryFString(Double, 1e+17, 2, 9);
    tryFString(Double, 1e+17, 3, 10);
    tryFString(Double, 1e+17, 4, 11);
    tryFString(Double, 1e+17, 5, 12);

    return testDone();
}
