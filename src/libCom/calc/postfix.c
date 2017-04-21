/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Subroutines used to convert an infix expression to a postfix expression
 *
 *      Original Author: Bob Dalesio
 *      Date:            12-12-86
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "epicsAssert.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "epicsTypes.h"
#include "postfix.h"
#include "postfixPvt.h"
#include "shareLib.h"


/* declarations for postfix */

/* element types */
typedef enum {
	OPERAND,
	LITERAL_OPERAND,
	STORE_OPERATOR,
	UNARY_OPERATOR,
	VARARG_OPERATOR,
	BINARY_OPERATOR,
	SEPERATOR,
	CLOSE_PAREN,
	CONDITIONAL,
	EXPR_TERMINATOR,
} element_type;


/* element table
 *
 * structure of an element
 */
typedef struct expression_element{
    char *name; 	 /* character representation of an element */
    char in_stack_pri;	 /* priority on translation stack */
    char in_coming_pri;  /* priority in input string */
    signed char runtime_effect; /* stack change, positive means push */
    element_type type;	 /* element type */
    rpn_opcode code;	 /* postfix opcode */
} ELEMENT;

/*
 * NOTE: Keep these lists sorted. Elements are searched in reverse order,
 *  and where two names start with the same substring we must pick out the
 *  longest name first (hence the sort requirement).
 * NOTE: All VARARG_OPERATORs have to be made known to the calcExprDump()
 *  routine at the end of this file.
 */
static const ELEMENT operands[] = {
/* name 	prio's	stack	element type	opcode */
{"!",		7, 8,	0,	UNARY_OPERATOR, REL_NOT},
{"(",		0, 8,	0,	UNARY_OPERATOR,	NOT_GENERATED},
{"-",		7, 8,	0,	UNARY_OPERATOR,	UNARY_NEG},
{".",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"0",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"0X",		0, 0,	1,	LITERAL_OPERAND,LITERAL_INT},
{"1",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"2",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"3",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"4",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"5",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"6",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"7",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"8",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"9",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"A",		0, 0,	1,	OPERAND,	FETCH_A},
{"ABS",		7, 8,	0,	UNARY_OPERATOR,	ABS_VAL},
{"ACOS",	7, 8,	0,	UNARY_OPERATOR,	ACOS},
{"ASIN",	7, 8,	0,	UNARY_OPERATOR,	ASIN},
{"ATAN",	7, 8,	0,	UNARY_OPERATOR,	ATAN},
{"ATAN2",	7, 8,	-1,	UNARY_OPERATOR,	ATAN2},
{"B",		0, 0,	1,	OPERAND,	FETCH_B},
{"C",		0, 0,	1,	OPERAND,	FETCH_C},
{"CEIL",	7, 8,	0,	UNARY_OPERATOR,	CEIL},
{"COS",		7, 8,	0,	UNARY_OPERATOR,	COS},
{"COSH",	7, 8,	0,	UNARY_OPERATOR,	COSH},
{"D",		0, 0,	1,	OPERAND,	FETCH_D},
{"D2R",		0, 0,	1,	OPERAND,	CONST_D2R},
{"E",		0, 0,	1,	OPERAND,	FETCH_E},
{"EXP",		7, 8,	0,	UNARY_OPERATOR,	EXP},
{"F",		0, 0,	1,	OPERAND,	FETCH_F},
{"FINITE",	7, 8,	0,	VARARG_OPERATOR,FINITE},
{"FLOOR",	7, 8,	0,	UNARY_OPERATOR,	FLOOR},
{"G",		0, 0,	1,	OPERAND,	FETCH_G},
{"H",		0, 0,	1,	OPERAND,	FETCH_H},
{"I",		0, 0,	1,	OPERAND,	FETCH_I},
{"INF",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"ISINF",	7, 8,	0,	UNARY_OPERATOR,	ISINF},
{"ISNAN",	7, 8,	0,	VARARG_OPERATOR,ISNAN},
{"J",		0, 0,	1,	OPERAND,	FETCH_J},
{"K",		0, 0,	1,	OPERAND,	FETCH_K},
{"L",		0, 0,	1,	OPERAND,	FETCH_L},
{"LN",		7, 8,	0,	UNARY_OPERATOR,	LOG_E},
{"LOG",		7, 8,	0,	UNARY_OPERATOR,	LOG_10},
{"LOGE",	7, 8,	0,	UNARY_OPERATOR,	LOG_E},
{"MAX",		7, 8,	0,	VARARG_OPERATOR,MAX},
{"MIN",		7, 8,	0,	VARARG_OPERATOR,MIN},
{"NINT",	7, 8,	0,	UNARY_OPERATOR,	NINT},
{"NAN",		0, 0,	1,	LITERAL_OPERAND,LITERAL_DOUBLE},
{"NOT",		7, 8,	0,	UNARY_OPERATOR,	BIT_NOT},
{"PI",		0, 0,	1,	OPERAND,	CONST_PI},
{"R2D",		0, 0,	1,	OPERAND,	CONST_R2D},
{"RNDM",	0, 0,	1,	OPERAND,	RANDOM},
{"SIN",		7, 8,	0,	UNARY_OPERATOR,	SIN},
{"SINH",	7, 8,	0,	UNARY_OPERATOR,	SINH},
{"SQR",		7, 8,	0,	UNARY_OPERATOR,	SQU_RT},
{"SQRT",	7, 8,	0,	UNARY_OPERATOR,	SQU_RT},
{"TAN",		7, 8,	0,	UNARY_OPERATOR,	TAN},
{"TANH",	7, 8,	0,	UNARY_OPERATOR,	TANH},
{"VAL",		0, 0,	1,	OPERAND,	FETCH_VAL},
{"~",		7, 8,	0,	UNARY_OPERATOR, BIT_NOT},
};

