/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
//	Author: Andrew Johnson

#include <stdlib.h>
#include <string.h>

#include "epicsUnitTest.h"
#include "epicsTypes.h"
#include "epicsMath.h"
#include "epicsAlgorithm.h"
#include "postfix.h"
#include "testMain.h"

/* Infrastructure for running tests */

double doCalc(const char *expr) {
    /* Evaluate expression, return result */
    double args[CALCPERFORM_NARGS] = {
	1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0
    };
    char *rpn = (char*)malloc(INFIX_TO_POSTFIX_SIZE(strlen(expr)+1));
    short err;
    double result = 0.0;
    result /= result;  /* Start as NaN */

    if(!rpn) {
        testAbort("postfix: %s no memory", expr);
        return epicsNAN;
    }

    if (postfix(expr, rpn, &err)) {
	testDiag("postfix: %s in expression '%s'", calcErrorStr(err), expr);
    } else
	if (calcPerform(args, &result, rpn) && finite(result)) {
	    testDiag("calcPerform: error evaluating '%s'", expr);
	}
    free(rpn);
    return result;
}

void testCalc(const char *expr, double expected) {
    /* Evaluate expression, test against expected result */
    bool pass = false;
    double args[CALCPERFORM_NARGS] = {
        1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0
    };
    char *rpn = (char*)malloc(INFIX_TO_POSTFIX_SIZE(strlen(expr)+1));
    short err;
    double result = 0.0;
    result /= result;  /* Start as NaN */

    if(!rpn) {
        testFail("postfix: %s no memory", expr);
        return;
    }

    if (postfix(expr, rpn, &err)) {
        testDiag("postfix: %s in expression '%s'", calcErrorStr(err), expr);
    } else
        if (calcPerform(args, &result, rpn) && finite(result)) {
            testDiag("calcPerform: error evaluating '%s'", expr);
        }

    if (finite(expected) && finite(result)) {
        pass = fabs(expected - result) < 1e-8;
    } else if (isnan(expected)) {
        pass = (bool) isnan(result);
    } else {
        pass = (result == expected);
    }
    if (!testOk(pass, "%s", expr)) {
        testDiag("Expected result is %g, actually got %g", expected, result);
        calcExprDump(rpn);
    }
    free(rpn);
}

void testUInt32Calc(const char *expr, epicsUInt32 expected) {
    /* Evaluate expression, test against expected result */
    bool pass = false;
    double args[CALCPERFORM_NARGS] = {
        1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0
    };
    char *rpn = (char*)malloc(INFIX_TO_POSTFIX_SIZE(strlen(expr)+1));
    short err;
    epicsUInt32 uresult;
    double result = 0.0;
    result /= result;  /* Start as NaN */

    if(!rpn) {
        testFail("postfix: %s no memory", expr);
        return;
    }

    if (postfix(expr, rpn, &err)) {
        testDiag("postfix: %s in expression '%s'", calcErrorStr(err), expr);
    } else
        if (calcPerform(args, &result, rpn) && finite(result)) {
            testDiag("calcPerform: error evaluating '%s'", expr);
        }

    uresult = (epicsUInt32) result;
    pass = (uresult == expected);
    if (!testOk(pass, "%s", expr)) {
        testDiag("Expected result is 0x%x (%u), actually got 0x%x (%u)",
                 expected, expected, uresult, uresult);
        calcExprDump(rpn);
    }
    free(rpn);
}

void testArgs(const char *expr, unsigned long einp, unsigned long eout) {
    char *rpn = (char*)malloc(INFIX_TO_POSTFIX_SIZE(strlen(expr)+1));
    short err = 0;
    unsigned long vinp, vout;

    if(!rpn) {
        testFail("postfix: %s no memory", expr);
        return;
    }

    if (postfix(expr, rpn, &err)) {
        testFail("postfix: %s in expression '%s'", calcErrorStr(err), expr);
        return;
    }
    if (calcArgUsage(rpn, &vinp, &vout)) {
        testFail("calcArgUsage returned error for '%s'", expr);
        return;
    }
    if (!testOk(vinp == einp && vout == eout, "Args for '%s'", expr)) {
        testDiag("Expected (%lx, %lx) got (%lx, %lx)", einp, eout, vinp, vout);
    }
    free(rpn);
}

void testBadExpr(const char *expr, short expected_err) {
    /* Parse an invalid expression, test against expected error code */
    char *rpn = (char*)malloc(INFIX_TO_POSTFIX_SIZE(strlen(expr)+1));
    short err = 0;

    if(!rpn) {
        testFail("postfix: %s no memory", expr);
        return;
    }

    postfix(expr, rpn, &err);
    if (!testOk(err == expected_err, "Bad expression '%s'", expr)) {
        testDiag("Expected '%s', actually got '%s'",
                 calcErrorStr(expected_err), calcErrorStr(err));
        calcExprDump(rpn);
    }
    free(rpn);
}

/* Test an expression that is also valid C code */
#define testExpr(expr) testCalc(#expr, expr);

/* These are the argument bits for testArgs */
#define A_A 0x001
#define A_B 0x002
#define A_C 0x004
#define A_D 0x008
#define A_E 0x010
#define A_F 0x020
#define A_G 0x040
#define A_H 0x080
#define A_I 0x100
#define A_J 0x200
#define A_K 0x400
#define A_L 0x800


/* Macros and functions to make some expressions into valid C code */

