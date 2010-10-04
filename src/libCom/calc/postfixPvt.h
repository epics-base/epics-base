/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* postfixPvt.h
 *      Original Author: Bob Dalesio
 *      Date:            9-21-88
 */

/* Notes:
 *  1. The FETCH_A through FETCH_L and STORE_A through STORE_L opcodes must
 *     be contiguous.
 *  2. The LITERAL opcodes are followed by a binary representation of their
 *     values, but these are not aligned properly.
 *  3. The var-arg functions MIN, MAX, FINITE and ISNAN are followed by
 *     a byte giving the number of arguments to process.
 *  4. You can't use strlen() on an RPN buffer since the literal values
 *     can contain zero bytes.
 */

#ifndef INCpostfixPvth
#define INCpostfixPvth


/* RPN opcodes */
typedef enum {
	END_EXPRESSION = 0,
    /* Operands */
	LITERAL_DOUBLE, LITERAL_INT, FETCH_VAL,
	FETCH_A, FETCH_B, FETCH_C, FETCH_D, FETCH_E, FETCH_F,
	FETCH_G, FETCH_H, FETCH_I, FETCH_J, FETCH_K, FETCH_L,
    /* Assignment */
	STORE_A, STORE_B, STORE_C, STORE_D, STORE_E, STORE_F,
	STORE_G, STORE_H, STORE_I, STORE_J, STORE_K, STORE_L,
    /* Trigonometry Constants */
	CONST_PI,
	CONST_D2R,
	CONST_R2D,
    /* Arithmetic */
	UNARY_NEG,
	ADD,
	SUB,
	MULT,
	DIV,
	MODULO,
	POWER,
    /* Algebraic */
	ABS_VAL,
	EXP,
	LOG_10,
	LOG_E,
	MAX,
	MIN,
	SQU_RT,
    /* Trigonometric */
	ACOS,
	ASIN,
	ATAN,
	ATAN2,
	COS,
	COSH,
	SIN,
	SINH,
	TAN,
	TANH,
    /* Numeric */
	CEIL,
	FLOOR,
	FINITE,
	ISINF,
	ISNAN,
	NINT,
	RANDOM,
    /* Boolean */
	REL_OR,
	REL_AND,
	REL_NOT,
    /* Bitwise */
	BIT_OR,
	BIT_AND,
	BIT_EXCL_OR,
	BIT_NOT,
	RIGHT_SHIFT,
	LEFT_SHIFT,
    /* Relationals */
	NOT_EQ,
	LESS_THAN,
	LESS_OR_EQ,
	EQUAL,
	GR_OR_EQ,
	GR_THAN,
    /* Conditional */
	COND_IF,
	COND_ELSE,
	COND_END,
    /* Misc */
	NOT_GENERATED
} rpn_opcode;

#endif /* INCpostfixPvth */