static const ELEMENT operators[] = {
/* name 	prio's	stack	element type	opcode */
{"!=",		3, 3,	-1,	BINARY_OPERATOR,NOT_EQ},
{"#",		3, 3,	-1,	BINARY_OPERATOR,NOT_EQ},
{"%",		5, 5,	-1,	BINARY_OPERATOR,MODULO},
{"&",		2, 2,	-1,	BINARY_OPERATOR,BIT_AND},
{"&&",		2, 2,	-1,	BINARY_OPERATOR,REL_AND},
{")",		0, 0,	0,	CLOSE_PAREN,	NOT_GENERATED},
{"*",		5, 5,	-1,	BINARY_OPERATOR,MULT},
{"**",		6, 6,	-1,	BINARY_OPERATOR,POWER},
{"+",		4, 4,	-1,	BINARY_OPERATOR,ADD},
{",",		0, 0,	0,	SEPERATOR,	NOT_GENERATED},
{"-",		4, 4,	-1,	BINARY_OPERATOR,SUB},
{"/",		5, 5,	-1,	BINARY_OPERATOR,DIV},
{":",		0, 0,	-1,	CONDITIONAL,	COND_ELSE},
{":=",		0, 0,	-1,	STORE_OPERATOR, STORE_A},
{";",		0, 0,	0,	EXPR_TERMINATOR,NOT_GENERATED},
{"<",		3, 3,	-1,	BINARY_OPERATOR,LESS_THAN},
{"<<",		2, 2,	-1,	BINARY_OPERATOR,LEFT_SHIFT},
{"<=",		3, 3,	-1,	BINARY_OPERATOR,LESS_OR_EQ},
{"=",		3, 3,	-1,	BINARY_OPERATOR,EQUAL},
{"==",		3, 3,	-1,	BINARY_OPERATOR,EQUAL},
{">",		3, 3,	-1,	BINARY_OPERATOR,GR_THAN},
{">=",		3, 3,	-1,	BINARY_OPERATOR,GR_OR_EQ},
{">>",		2, 2,	-1,	BINARY_OPERATOR,RIGHT_SHIFT},
{"?",		0, 0,	-1,	CONDITIONAL,	COND_IF},
{"AND",		2, 2,	-1,	BINARY_OPERATOR,BIT_AND},
{"OR",		1, 1,	-1,	BINARY_OPERATOR,BIT_OR},
{"XOR",		1, 1,	-1,	BINARY_OPERATOR,BIT_EXCL_OR},
{"^",		6, 6,	-1,	BINARY_OPERATOR,POWER},
{"|",		1, 1,	-1,	BINARY_OPERATOR,BIT_OR},
{"||",		1, 1,	-1,	BINARY_OPERATOR,REL_OR},
};


/* get_element
 *
 * find the next expression element in the infix expression
 */
