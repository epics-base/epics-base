/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* sCalcPostfixPvt.h
 *      Author:          Bob Dalesio
 *      Date:            9-21-88
 */

#ifndef INCpostfixh
#include <sCalcPostfix.h>
#endif
#ifndef INCpostfixPvth
#define INCpostfixPvth

/* #define		BAD_EXPRESSION	0 */

/*	defines for element table      */
/* elements that define a value */
#define 	FETCH		1
#define 	SFETCH		2
#define		CONST_PI	3
#define		CONST_D2R	4
#define		CONST_R2D	5
#define		CONST_S2R	6
#define		CONST_R2S	7
#define		RANDOM		8
#define		LITERAL		9
#define		SLITERAL	10
#define		SSCANF		11

#define		VALUE_ELEMENT 11

/* elements that operate on a value */
#define		ACOS		12
#define		ASIN		13
#define		ATAN		14
#define		COS			15
#define		COSH		16
#define		SIN			17
#define		RIGHT_SHIFT	18
#define		LEFT_SHIFT	19
#define		SINH		20
#define		TAN			21
#define		TANH		22
#define		LOG_2		23
#define		COND_ELSE	24
#define		ABS_VAL		25
#define		UNARY_NEG	26
#define		SQU_RT		27
#define		EXP			28
#define		CEIL		29
#define		FLOOR		30
#define		LOG_10		31
#define		LOG_E		32
#define		ADD			33
#define		SUB			34
#define		MULT		35
#define		DIV			36
#define		EXPON		37
#define		MODULO		38
#define		BIT_OR		39
#define		BIT_AND		40
#define		BIT_EXCL_OR	41
#define		GR_OR_EQ	42
#define		GR_THAN		43
#define		LESS_OR_EQ	44
#define		LESS_THAN	45
#define		NOT_EQ		46
#define		EQUAL		47
#define		REL_OR		48
#define		REL_AND		49
#define		REL_NOT		50
#define		BIT_NOT		51
#define		PAREN		52
#define		MAX_VAL		53
#define		MIN_VAL		54
#define		COMMA		55
#define		COND_IF		56
#define		COND_END	57
#define		NINT		58
#define		ATAN2		59
#define		STORE		60
#define		TO_DOUBLE	61
#define		PRINTF		62
#define		SUBRANGE	63
#define		TO_STRING	64
#define		REPLACE		65
/* #define		END_STACK	127 */

#define USES_STRING 126
#define NO_STRING 125
#endif /* INCpostfixPvth */
