/* $Id$
 *      Author:          Bob Dalesio
 *      Date:            9-21-88
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  01-11-89        lrd     add right and left shift
 * .02  02-01-89        lrd     add trig functions
 * .03  02-17-92        jba     add exp, CEIL, and FLOOR
 * .04  03-03-92        jba     added MAX, MIN, and comma
 * .05  03-06-92        jba     added multiple conditional expressions ?
 * .06  04-02-92        jba     added CONSTANT for floating pt constants in expression
 * .07  05-11-94        jba     added CONST_PI, CONST_D2R, and CONST_R2D
 */

#include "shareLib.h"

/*	defines for element table      */
#define 	FETCH_A		0
#define		FETCH_B		1
#define		FETCH_C		2
#define		FETCH_D		3
#define		FETCH_E		4
#define		FETCH_F		5
#define		FETCH_G		6
#define		FETCH_H		7
#define		FETCH_I		8
#define		FETCH_J		9
#define		FETCH_K		10
#define		FETCH_L		11
#define		ACOS		12
#define		ASIN		13
#define		ATAN		14
#define		COS		15
#define		COSH		16
#define		SIN		17
#define		STORE_A		18
#define		STORE_B		19
#define		STORE_C		20
#define		STORE_D		21
#define		STORE_E		22
#define		STORE_F		23
#define		STORE_G		24
#define		STORE_H		25
#define		STORE_I		26
#define		STORE_J		27
#define		STORE_K		28
#define		STORE_L		29
#define		RIGHT_SHIFT	30
#define		LEFT_SHIFT	31
#define		SINH		32
#define		TAN		33
#define		TANH		34
#define		LOG_2		35
#define		COND_ELSE	36
#define		ABS_VAL		37
#define		UNARY_NEG	38
#define		SQU_RT		39
#define		EXP		40
#define		CEIL		41
#define		FLOOR		42
#define		LOG_10		43
#define		LOG_E		44
#define		RANDOM		45
#define		ADD		46
#define		SUB		47
#define		MULT		48
#define		DIV		49
#define		EXPON		50
#define		MODULO		51
#define		BIT_OR		52
#define		BIT_AND		53
#define		BIT_EXCL_OR	54
#define		GR_OR_EQ	55
#define		GR_THAN		56
#define		LESS_OR_EQ	57
#define		LESS_THAN	58
#define		NOT_EQ		59
#define		EQUAL		60
#define		REL_OR		61
#define		REL_AND		62
#define		REL_NOT		63
#define		BIT_NOT		64
#define		PAREN		65
#define		MAX		66
#define		MIN		67
#define		COMMA		68
#define		COND_IF		69
#define		COND_END	70
#define		CONSTANT	71
#define		CONST_PI	72
#define		CONST_D2R	73
#define		CONST_R2D	74
#define		NINT		75
#define		END_STACK	127

epicsShareFunc long epicsShareAPI 
	calcPerform(double *parg, double *presult, char *post);

epicsShareFunc long epicsShareAPI 
	postfix(register char *pinfix, register char *ppostfix, short *perror);