static int
    get_element(int opnd, const char **ppsrc, const ELEMENT **ppel)
{
    const ELEMENT *ptable, *pel;

    *ppel = NULL;

    while (isspace((int) (unsigned char) **ppsrc)) ++*ppsrc;
    if (**ppsrc == '\0') return FALSE;

    if (opnd) {
	ptable = operands;
	pel = ptable + NELEMENTS(operands) - 1;
    } else {
	ptable = operators;
	pel = ptable + NELEMENTS(operators) - 1;
    }

    while (pel >= ptable) {
	size_t len = strlen(pel->name);

	if (epicsStrnCaseCmp(*ppsrc, pel->name, len) == 0) {
	    *ppel = pel;
	    *ppsrc += len;
	    return TRUE;
	}
	--pel;
    }
    return FALSE;
}


/* postfix
 *
 * convert an infix expression to a postfix expression
 */
epicsShareFunc long
    postfix(const char *psrc, char *pout, short *perror)
{
    ELEMENT stack[80];
    ELEMENT *pstacktop = stack;
    const ELEMENT *pel;
    int operand_needed = TRUE;
    int runtime_depth = 0;
    int cond_count = 0;
    char * const pdest = pout;
    char *pnext;

    if (psrc == NULL || *psrc == '\0' ||
	pout == NULL || perror == NULL) {
	if (perror) *perror = CALC_ERR_NULL_ARG;
	if (pout) *pout = END_EXPRESSION;
	return -1;
    }

    /* place the expression elements into postfix */
    *pout = END_EXPRESSION;
    *perror = CALC_ERR_NONE;

    while (get_element(operand_needed, &psrc, &pel)) {
	switch (pel->type) {

	case OPERAND:
	    *pout++ = pel->code;
	    runtime_depth += pel->runtime_effect;
	    operand_needed = FALSE;
	    break;

        case LITERAL_OPERAND:
            runtime_depth += pel->runtime_effect;

            psrc -= strlen(pel->name);
            if (pel->code == LITERAL_DOUBLE) {
                double lit_d;
                epicsInt32 lit_i;

                if (epicsParseDouble(psrc, &lit_d, &pnext)) {
                    *perror = CALC_ERR_BAD_LITERAL;
                    goto bad;
                }
                psrc = pnext;
                lit_i = (epicsInt32) lit_d;
                if (lit_d != (double) lit_i) {
                    *pout++ = pel->code;
                    memcpy(pout, &lit_d, sizeof(double));
                    pout += sizeof(double);
                } else {
                    *pout++ = LITERAL_INT;
                    memcpy(pout, &lit_i, sizeof(epicsInt32));
                    pout += sizeof(epicsInt32);
                }
            }
            else {
                epicsUInt32 lit_ui;

                assert(pel->code == LITERAL_INT);
                if (epicsParseUInt32(psrc, &lit_ui, 0, &pnext)) {
                    *perror = CALC_ERR_BAD_LITERAL;
                    goto bad;
                }
                psrc = pnext;
                *pout++ = LITERAL_INT;
                memcpy(pout, &lit_ui, sizeof(epicsUInt32));
                pout += sizeof(epicsUInt32);
            }

            operand_needed = FALSE;
            break;

	case STORE_OPERATOR:
	    if (pout == pdest || pstacktop > stack ||
		*--pout < FETCH_A || *pout > FETCH_L) {
		*perror = CALC_ERR_BAD_ASSIGNMENT;
		goto bad;
	    }
	    /* Convert fetch into a store on the stack */
	    *++pstacktop = *pel;
	    pstacktop->code = STORE_A + *pout - FETCH_A;
	    runtime_depth -= 1;
	    operand_needed = TRUE;
	    break;

	case UNARY_OPERATOR:
	case VARARG_OPERATOR:
	    /* Move operators of >= priority to the output */
	    while ((pstacktop > stack) &&
		   (pstacktop->in_stack_pri >= pel->in_coming_pri)) {
		*pout++ = pstacktop->code;
		if (pstacktop->type == VARARG_OPERATOR) {
		    *pout++ = 1 - pstacktop->runtime_effect;
		}
		runtime_depth += pstacktop->runtime_effect;
		pstacktop--;
	    }

	    /* Push new operator onto stack */
	    pstacktop++;
	    *pstacktop = *pel;
	    break;

	case BINARY_OPERATOR:
	    /* Move operators of >= priority to the output */
	    while ((pstacktop > stack) &&
		   (pstacktop->in_stack_pri >= pel->in_coming_pri)) {
		*pout++ = pstacktop->code;
		if (pstacktop->type == VARARG_OPERATOR) {
		    *pout++ = 1 - pstacktop->runtime_effect;
		}
		runtime_depth += pstacktop->runtime_effect;
		pstacktop--;
	    }

	    /* Push new operator onto stack */
	    pstacktop++;
	    *pstacktop = *pel;

	    operand_needed = TRUE;
	    break;

	case SEPERATOR:
	    /* Move operators to the output until open paren */
	    while (pstacktop->name[0] != '(') {
		if (pstacktop <= stack+1) {
		    *perror = CALC_ERR_BAD_SEPERATOR;
		    goto bad;
		}
		*pout++ = pstacktop->code;
		if (pstacktop->type == VARARG_OPERATOR) {
		    *pout++ = 1 - pstacktop->runtime_effect;
		}
		runtime_depth += pstacktop->runtime_effect;
		pstacktop--;
	    }
	    operand_needed = TRUE;
	    pstacktop->runtime_effect -= 1;
	    break;

	case CLOSE_PAREN:
	    /* Move operators to the output until matching paren */
	    while (pstacktop->name[0] != '(') {
		if (pstacktop <= stack+1) {
		    *perror = CALC_ERR_PAREN_NOT_OPEN;
		    goto bad;
		}
		*pout++ = pstacktop->code;
		if (pstacktop->type == VARARG_OPERATOR) {
		    *pout++ = 1 - pstacktop->runtime_effect;
		}
		runtime_depth += pstacktop->runtime_effect;
		pstacktop--;
	    }
	    pstacktop--;	/* remove ( from stack */
	    /* if there is a vararg operator before the opening paren,
	       it inherits the (opening) paren's stack effect */
	    if ((pstacktop > stack) &&
		pstacktop->type == VARARG_OPERATOR) {
		pstacktop->runtime_effect = (pstacktop+1)->runtime_effect;
		/* check for no arguments */
		if (pstacktop->runtime_effect > 0) {
		    *perror = CALC_ERR_INCOMPLETE;
		    goto bad;
		}
	    }
	    break;

	case CONDITIONAL:
	    /* Move operators of > priority to the output */
	    while ((pstacktop > stack) &&
		   (pstacktop->in_stack_pri > pel->in_coming_pri)) {
		*pout++ = pstacktop->code;
		if (pstacktop->type == VARARG_OPERATOR) {
		    *pout++ = 1 - pstacktop->runtime_effect;
		}
		runtime_depth += pstacktop->runtime_effect;
		pstacktop--;
	    }

	    /* Add new element to the output */
	    *pout++ = pel->code;
	    runtime_depth += pel->runtime_effect;

	    /* For : operator, also push COND_END code to stack */
	    if (pel->name[0] == ':') {
		if (--cond_count < 0) {
		    *perror = CALC_ERR_CONDITIONAL;
		    goto bad;
		}
		pstacktop++;
		*pstacktop = *pel;
		pstacktop->code = COND_END;
		pstacktop->runtime_effect = 0;
	    } else {
		cond_count++;
	    }

	    operand_needed = TRUE;
	    break;

	case EXPR_TERMINATOR:
	    /* Move everything from stack to the output */
	    while (pstacktop > stack) {
		if (pstacktop->name[0] == '(') {
		    *perror = CALC_ERR_PAREN_OPEN;
		    goto bad;
		}
		*pout++ = pstacktop->code;
		if (pstacktop->type == VARARG_OPERATOR) {
		    *pout++ = 1 - pstacktop->runtime_effect;
		}
		runtime_depth += pstacktop->runtime_effect;
		pstacktop--;
	    }

	    if (cond_count != 0) {
		*perror = CALC_ERR_CONDITIONAL;
		goto bad;
	    }
	    if (runtime_depth > 1) {
		*perror = CALC_ERR_TOOMANY;
		goto bad;
	    }

	    operand_needed = TRUE;
	    break;

	default:
	    *perror = CALC_ERR_INTERNAL;
	    goto bad;
	}

	if (runtime_depth < 0) {
	    *perror = CALC_ERR_UNDERFLOW;
	    goto bad;
	}
	if (runtime_depth >= CALCPERFORM_STACK) {
	    *perror = CALC_ERR_OVERFLOW;
	    goto bad;
	}
    }

    if (*psrc != '\0') {
	*perror = CALC_ERR_SYNTAX;
	goto bad;
    }

    /* Move everything from stack to the output */
    while (pstacktop > stack) {
	if (pstacktop->name[0] == '(') {
	    *perror = CALC_ERR_PAREN_OPEN;
	    goto bad;
	}
	*pout++ = pstacktop->code;
	if (pstacktop->type == VARARG_OPERATOR) {
	    *pout++ = 1 - pstacktop->runtime_effect;
	}
	runtime_depth += pstacktop->runtime_effect;
	pstacktop--;
    }
    *pout = END_EXPRESSION;

    if (cond_count != 0) {
	*perror = CALC_ERR_CONDITIONAL;
	goto bad;
    }
    if (operand_needed || runtime_depth != 1) {
	*perror = CALC_ERR_INCOMPLETE;
	goto bad;
    }
    return 0;

bad:
    *pdest = END_EXPRESSION;
    return -1;
}