#ifndef PI
#define PI 3.14159265358979
#endif
#define Inf epicsINF
#define NaN epicsNAN
#define D2R (PI/180.)
#define R2D (180./PI)
#define ABS(x) fabs(x)
#define AND &
#define ATAN2(x,y) atan2(y,x)
#define LN(x) log(x)
#define LOG(x) log10(x)
#define LOGE(x) log(x)
#define NINT(x) (double)(long)((x) >= 0 ? (x)+0.5 : (x)-0.5)
#define OR |
#define SQR(x) sqrt(x)
#define XOR ^

static inline double MAX(double a) {
    return a;
}
static inline double MAX(double a, double b) {
    return epicsMax(a,b);
}
static inline double MAX(double a, double b, double c) {
    return MAX(MAX(a,b),c);
}
static inline double MAX(double a, double b, double c, double d) {
    return MAX(MAX(a,b,c),d);
}
static inline double MAX(double a, double b, double c, double d, double e) {
    return MAX(MAX(a,b,c,d),e);
}
static inline double MAX(double a, double b, double c, double d, double e,
    double f) {
    return MAX(MAX(a,b,c,d,e),f);
}
static inline double MAX(double a, double b, double c, double d, double e,
    double f, double g) {
    return MAX(MAX(a,b,c,d,e,f),g);
}
static inline double MAX(double a, double b, double c, double d, double e,
    double f, double g, double h) {
    return MAX(MAX(a,b,c,d,e,f,g),h);
}
static inline double MAX(double a, double b, double c, double d, double e,
    double f, double g, double h, double i) {
    return MAX(MAX(a,b,c,d,e,f,g,h),i);
}
static inline double MAX(double a, double b, double c, double d, double e,
    double f, double g, double h, double i, double j) {
    return MAX(MAX(a,b,c,d,e,f,g,h,i),j);
}
static inline double MAX(double a, double b, double c, double d, double e,
    double f, double g, double h, double i, double j, double k) {
    return MAX(MAX(a,b,c,d,e,f,g,h,i,j),k);
}
static inline double MAX(double a, double b, double c, double d, double e,
    double f, double g, double h, double i, double j, double k, double l) {
    return MAX(MAX(a,b,c,d,e,f,g,h,i,j,k),l);
}

static inline double MIN(double a) {
    return a;
}
static inline double MIN(double a, double b) {
    return epicsMin(a,b);
}
static inline double MIN(double a, double b, double c) {
    return MIN(MIN(a,b),c);
}
static inline double MIN(double a, double b, double c, double d) {
    return MIN(MIN(a,b,c),d);
}
static inline double MIN(double a, double b, double c, double d, double e) {
    return MIN(MIN(a,b,c,d),e);
}
static inline double MIN(double a, double b, double c, double d, double e,
    double f) {
    return MIN(MIN(a,b,c,d,e),f);
}
static inline double MIN(double a, double b, double c, double d, double e,
    double f, double g) {
    return MIN(MIN(a,b,c,d,e,f),g);
}
static inline double MIN(double a, double b, double c, double d, double e,
    double f, double g, double h) {
    return MIN(MIN(a,b,c,d,e,f,g),h);
}
static inline double MIN(double a, double b, double c, double d, double e,
    double f, double g, double h, double i) {
    return MIN(MIN(a,b,c,d,e,f,g,h),i);
}
static inline double MIN(double a, double b, double c, double d, double e,
    double f, double g, double h, double i, double j) {
    return MIN(MIN(a,b,c,d,e,f,g,h,i),j);
}
static inline double MIN(double a, double b, double c, double d, double e,
    double f, double g, double h, double i, double j, double k) {
    return MIN(MIN(a,b,c,d,e,f,g,h,i,j),k);
}
static inline double MIN(double a, double b, double c, double d, double e,
    double f, double g, double h, double i, double j, double k, double l) {
    return MIN(MIN(a,b,c,d,e,f,g,h,i,j,k),l);
}

/* The test code below generates lots of spurious warnings because
 * it's making sure that our operator priorities match those of C.
 * Disable them to quieten the compilation process where possible.
 */
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)
#  pragma GCC diagnostic ignored "-Wparentheses"
#endif

