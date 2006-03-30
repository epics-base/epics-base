/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// $Id$
//	Author: Andrew Johnson

#include "epicsUnitTest.h"
#include "epicsMath.h"
#include "epicsAlgorithm.h"
#include "postfix.h"

#if defined(vxWorks) || defined(__rtems__)
  #define MAIN(prog) extern "C" int prog
#else
  #define MAIN(prog) int main
#endif


/* Infrastructure for running tests */

double doCalc(const char *expr) {
    /* Evaluate expression, return result */
    double args[CALCPERFORM_NARGS] = {
	1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0
    };
    char rpn[MAX_POSTFIX_SIZE];
    short err;
    double result = 0.0;
    result /= result;  /* Start as NaN */
    
    if (postfix(expr, rpn, &err)) {
	testDiag("postfix: %s in expression '%s'", calcErrorStr(err), expr);
    } else
	if (calcPerform(args, &result, rpn) && finite(result)) {
	    testDiag("calcPerform: error evaluating '%s'", expr);
	}
    return result;
}

void testCalc(const char *expr, double expected) {
    /* Evaluate expression, test against expected result */
    bool pass = false;
    double args[CALCPERFORM_NARGS] = {
	1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0
    };
    char rpn[MAX_POSTFIX_SIZE];
    short err;
    double result = 0.0;
    result /= result;  /* Start as NaN */
    
    if (postfix(expr, rpn, &err)) {
	testDiag("postfix: %s in expression '%s'", calcErrorStr(err), expr);
    } else
	if (calcPerform(args, &result, rpn) && finite(result)) {
	    testDiag("calcPerform: error evaluating '%s'", expr);
	}
    
    if (finite(expected) && finite(result)) {
	pass = fabs(expected - result) < 1e-8;
    } else if (isnan(expected)) {
	pass = isnan(result);
    } else {
	pass = (result == expected);
    }
    if (!testOk(pass, expr)) {
	testDiag("Expected result is %g, actually got %g", expected, result);
	calcExprDump(rpn);
    }
    return;
}

/* Test an expression that is also valid C code */
#define testExpr(expr) testCalc(#expr, expr);


/* Macros to make some expressions into valid C code */

#ifndef PI
#define PI 3.14159265358979
#endif
#define D2R (PI/180.)
#define R2D (180./PI)
#define ABS(x) fabs(x)
#define AND &
#define ATAN2(x,y) atan2(y,x)
#define LN(x) log(x)
#define LOG(x) log10(x)
#define LOGE(x) log(x)
#define MAX(a,b) epicsMax(a,b)
#define MIN(a,b) epicsMin(a,b)
#define NINT(x) (double)(long)((x) >= 0 ? (x)+0.5 : (x)-0.5)
#define OR |
#define SQR(x) sqrt(x)
#define XOR ^


MAIN(epicsCalcTest) (int /*argc*/, char* /*argv[]*/)
{
    int repeat;
    const double a=1.0, b=2.0, c=3.0, d=4.0, e=5.0, f=6.0,
		 g=7.0, h=8.0, i=9.0, j=10.0, k=11.0, l=12.0;
    double Inf = 1.0;
    double NaN = 0.0;
    Inf /= NaN;
    NaN /= NaN;
    
    testPlan(240);
    
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
    testExpr(finite(0));
    testExpr(finite(Inf));
    testExpr(finite(-Inf));
    testExpr(finite(NaN));
    testExpr(isinf(0));
    testExpr(isinf(Inf));
    testExpr(isinf(-Inf));
    testExpr(isinf(NaN));
    testExpr(isnan(0));
    testExpr(isnan(Inf));
    testExpr(isnan(NaN));
    testExpr(LN(5.));
    testExpr(LOG(5.));
    testExpr(LOGE(2.));
    testExpr(MAX(1,2));
    testExpr(MAX(1.,Inf));
    testExpr(MAX(1.,-Inf));
    testCalc("MAX(1,NaN)", NaN);
    testCalc("MAX(NaN,1)", NaN);
    testExpr(MIN(1,2));
    testExpr(MIN(1.,Inf));
    testExpr(MIN(1.,-Inf));
    testCalc("MIN(1,NaN)", NaN);
    testCalc("MIN(NaN,1)", NaN);
    testExpr(NINT(0.6));
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
    
    testCalc("0 # 1", 0 != 1);
    testCalc("0 # 0", 0 != 0);
    testCalc("1 # 0", 1 != 0);
    testCalc("1 # 0 # 2", 1 != 0 != 2);
    
    testExpr(7 % 4);
    testExpr(-7 % 4);
    testExpr(63 % 16 % 6)
    
    testExpr(7 & 4);
    
    testExpr(0 && 0);
    testExpr(0 && 1);
    testExpr(1 && 0);
    testExpr(1 && 1);
    
    testExpr(2 * 2);
    
    testCalc("2 ** 0.2", pow(2., 0.2));
    testCalc("2 ** -0.2", pow(2., -0.2));
    testCalc("-0.2 ** 2", pow(-0.2, 2.));
    testCalc("-0.2 ** -2", pow(-0.2, -2));
    testCalc("2 ** 2 ** 3", pow(pow(2., 2.), 3.));
    
    testExpr(0 + 1);
    
    testExpr(0 - 1);
    testExpr(0 - 1 - 2);
    
    testExpr(2.0 / 3.0);
    testExpr(1.0 / 2.0 / 3.0);
    
    testExpr(0 < 1);
    testExpr(0 < 0);
    testExpr(1 < 0);
    testExpr(2 < 0 < 2)
    
    testExpr(1 << 2);
    testExpr(1 << 3 << 2)
    
    testExpr(0 <= 1);
    testExpr(0 <= 0);
    testExpr(1 <= 0);
    testExpr(3 <= 2 <= 3)
    
    testCalc("0 = 1", 0 == 1);
    testCalc("0 = 0", 0 == 0);
    testCalc("1 = 0", 1 == 0);
    testCalc("2 = 2 = 1", 2 == 2 == 1);
    
    testExpr(0 == 1);
    testExpr(0 == 0);
    testExpr(1 == 0);
    testExpr(2 == 2 == 1);
    
    testExpr(0 > 1);
    testExpr(0 > 0);
    testExpr(1 > 0);
    testExpr(2 > 0 > 2);
    
    testExpr(0 >= 1);
    testExpr(0 >= 0);
    testExpr(1 >= 0);
    testExpr(3 >= 2 >= 3);
    
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
    testCalc("(1 | 2) ** 3", pow(1 | 2, 3.));	// 8 6
    testCalc("1+(1|2)**3", 1+pow(1 | 2, 3.));	// 8 6
    testExpr(1+(1?(1<2):(1>2))*2);
    
    return testDone();
}