/* calcErrorStr
 *
 * Return a message string appropriate for the given error code
 */
epicsShareFunc const char *
    calcErrorStr(short error)
{
    static const char *errStrs[] = {
	"No error",
	"Too many results returned",
	"Badly formed numeric literal",
	"Bad assignment target",
	"Comma without enclosing parentheses",
	"Close parenthesis found without open",
	"Parenthesis still open at end of expression",
	"Unbalanced conditional ?: operators",
	"Incomplete expression, operand missing",
	"Not enough operands provided",
	"Runtime stack overflow",
	"Syntax error, unknown operator/operand",
	"NULL or empty input argument to postfix()",
	"Internal error, unknown element type",
    };
    
    if (error < CALC_ERR_NONE || error > CALC_ERR_INTERNAL)
	return NULL;
    return errStrs[error];
}


/* calcExprDump
 *
 * Disassemble the given postfix instructions to stdout
 */
epicsShareFunc void
    calcExprDump(const char *pinst)
{
    static const char *opcodes[] = {
	"End Expression",
    /* Operands */
	"LITERAL_DOUBLE", "LITERAL_INT", "VAL",
	"FETCH_A", "FETCH_B", "FETCH_C", "FETCH_D", "FETCH_E", "FETCH_F",
	"FETCH_G", "FETCH_H", "FETCH_I", "FETCH_J", "FETCH_K", "FETCH_L",
    /* Assignment */
	"STORE_A", "STORE_B", "STORE_C", "STORE_D", "STORE_E", "STORE_F",
	"STORE_G", "STORE_H", "STORE_I", "STORE_J", "STORE_K", "STORE_L",
    /* Trigonometry Constants */
	"CONST_PI",
	"CONST_D2R",
	"CONST_R2D",
    /* Arithmetic */
	"UNARY_NEG",
	"ADD",
	"SUB",
	"MULT",
	"DIV",
	"MODULO",
	"POWER",
    /* Algebraic */
	"ABS_VAL",
	"EXP",
	"LOG_10",
	"LOG_E",
	"MAX",
	"MIN",
	"SQU_RT",
    /* Trigonometric */
	"ACOS",
	"ASIN",
	"ATAN",
	"ATAN2",
	"COS",
	"COSH",
	"SIN",
	"SINH",
	"TAN",
	"TANH",
    /* Numeric */
	"CEIL",
	"FLOOR",
	"FINITE",
	"ISINF",
	"ISNAN",
	"NINT",
	"RANDOM",
    /* Boolean */
	"REL_OR",
	"REL_AND",
	"REL_NOT",
    /* Bitwise */
	"BIT_OR",
	"BIT_AND",
	"BIT_EXCL_OR",
	"BIT_NOT",
	"RIGHT_SHIFT",
	"LEFT_SHIFT",
    /* Relationals */
	"NOT_EQ",
	"LESS_THAN",
	"LESS_OR_EQ",
	"EQUAL",
	"GR_OR_EQ",
	"GR_THAN",
    /* Conditional */
	"COND_IF",
	"COND_ELSE",
	"COND_END",
    /* Misc */
	"NOT_GENERATED"
    };
    char op;
    double lit_d;
    epicsInt32 lit_i;
    
    while ((op = *pinst) != END_EXPRESSION) {
	switch (op) {
	case LITERAL_DOUBLE:
	    memcpy(&lit_d, ++pinst, sizeof(double));
	    printf("\tDouble %g\n", lit_d);
	    pinst += sizeof(double);
	    break;
	case LITERAL_INT:
	    memcpy(&lit_i, ++pinst, sizeof(epicsInt32));
	    printf("\tInteger %d (0x%x)\n", lit_i, lit_i);
	    pinst += sizeof(epicsInt32);
	    break;
	case MIN:
	case MAX:
	case FINITE:
	case ISNAN:
	    printf("\t%s, %d arg(s)\n", opcodes[(int) op], *++pinst);
	    pinst++;
	    break;
	default:
	    printf("\t%s\n", opcodes[(int) op]);
	    pinst++;
	}
    }
}