MAIN(epicsCalcTest)
{
    int repeat;
    const double a=1.0, b=2.0, c=3.0, d=4.0, e=5.0, f=6.0,
		 g=7.0, h=8.0, i=9.0, j=10.0, k=11.0, l=12.0;
    
    testPlan(613);

    /* LITERAL_OPERAND elements */
    testExpr(0);
    testExpr(1);
    testExpr(2);
    testExpr(3);
    testExpr(4);
    testExpr(5);
    testExpr(6);
    testExpr(7);
    testExpr(8);
    testExpr(9);
    testExpr(.1);
    testExpr(0.1);
    testExpr(0X0);
    testExpr(0x10);
    testExpr(0x7fffffff);
    testCalc("0x80000000", -2147483648.0);
    testCalc("0xffffffff", -1);
    testExpr(Inf);
    testCalc("Infinity", Inf);
    testExpr(NaN);
    
    /* OPERAND elements */
    testExpr(a);
    testExpr(b);
    testExpr(c);
    testExpr(d);
    testExpr(e);
    testExpr(f);
    testExpr(g);
    testExpr(h);
    testExpr(i);
    testExpr(j);
    testExpr(k);
    testExpr(l);
    testExpr(PI);
    testExpr(D2R);
    testExpr(R2D);
    
    for (repeat=0; repeat<100; repeat++) {
	double res = doCalc("rndm");
	if (res<0 || res >1) {
	    testDiag("rndm returned %g", res);
	    break;
	}
    }
    testOk(repeat == 100, "rndm");
    
    /* UNARY_MINUS element */
    testExpr(-1);
    testExpr(-Inf);
    testExpr(- -1);
    testCalc("-0x80000000", 2147483648.0);
    
    /* UNARY_OPERATOR elements */
    testExpr((1));
    testExpr(!0);
    testExpr(!1);
    testExpr(!!0);
    testExpr(ABS(1.0));
    testExpr(ABS(-1.));
    testExpr(acos(1.));
    testExpr(asin(0.5));
    testExpr(atan(0.5));
    testExpr(ATAN2(1., 2.));
    testExpr(ceil(0.5));
    testExpr(cos(0.5));
    testExpr(cosh(0.5));
    testExpr(exp(1.));
    testExpr(floor(1.5));

    testExpr(finite(0.));
    testExpr(finite(Inf));
    testExpr(finite(-Inf));
    testExpr(finite(NaN));
    testCalc("finite(0,1,2)", 1);
    testCalc("finite(0,1,NaN)", 0);
    testCalc("finite(0,NaN,2)", 0);
    testCalc("finite(NaN,1,2)", 0);
    testCalc("finite(0,1,Inf)", 0);
    testCalc("finite(0,Inf,2)", 0);
    testCalc("finite(Inf,1,2)", 0);
    testCalc("finite(0,1,-Inf)", 0);
    testCalc("finite(0,-Inf,2)", 0);
    testCalc("finite(-Inf,1,2)", 0);
    testExpr(isinf(0.));
    testExpr(isinf(Inf));
    testExpr(!!isinf(-Inf));    // Some GCCs return -1/0/+1 rather than 0/+1
    testExpr(isinf(NaN));
    testExpr(isnan(0.));
    testExpr(isnan(Inf));
    testExpr(isnan(-Inf));
    testExpr(isnan(NaN));
    testCalc("isnan(0,1,2)", 0);
    testCalc("isnan(0,1,NaN)", 1);
    testCalc("isnan(0,NaN,2)", 1);
    testCalc("isnan(NaN,1,2)", 1);
    testCalc("isnan(0,1,Inf)", 0);
    testCalc("isnan(0,Inf,2)", 0);
    testCalc("isnan(Inf,1,2)", 0);
    testCalc("isnan(0,1,-Inf)", 0);
    testCalc("isnan(0,-Inf,2)", 0);
    testCalc("isnan(-Inf,1,2)", 0);

    testExpr(LN(5.));
    testExpr(LOG(5.));
    testExpr(LOGE(2.));

    testExpr(MAX(-99));
    testExpr(MAX( 1., 2.));
    testExpr(MAX( 1., Inf));
    testExpr(MAX( 1.,-Inf));
    testExpr(MAX( 1., NaN));
    testExpr(MAX( Inf, 1.));
    testExpr(MAX(-Inf, 1.));
    testExpr(MAX( NaN, 1.));
    testExpr(MAX( 1., 2.,3.));
    testExpr(MAX( 1., 3.,2.));
    testExpr(MAX( 2., 1.,3.));
    testExpr(MAX( 2., 3.,1.));
    testExpr(MAX( 3., 1.,2.));
    testExpr(MAX( 3., 2.,1.));
    testExpr(MAX( 1., 2., Inf));
    testExpr(MAX( 1., 2.,-Inf));
    testExpr(MAX( 1., 2., NaN));
    testExpr(MAX( 1., Inf,2.));
    testExpr(MAX( 1.,-Inf,2.));
    testExpr(MAX( 1., NaN,2.));
    testExpr(MAX( Inf, 1.,2.));
    testExpr(MAX(-Inf, 1.,2.));
    testExpr(MAX( NaN, 1.,2.));
    testExpr(MAX( 1., 2., 3., 4.));
    testExpr(MAX( 1., 2., 4., 3.));
    testExpr(MAX( 1., 4., 3., 2.));
    testExpr(MAX( 4., 2., 3., 1.));
    testExpr(MAX( 1., 2., 3.,NaN));
    testExpr(MAX( 1., 2.,NaN, 3.));
    testExpr(MAX( 1.,NaN, 3., 2.));
    testExpr(MAX(NaN, 2., 3., 1.));
    testExpr(MAX( 1., 2., 3., 4., 5.));
    testExpr(MAX( 1., 2., 3., 5., 4.));
    testExpr(MAX( 1., 2., 5., 4., 3.));
    testExpr(MAX( 1., 5., 3., 4., 2.));
    testExpr(MAX( 5., 2., 3., 4., 1.));
    testExpr(MAX( 1., 2., 3., 4.,NaN));
    testExpr(MAX( 1., 2., 3.,NaN, 4.));
    testExpr(MAX( 1., 2.,NaN, 4., 3.));
    testExpr(MAX( 1.,NaN, 3., 4., 2.));
    testExpr(MAX(NaN, 2., 3., 4., 1.));
    testExpr(MAX( 1., 2., 3., 4., 5., 6.));
    testExpr(MAX( 1., 2., 3., 4., 6., 5.));
    testExpr(MAX( 1., 2., 3., 6., 5., 4.));
    testExpr(MAX( 1., 2., 6., 4., 5., 3.));
    testExpr(MAX( 1., 6., 3., 4., 5., 2.));
    testExpr(MAX( 6., 2., 3., 4., 5., 1.));
    testExpr(MAX( 1., 2., 3., 4., 5.,NaN));
    testExpr(MAX( 1., 2., 3., 4.,NaN, 5.));
    testExpr(MAX( 1., 2., 3.,NaN, 5., 4.));
    testExpr(MAX( 1., 2.,NaN, 4., 5., 3.));
    testExpr(MAX( 1.,NaN, 3., 4., 5., 2.));
    testExpr(MAX(NaN, 2., 3., 4., 5., 1.));
    testExpr(MAX( 1., 2., 3., 4., 5.,Inf));
    testExpr(MAX( 1., 2., 3., 4.,Inf, 5.));
    testExpr(MAX( 1., 2., 3.,Inf, 5., 4.));
    testExpr(MAX( 1., 2.,Inf, 4., 5., 3.));
    testExpr(MAX( 1.,Inf, 3., 4., 5., 2.));
    testExpr(MAX(Inf, 2., 3., 4., 5., 1.));
    testExpr(MAX(1,2,3,4,5,6,7,8,9,10,11,12));
    testExpr(MAX(5,4,3,2,1,0,-1,-2,-3,-4,-5,-6));
    testExpr(MAX(-1,1,0));

    testExpr(MIN(99));
    testExpr(MIN(1.,2.));
    testExpr(MIN(1.,Inf));
    testExpr(MIN(1.,-Inf));
    testExpr(MIN(1.,NaN));
    testExpr(MIN(NaN,1.));
    testExpr(MIN( 1., 2.,3.));
    testExpr(MIN( 1., 3.,2.));
    testExpr(MIN( 2., 1.,3.));
    testExpr(MIN( 2., 3.,1.));
    testExpr(MIN( 3., 1.,2.));
    testExpr(MIN( 3., 2.,1.));
    testExpr(MIN( 1., 2., Inf));
    testExpr(MIN( 1., 2.,-Inf));
    testExpr(MIN( 1., 2., NaN));
    testExpr(MIN( 1., Inf,2.));
    testExpr(MIN( 1.,-Inf,2.));
    testExpr(MIN( 1., NaN,2.));
    testExpr(MIN( Inf, 1.,2.));
    testExpr(MIN(-Inf, 1.,2.));
    testExpr(MIN( NaN, 1.,2.));
    testExpr(MIN( 1., 2., 3., 4.));
    testExpr(MIN( 1., 2., 4., 3.));
    testExpr(MIN( 1., 4., 3., 2.));
    testExpr(MIN( 4., 2., 3., 1.));
    testExpr(MIN( 1., 2., 3.,NaN));
    testExpr(MIN( 1., 2.,NaN, 3.));
    testExpr(MIN( 1.,NaN, 3., 2.));
    testExpr(MIN(NaN, 2., 3., 1.));
    testExpr(MIN( 1., 2., 3., 4., 5.));
    testExpr(MIN( 1., 2., 3., 5., 4.));
    testExpr(MIN( 1., 2., 5., 4., 3.));
    testExpr(MIN( 1., 5., 3., 4., 2.));
    testExpr(MIN( 5., 2., 3., 4., 1.));
    testExpr(MIN( 1., 2., 3., 4.,NaN));
    testExpr(MIN( 1., 2., 3.,NaN, 4.));
    testExpr(MIN( 1., 2.,NaN, 4., 3.));
    testExpr(MIN( 1.,NaN, 3., 4., 2.));
    testExpr(MIN(NaN, 2., 3., 4., 1.));
    testExpr(MIN( 1., 2., 3., 4., 5., 6.));
    testExpr(MIN( 2., 1., 3., 4., 5., 6.));
    testExpr(MIN( 3., 2., 1., 4., 5., 6.));
    testExpr(MIN( 4., 2., 3., 1., 5., 6.));
    testExpr(MIN( 5., 2., 3., 4., 1., 6.));
    testExpr(MIN( 6., 2., 3., 4., 5., 1.));
    testExpr(MIN( 1., 2., 3., 4., 5.,NaN));
    testExpr(MIN( 1., 2., 3., 4.,NaN, 5.));
    testExpr(MIN( 1., 2., 3.,NaN, 5., 4.));
    testExpr(MIN( 1., 2.,NaN, 4., 5., 3.));
    testExpr(MIN( 1.,NaN, 3., 4., 5., 2.));
    testExpr(MIN(NaN, 2., 3., 4., 5., 1.));
    testExpr(MIN( 1., 2., 3., 4., 5.,-Inf));
    testExpr(MIN( 1., 2., 3., 4.,-Inf, 5.));
    testExpr(MIN( 1., 2., 3.,-Inf, 5., 4.));
    testExpr(MIN( 1., 2.,-Inf, 4., 5., 3.));
    testExpr(MIN( 1.,-Inf, 3., 4., 5., 2.));
    testExpr(MIN(-Inf, 2., 3., 4., 5., 1.));
    testExpr(MIN(1,2,3,4,5,6,7,8,9,10,11,12));
    testExpr(MIN(5,4,3,2,1,0,-1,-2,-3,-4,-5,-6));
    testExpr(MIN(1,-1,0));
    testExpr(MAX(MIN(0,2),MAX(0),MIN(3,2,1)));

    testExpr(NINT(0.4));
    testExpr(NINT(0.6));
    testExpr(NINT(-0.4));
    testExpr(NINT(-0.6));
    testExpr(sin(0.5));
    testExpr(sinh(0.5));
    testExpr(SQR(10.));
    testExpr(sqrt(16.));
    testExpr(tan(0.5));
    testExpr(tanh(0.5));
    testExpr(~5);
    testExpr(~~5);
    
    /* BINARY_OPERATOR elements */
    testExpr(0 != 1);
    testExpr(0 != 0);
    testExpr(1 != 0);
    testExpr(1 != 0 != 2);
    testExpr(0.0 != Inf);
    testExpr(0.0 != -Inf);
    testExpr(0.0 != NaN);
    testExpr(Inf != 0.0);
    testExpr(Inf != Inf);
    testExpr(Inf != -Inf);
    testExpr(Inf != NaN);
    testExpr(-Inf != 0.0);
    testExpr(-Inf != Inf);
    testExpr(-Inf != -Inf);
    testExpr(-Inf != NaN);
    testExpr(NaN != 0.0);
    testExpr(NaN != Inf);
    testExpr(NaN != -Inf);
    testExpr(NaN != NaN);
    
    testCalc("0 # 1", 0 != 1);
    testCalc("0 # 0", 0 != 0);
    testCalc("1 # 0", 1 != 0);
    testCalc("1 # 0 # 2", 1 != 0 != 2);
    
    testExpr(7 % 4);
    testExpr(-7 % 4);
    testExpr(63 % 16 % 6)
    testCalc("1 % 0", NaN);
    
    testExpr(7 & 4);
    
    testExpr(0 && 0);
    testExpr(0 && 1);
    testExpr(1 && 0);
    testExpr(1 && 1);
    
    testExpr(2 * 2);
    testExpr(0.0 * Inf);
    testExpr(0.0 * -Inf);
    testExpr(0.0 * NaN);
    testExpr(Inf * 0.0);
    testExpr(Inf * Inf);
    testExpr(Inf * -Inf);
    testExpr(Inf * NaN);
    testExpr(-Inf * 0.0);
    testExpr(-Inf * Inf);
    testExpr(-Inf * -Inf);
    testExpr(-Inf * NaN);
    testExpr(NaN * 0.0);
    testExpr(NaN * Inf);
    testExpr(NaN * -Inf);
    testExpr(NaN * NaN);
    
    testCalc("2 ** 0.2", pow(2., 0.2));
    testCalc("2 ** -0.2", pow(2., -0.2));
    testCalc("-0.2 ** 2", pow(-0.2, 2.));
    testCalc("-0.2 ** -2", pow(-0.2, -2));
    testCalc("2 ** 2 ** 3", pow(pow(2., 2.), 3.));
    
    testExpr(0 + 1);
    testExpr(0.0 + Inf);
    testExpr(0.0 + -Inf);
    testExpr(0.0 + NaN);
    testExpr(Inf + 0.0);
    testExpr(Inf + Inf);
#if defined(_WIN64) && defined(_MSC_VER)
    testCalc("Inf + -Inf", NaN);
#else
    testExpr(Inf + -Inf);
#endif
    testExpr(Inf + NaN);
    testExpr(-Inf + 0.0);
#if defined(_WIN64) && defined(_MSC_VER)
    testCalc("-Inf + Inf", NaN);
#else
    testExpr(-Inf + Inf);
#endif
    testExpr(-Inf + -Inf);
    testExpr(-Inf + NaN);
    testExpr(NaN + 0.0);
    testExpr(NaN + Inf);
    testExpr(NaN + -Inf);
    testExpr(NaN + NaN);
    
    testExpr(0 - 1);
    testExpr(0 - 1 - 2);
    testExpr(0.0 - Inf);
    testExpr(0.0 - -Inf);
    testExpr(0.0 - NaN);
    testExpr(Inf - 0.0);
    testExpr(Inf - Inf);
    testExpr(Inf - -Inf);
    testExpr(Inf - NaN);
    testExpr(-Inf - 0.0);
    testExpr(-Inf - Inf);
    testExpr(-Inf - -Inf);
    testExpr(-Inf - NaN);
    testExpr(NaN - 0.0);
    testExpr(NaN - Inf);
    testExpr(NaN - -Inf);
    testExpr(NaN - NaN);
    
    testExpr(2.0 / 3.0);
    testExpr(1.0 / 2.0 / 3.0);
    testExpr(0.0 / Inf);
    testExpr(0.0 / -Inf);
    testExpr(0.0 / NaN);
    testExpr(Inf / 1.0);
    testExpr(Inf / Inf);
    testExpr(Inf / -Inf);
    testExpr(Inf / NaN);
    testExpr(-Inf / 1.0);
    testExpr(-Inf / Inf);
    testExpr(-Inf / -Inf);
    testExpr(-Inf / NaN);
    testExpr(NaN / 1.0);
    testExpr(NaN / Inf);
    testExpr(NaN / -Inf);
    testExpr(NaN / NaN);
    
    testExpr(0 < 1);
    testExpr(0 < 0);
    testExpr(1 < 0);
    testExpr(2 < 0 < 2)
    testExpr(0.0 < Inf);
    testExpr(0.0 < -Inf);
    testExpr(0.0 < NaN);
    testExpr(Inf < 0.0);
    testExpr(Inf < Inf);
    testExpr(Inf < -Inf);
    testExpr(Inf < NaN);
    testExpr(-Inf < 0.0);
    testExpr(-Inf < Inf);
    testExpr(-Inf < -Inf);
    testExpr(-Inf < NaN);
    testExpr(NaN < 0.0);
    testExpr(NaN < Inf);
    testExpr(NaN < -Inf);
    testExpr(NaN < NaN);
    
    testExpr(1 << 2);
    testExpr(1 << 3 << 2)
    
    testExpr(0 <= 1);
    testExpr(0 <= 0);
    testExpr(1 <= 0);
    testExpr(3 <= 2 <= 3)
    testExpr(0.0 <= Inf);
    testExpr(0.0 <= -Inf);
    testExpr(0.0 <= NaN);
    testExpr(Inf <= 0.0);
    testExpr(Inf <= Inf);
    testExpr(Inf <= -Inf);
    testExpr(Inf <= NaN);
    testExpr(-Inf <= 0.0);
    testExpr(-Inf <= Inf);
    testExpr(-Inf <= -Inf);
    testExpr(-Inf <= NaN);
    testExpr(NaN <= 0.0);
    testExpr(NaN <= Inf);
    testExpr(NaN <= -Inf);
    testExpr(NaN <= NaN);
    
    testCalc("0 = 1", 0 == 1);
    testCalc("0 = 0", 0 == 0);
    testCalc("1 = 0", 1 == 0);
    testCalc("2 = 2 = 1", 2 == 2 == 1);
    
    testExpr(0 == 1);
    testExpr(0 == 0);
    testExpr(1 == 0);
    testExpr(2 == 2 == 1);
    testExpr(0.0 == Inf);
    testExpr(0.0 == -Inf);
    testExpr(0.0 == NaN);
    testExpr(Inf == 0.0);
    testExpr(Inf == Inf);
    testExpr(Inf == -Inf);
    testExpr(Inf == NaN);
    testExpr(-Inf == 0.0);
    testExpr(-Inf == Inf);
    testExpr(-Inf == -Inf);
    testExpr(-Inf == NaN);
    testExpr(NaN == 0.0);
    testExpr(NaN == Inf);
    testExpr(NaN == -Inf);
    testExpr(NaN == NaN);
    
    testExpr(0 > 1);
    testExpr(0 > 0);
    testExpr(1 > 0);
    testExpr(2 > 0 > 2);
    testExpr(0.0 > Inf);
    testExpr(0.0 > -Inf);
    testExpr(0.0 > NaN);
    testExpr(Inf > 0.0);
    testExpr(Inf > Inf);
    testExpr(Inf > -Inf);
    testExpr(Inf > NaN);
    testExpr(-Inf > 0.0);
    testExpr(-Inf > Inf);
    testExpr(-Inf > -Inf);
    testExpr(-Inf > NaN);
    testExpr(NaN > 0.0);
    testExpr(NaN > Inf);
    testExpr(NaN > -Inf);
    testExpr(NaN > NaN);
    
    testExpr(0 >= 1);
    testExpr(0 >= 0);
    testExpr(1 >= 0);
    testExpr(3 >= 2 >= 3);
    testExpr(0.0 >= Inf);
    testExpr(0.0 >= -Inf);
    testExpr(0.0 >= NaN);
    testExpr(Inf >= 0.0);
    testExpr(Inf >= Inf);
    testExpr(Inf >= -Inf);
    testExpr(Inf >= NaN);
    testExpr(-Inf >= 0.0);
    testExpr(-Inf >= Inf);
    testExpr(-Inf >= -Inf);
    testExpr(-Inf >= NaN);
    testExpr(NaN >= 0.0);
    testExpr(NaN >= Inf);
    testExpr(NaN >= -Inf);
    testExpr(NaN >= NaN);
    
    testExpr(8 >> 1);
    testExpr(64 >> 2 >> 1);
    
    testExpr(7 AND 4);
    
    testExpr(1 OR 8);
    
    testExpr(3 XOR 9);
    
    testCalc("2 ^ 0.2", pow(2., 0.2));
    testCalc("2 ^ -0.2", pow(2., -0.2));
    testCalc("(-0.2) ^ 2", pow(-0.2, 2.));
    testCalc("(-0.2) ^ -2", pow(-0.2, -2.));
    testCalc("2 ^ 2 ^ 3", pow(pow(2., 2.), 3.));
    
    testExpr(1 | 8);
    
    testExpr(0 || 0);
    testExpr(0 || 1);
    testExpr(1 || 0);
    testExpr(1 || 1);
    
    /* CONDITIONAL elements */
    testExpr(0 ? 1 : 2);
    testExpr(1 ? 1 : 2);
    testExpr(Inf ? 1 : 2);
    testExpr(NaN ? 1 : 2);
    testExpr(0 ? 0 ? 2 : 3 : 4);
    testExpr(0 ? 1 ? 2 : 3 : 4);
    testExpr(1 ? 0 ? 2 : 3 : 4);
    testExpr(1 ? 1 ? 2 : 3 : 4);
    testExpr(0 ? 2 : 0 ? 3 : 4);
    testExpr(0 ? 2 : 1 ? 3 : 4);
    testExpr(1 ? 2 : 0 ? 3 : 4);
    testExpr(1 ? 2 : 1 ? 3 : 4);
    
    /* STORE_OPERATOR and EXPR_TERM elements*/
    testCalc("a := 0; a", 0);
    testCalc("b := 0; b", 0);
    testCalc("c := 0; c", 0);
    testCalc("d := 0; d", 0);
    testCalc("e := 0; e", 0);
    testCalc("f := 0; f", 0);
    testCalc("g := 0; g", 0);
    testCalc("h := 0; h", 0);
    testCalc("i := 0; i", 0);
    testCalc("j := 0; j", 0);
    testCalc("k := 0; k", 0);
    testCalc("l := 0; l", 0);
    
    testCalc("a; a := 0", a);
    testCalc("b; b := 0", b);
    testCalc("c; c := 0", c);
    testCalc("d; d := 0", d);
    testCalc("e; e := 0", e);
    testCalc("f; f := 0", f);
    testCalc("g; g := 0", g);
    testCalc("h; h := 0", h);
    testCalc("i; i := 0", i);
    testCalc("j; j := 0", j);
    testCalc("k; k := 0", k);
    testCalc("l; l := 0", l);
    
    // Check relative precedences.
    testExpr(0 ? 1 : 2 | 4);			// 0 1
    testExpr(1 ? 1 : 2 | 4);			// 0 1
    testExpr(0 ? 2 | 4 : 1);			// 0 1
    testExpr(1 ? 2 | 4 : 1);			// 0 1
    testExpr(0 ? 1 : 2 & 3);			// 0 2
    testExpr(1 ? 1 : 2 & 3);			// 0 2
    testExpr(0 ? 2 & 3 : 1);			// 0 2
    testExpr(1 ? 2 & 3 : 1);			// 0 2
    testExpr(0 ? 2 : 3 >= 1);			// 0 3
    testExpr(0 ? 3 >= 1 : 2);			// 0 3
    testExpr(1 ? 0 == 1 : 2);			// 0 3
    testExpr(1 ? 2 : 0 == 1);			// 0 3
    testExpr(0 ? 1 : 2 + 4);			// 0 4
    testExpr(1 ? 1 : 2 + 4);			// 0 4
    testExpr(0 ? 2 + 4 : 1);			// 0 4
    testExpr(1 ? 2 + 4 : 1);			// 0 4
    testExpr(0 ? 1 : 2 * 4);			// 0 5
    testExpr(1 ? 1 : 2 * 4);			// 0 5
    testExpr(0 ? 2 * 4 : 1);			// 0 5
    testExpr(1 ? 2 * 4 : 1);			// 0 5
    testCalc("0 ? 1 : 2 ** 3", 8);		// 0 6
    testCalc("1 ? 1 : 2 ** 3", 1);		// 0 6
    testCalc("0 ? 2 ** 3 : 1", 1);		// 0 6
    testCalc("1 ? 2 ** 3 : 1", 8);		// 0 6
    testCalc("1 | 3 XOR 1", (1 | 3) ^ 1);	// 1 1
    testExpr(1 XOR 3 | 1);			// 1 1
    testExpr(3 | 1 & 2);			// 1 2
    testExpr(2 | 4 > 3);			// 1 3
    testExpr(2 OR 4 > 3);			// 1 3
    testExpr(2 XOR 3 >= 0);			// 1 3
    testExpr(2 | 1 - 3);			// 1 4
    testExpr(2 | 4 / 2);			// 1 5
    testCalc("1 | 2 ** 3", 1 | (int) pow(2., 3.));// 1 6
    testExpr(3 << 2 & 10);			// 2 2
    testCalc("18 & 6 << 2", (18 & 6) << 2);	// 2 2
    testExpr(36 >> 2 & 10);			// 2 2
    testCalc("18 & 20 >> 2", (18 & 20) >> 2);	// 2 2
    testExpr(3 & 4 == 4);			// 2 3
    testExpr(3 AND 4 == 4);			// 2 3
    testCalc("1 << 2 != 4", 1 << (2 != 4));	// 2 3
    testCalc("16 >> 2 != 4", 16 >> (2 != 4));	// 2 3
    testExpr(3 AND -2); 			// 2 8
    testExpr(0 < 1 ? 2 : 3);			// 3 0
    testExpr(1 <= 0 ? 2 : 3);			// 3 0
    testExpr(0 + -1);				// 4 8
    testExpr(0 - -1);				// 4 8
    testExpr(10 + 10 * 2);			// 4 5
    testExpr(20 + 20 / 2);			// 4 5
    testExpr(-1 + 1);				// 7 4
    testExpr(-1 - 2);				// 7 4
    testCalc("-2 ** 2", pow(-2., 2.));		// 7 6
    testCalc("-2 ^ 2", pow(-2., 2.));		// 7 6
    
    // Check parentheses
    testCalc("(1 | 2) ** 3", pow((double) (1 | 2), 3.));// 8 6
    testCalc("1+(1|2)**3", 1+pow((double) (1 | 2), 3.));// 8 6
    testExpr(1+(1?(1<2):(1>2))*2);
    
    testArgs("a", A_A, 0);
    testArgs("A", A_A, 0);
    testArgs("B", A_B, 0);
    testArgs("C", A_C, 0);
    testArgs("D", A_D, 0);
    testArgs("E", A_E, 0);
    testArgs("F", A_F, 0);
    testArgs("G", A_G, 0);
    testArgs("H", A_H, 0);
    testArgs("I", A_I, 0);
    testArgs("J", A_J, 0);
    testArgs("K", A_K, 0);
    testArgs("L", A_L, 0);
    testArgs("A+B+C+D+E+F+G+H+I+J+K+L",
	A_A|A_B|A_C|A_D|A_E|A_F|A_G|A_H|A_I|A_J|A_K|A_L, 0);
    testArgs("0.1;A:=0", 0, A_A);
    testArgs("1.1;B:=0", 0, A_B);
    testArgs("2.1;C:=0", 0, A_C);
    testArgs("3.1;D:=0", 0, A_D);
    testArgs("4.1;E:=0", 0, A_E);
    testArgs("5.1;F:=0", 0, A_F);
    testArgs("6.1;G:=0", 0, A_G);
    testArgs("7.1;H:=0", 0, A_H);
    testArgs("8.1;I:=0", 0, A_I);
    testArgs("9.1;J:=0", 0, A_J);
    testArgs("10.1;K:=0", 0, A_K);
    testArgs("11.1;L:=0", 0, A_L);
    testArgs("12.1;A:=0;B:=A;C:=B;D:=C", 0, A_A|A_B|A_C|A_D);
    testArgs("13.1;B:=A;A:=B;C:=D;D:=C", A_A|A_D, A_A|A_B|A_C|A_D);
    
    // Malformed expressions
    testBadExpr("0x0.1", CALC_ERR_SYNTAX);
    testBadExpr("1*", CALC_ERR_INCOMPLETE);
    testBadExpr("*1", CALC_ERR_SYNTAX);
    testBadExpr("MIN", CALC_ERR_INCOMPLETE);
    testBadExpr("MIN()", CALC_ERR_SYNTAX);
    testBadExpr("MIN(A,)", CALC_ERR_SYNTAX);
    testBadExpr("MIN(A,B,)", CALC_ERR_SYNTAX);
    testBadExpr("MAX", CALC_ERR_INCOMPLETE);
    testBadExpr("MAX()", CALC_ERR_SYNTAX);
    testBadExpr("MAX(A,)", CALC_ERR_SYNTAX);
    testBadExpr("MAX(A,B,)", CALC_ERR_SYNTAX);
    testBadExpr("1?", CALC_ERR_CONDITIONAL);
    testBadExpr("1?1", CALC_ERR_CONDITIONAL);
    testBadExpr(":1", CALC_ERR_SYNTAX);

    // Bit manipulations wrt bit 31 (bug lp:1514520)
    //   using integer literals
    testUInt32Calc("0xaaaaaaaa AND 0xffff0000", 0xaaaa0000u);
    testUInt32Calc("0xaaaaaaaa OR 0xffff0000", 0xffffaaaau);
    testUInt32Calc("0xaaaaaaaa XOR 0xffff0000", 0x5555aaaau);
    testUInt32Calc("~0xaaaaaaaa", 0x55555555u);
    testUInt32Calc("~~0xaaaaaaaa", 0xaaaaaaaau);
    testUInt32Calc("0xaaaaaaaa >> 8", 0xffaaaaaau);
    testUInt32Calc("0xaaaaaaaa << 8", 0xaaaaaa00u);
    //   using integer literals assigned to variables
    testUInt32Calc("a:=0xaaaaaaaa; b:=0xffff0000; a AND b", 0xaaaa0000u);
    testUInt32Calc("a:=0xaaaaaaaa; b:=0xffff0000; a OR b", 0xffffaaaau);
    testUInt32Calc("a:=0xaaaaaaaa; b:=0xffff0000; a XOR b", 0x5555aaaau);
    testUInt32Calc("a:=0xaaaaaaaa; ~a", 0x55555555u);
    testUInt32Calc("a:=0xaaaaaaaa; ~~a", 0xaaaaaaaau);
    testUInt32Calc("a:=0xaaaaaaaa; a >> 8", 0xffaaaaaau);
    testUInt32Calc("a:=0xaaaaaaaa; a << 8", 0xaaaaaa00u);

    // Test proper conversion of double values (+ 0.1 enforces double literal)
    // when used as inputs to the bitwise operations.
    //      0xaaaaaaaa = -1431655766 or 2863311530u
    testUInt32Calc("-1431655766.1 OR 0", 0xaaaaaaaau);
    testUInt32Calc("2863311530.1 OR 0", 0xaaaaaaaau);
    testUInt32Calc("0 OR -1431655766.1", 0xaaaaaaaau);
    testUInt32Calc("0 OR 2863311530.1", 0xaaaaaaaau);
    testUInt32Calc("-1431655766.1 XOR 0", 0xaaaaaaaau);
    testUInt32Calc("2863311530.1 XOR 0", 0xaaaaaaaau);
    testUInt32Calc("0 XOR -1431655766.1", 0xaaaaaaaau);
    testUInt32Calc("0 XOR 2863311530.1", 0xaaaaaaaau);
    testUInt32Calc("-1431655766.1 AND 0xffffffff", 0xaaaaaaaau);
    testUInt32Calc("2863311530.1 AND 0xffffffff", 0xaaaaaaaau);
    testUInt32Calc("0xffffffff AND -1431655766.1", 0xaaaaaaaau);
    testUInt32Calc("0xffffffff AND 2863311530.1", 0xaaaaaaaau);
    testUInt32Calc("~ -1431655766.1", 0x55555555u);
    testUInt32Calc("~ 2863311530.1", 0x55555555u);
    testUInt32Calc("-1431655766.1 >> 0", 0xaaaaaaaau);
    testUInt32Calc("2863311530.1 >> 0", 0xaaaaaaaau);
    testUInt32Calc("-1431655766.1 >> 0.1", 0xaaaaaaaau);
    testUInt32Calc("2863311530.1 >> 0.1", 0xaaaaaaaau);
    testUInt32Calc("-1431655766.1 << 0", 0xaaaaaaaau);
    testUInt32Calc("2863311530.1 << 0", 0xaaaaaaaau);
    testUInt32Calc("-1431655766.1 << 0.1", 0xaaaaaaaau);
    testUInt32Calc("2863311530.1 << 0.1", 0xaaaaaaaau);

    return testDone();
}

