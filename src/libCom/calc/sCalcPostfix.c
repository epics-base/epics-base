/* $Id$
 * Subroutines used to convert an infix expression to a postfix expression
 *
 *      Author:          Bob Dalesio
 *      Date:            12-12-86
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
 * .01  01-11-89        lrd     added right shift and left shift operations
 * .02  01-13-89        lrd     modified to load into IOCs
 * .03  02-01-89        lrd     added trigonometric functions
 * .04  04-05-89        lrd     fixed the order of some operations in the
 *                              element table and added a warning label
 * .05  11-26-90        lrd     fix SINH, COSH, TANH
 * .06	02-20-92	rcz	fixed for vxWorks build
 * .07  02-24-92        jba     add EXP and fixed trailing blanks in expression
 * .08  03-03-92        jba     added MAX_VAL and MIN_VAL and comma(like close paren)
 * .09  03-06-92        jba     added multiple conditional expressions ?
 * .10  04-01-92        jba     allowed floating pt constants in expression
 * .11  05-01-92        jba     flt pt constant string replaced with double in postfix
 * .12  08-21-92        jba     ANSI c changes
 * .13  08-21-92        jba     initialized *ppostfix: needed when calc expr not defined
 * .14  12-11-92	mrk	Removed include for stdioLib.h
 * .15  11-03-93		jba		Added test for extra close paren at end of expression
 * .16  01-24-94		jba		Changed seperator test to catch invalid commas
 * .17  05-11-94		jba		Added support for CONST_PI, CONST_R2D, and CONST_D2R
 * 								and conversion of infix expression to uppercase
 * .18  01-22-98        tmm     Changed name postfix() to calcPostfix().  Changed arg list
 *                              from pointer to postfix expression to address of pointer to
 *                              postfix expression.  calcPostfix() frees old expression, allocates
 *                              space sufficient to hold new expression, and returns pointer to it.
 *                              Added S2R, R2S (conversions between arc-seconds and radians).  Changed
 *                              CONSTANT to LITERAL to avoid conflict with link.h.  Support 26 vars (A-Z).
 * .19  03-18-98        tmm     Added string operators
 */

/* 
 * Subroutines
 *
 *	Public
 *
 * sCalcPostfix		convert an algebraic expression to symbolic postfix
 *	args
 *		pinfix		the algebraic expression
 *		pp_postfix	address of the symbolic postfix expression
 *      perror      error information
 *	returns
 *		0		successful
 *		-1		not successful
 * Private routines for calcPostfix
 *
 * find_element		finds a symbolic element in the expression element tbl
 *	args
 *		pbuffer		pointer to the infix expression element
 *		pelement	pointer to the expression element table entry
 *		pno_bytes	pointer to the size of this element
 *		parg		pointer to arg (used for fetch)
 *	returns
 *		TRUE		element found
 *		FALSE		element not found
 *
 * get_element		finds the next expression element in the infix expr
 *	args
 *		pinfix		pointer into the infix expression
 *		pelement	pointer to the expression element table
 *		pno_bytes	size of the element in the infix expression
 *		parg		pointer to argument (used for fetch)
 *	returns
 *		FINE		found an expression element
 *		VARIABLE	found a database reference
 *		UNKNOWN_ELEMENT	unknown element found in the infix expression
 */

#ifdef vxWorks
#include  <vxWorks.h>
#endif

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>

#include	"dbDefs.h"
#define epicsExportSharedSymbols
#include	"sCalcPostfix.h"
#include	"sCalcPostfixPvt.h"


#define DEBUG 0
volatile int sCalcPostfixDebug=0;

/* declarations for postfix */
/* element types */
#define	OPERAND			0
#define UNARY_OPERATOR	1
#define	BINARY_OPERATOR	2
#define	EXPR_TERM		3
#define	COND			4
#define	CLOSE_PAREN		5
#define	CONDITIONAL		6
#define	ELSE			7
#define	SEPARATOR		8
#define	TRASH			9
#define	FLOAT_PT_CONST	10
#define	MINUS_OPERATOR	11
#define	STRING_CONST	12
#define	CLOSE_BRACKET	13
#define	CLOSE_CURLY		14

#define UNARY_MINUS_I_S_P  7
#define UNARY_MINUS_I_C_P  8
#define UNARY_MINUS_CODE   UNARY_NEG
#define BINARY_MINUS_I_S_P 4
#define BINARY_MINUS_I_C_P 4
#define BINARY_MINUS_CODE  SUB

/* parsing return values */
#define	FINE		0
#define	UNKNOWN_ELEMENT	-1
#define	END		-2

/*
 * element table
 *
 * structure of an element
 */
struct	expression_element{
	char	element[10];	/* character representation of an element */
	char	in_stack_pri;	/* priority in translation stack */
	char	in_coming_pri;	/* priority when first checking */
	char	type;		/* element type */
	char	code;		/* postfix representation */
};

/*
 * NOTE: DO NOT CHANGE WITHOUT READING THIS NOTICE !!!!!!!!!!!!!!!!!!!!
 * Because the routine that looks for a match in this table takes the first 
 * match it finds, elements whose designations are contained in other elements
 * MUST come first in this list. (e.g. ABS will match A if A preceeds ABS and
 * then try to find BS therefore ABS must be first in this list
 */
static struct expression_element	elements[] = {
/*
element	i_s_p	i_c_p	type_element	internal_rep */
"ABS",		7,	8,	UNARY_OPERATOR,	ABS_VAL,   /* absolute value */
"NOT",		7,	8,	UNARY_OPERATOR,	UNARY_NEG, /* unary negate */
"-",		7,	8,	MINUS_OPERATOR,	UNARY_NEG, /* unary negate (or binary op) */
"SQRT",		7,	8,	UNARY_OPERATOR,	SQU_RT,    /* square root */
"SQR",		7,	8,	UNARY_OPERATOR,	SQU_RT,    /* square root */
"EXP",		7,	8,	UNARY_OPERATOR,	EXP,       /* exponential function */
"LOGE",		7,	8,	UNARY_OPERATOR,	LOG_E,     /* log E */
"LN",		7,	8,	UNARY_OPERATOR,	LOG_E,     /* log E */
"LOG",		7,	8,	UNARY_OPERATOR,	LOG_10,    /* log 10 */
"ACOS",		7,	8,	UNARY_OPERATOR,	ACOS,      /* arc cosine */
"ASIN",		7,	8,	UNARY_OPERATOR,	ASIN,      /* arc sine */
"ATAN2",	7,	8,	UNARY_OPERATOR,	ATAN2,     /* arc tangent */
"ATAN",		7,	8,	UNARY_OPERATOR,	ATAN,      /* arc tangent */
"MAX",		7,	8,	UNARY_OPERATOR,	MAX_VAL,   /* maximum of 2 args */
"MIN",		7,	8,	UNARY_OPERATOR,	MIN_VAL,   /* minimum of 2 args */
"CEIL",		7,	8,	UNARY_OPERATOR,	CEIL,      /* smallest integer >= */
"FLOOR",	7,	8,	UNARY_OPERATOR,	FLOOR,     /* largest integer <=  */
"NINT",		7,	8,	UNARY_OPERATOR,	NINT,      /* nearest integer */
"INT",		7,	8,	UNARY_OPERATOR,	NINT,      /* nearest integer */
"COSH",		7,	8,	UNARY_OPERATOR,	COSH,      /* hyperbolic cosine */
"COS",		7,	8,	UNARY_OPERATOR,	COS,       /* cosine */
"SINH",		7,	8,	UNARY_OPERATOR,	SINH,      /* hyperbolic sine */
"SIN",		7,	8,	UNARY_OPERATOR,	SIN,       /* sine */
"TANH",		7,	8,	UNARY_OPERATOR,	TANH,      /* hyperbolic tangent*/
"TAN",		7,	8,	UNARY_OPERATOR,	TAN,       /* tangent */
"!=",		3,	3,	BINARY_OPERATOR,NOT_EQ,    /* not equal */
"!",		7,	8,	UNARY_OPERATOR, REL_NOT,   /* not */
"~",		7,	8,	UNARY_OPERATOR, BIT_NOT,   /* bitwise not */
"DBL",		7,	8,	UNARY_OPERATOR, TO_DOUBLE, /* convert to double */
"STR",		7,	8,	UNARY_OPERATOR, TO_STRING, /* convert to string */
"$P",		7,	8,	UNARY_OPERATOR,	PRINTF,    /* formatted print to string */
"PRINTF",	7,	8,	UNARY_OPERATOR,	PRINTF,    /* formatted print to string */
"$S",		7,	8,	UNARY_OPERATOR,	SSCANF,    /* scan string argument */
"SSCANF",	7,	8,	UNARY_OPERATOR,	SSCANF,    /* scan string argument */
"RNDM",    	0,	0,	OPERAND,	RANDOM,        /* Random Number */
"OR",		1,	1,	BINARY_OPERATOR,BIT_OR,    /* or */
"AND",		2,	2,	BINARY_OPERATOR,BIT_AND,   /* and */
"XOR",		1,	1,	BINARY_OPERATOR,BIT_EXCL_OR, /* exclusive or */
"PI",		0,	0,	OPERAND,	CONST_PI,      /* pi */
"D2R",		0,	0,	OPERAND,	CONST_D2R,     /* pi/180 */
"R2D",		0,	0,	OPERAND,	CONST_R2D,     /* 180/pi */
"S2R",		0,	0,	OPERAND,	CONST_S2R,     /* arc-sec to radians: pi/(180*3600) */
"R2S",		0,	0,	OPERAND,	CONST_R2S,     /* radians to arc-sec: (180*3600)/pi */
"0",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"1",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"2",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"3",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"4",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"5",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"6",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"7",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"8",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"9",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
".",		0,	0,	FLOAT_PT_CONST,	LITERAL,   /* flt pt constant */
"\"",		0,	0,	STRING_CONST,	SLITERAL,  /* string constant */
"'",		0,	0,	STRING_CONST,	SLITERAL,  /* string constant */
"?",		0,	0,	CONDITIONAL,	COND_IF,   /* conditional */
":",		0,	0,	CONDITIONAL,	COND_ELSE, /* else */
"(",		0,	8,	UNARY_OPERATOR,	PAREN,     /* open paren */
"[",		0,	8,	BINARY_OPERATOR,SUBRANGE,  /* string subrange */
"{",		0,	8,	BINARY_OPERATOR,REPLACE,   /* string replace */
"^",		6,	6,	BINARY_OPERATOR,EXPON,     /* exponentiation */
"**",		6,	6,	BINARY_OPERATOR,EXPON,     /* exponentiation */
"+",		4,	4,	BINARY_OPERATOR,ADD,       /* addition */
#if 0 /* "-" operator is overloaded; may be unary or binary */
"-",		4,	4,	BINARY_OPERATOR,SUB,       /* subtraction */
#endif
"*",		5,	5,	BINARY_OPERATOR,MULT,      /* multiplication */
"/",		5,	5,	BINARY_OPERATOR,DIV,       /* division */
"%",		5,	5,	BINARY_OPERATOR,MODULO,    /* modulo */
",",		0,	0,	SEPARATOR,	COMMA,         /* comma */
")",		0,	0,	CLOSE_PAREN,	PAREN,     /* close paren */
"]",		0,	0,	CLOSE_BRACKET,	SUBRANGE,  /* close bracket */
"}",		0,	0,	CLOSE_CURLY,	REPLACE,   /* close curly bracket */
"||",		1,	1,	BINARY_OPERATOR,REL_OR,    /* logical or */
"|",		1,	1,	BINARY_OPERATOR,BIT_OR,    /* bitwise or */
"&&",		2,	2,	BINARY_OPERATOR,REL_AND,   /* logical and */
"&",		2,	2,	BINARY_OPERATOR,BIT_AND,   /* bitwise and */
">>",		2,	2,	BINARY_OPERATOR,RIGHT_SHIFT, /* right shift */
">=",		3,	3,	BINARY_OPERATOR,GR_OR_EQ,  /* greater or equal*/
">",		3,	3,	BINARY_OPERATOR,GR_THAN,   /* greater than */
"<<",		2,	2,	BINARY_OPERATOR,LEFT_SHIFT, /* left shift */
"<=",		3,	3,	BINARY_OPERATOR,LESS_OR_EQ,/* less or equal to*/
"<",		3,	3,	BINARY_OPERATOR,LESS_THAN, /* less than */
"#",		3,	3,	BINARY_OPERATOR,NOT_EQ,    /* not equal */
"==",		3,	3,	BINARY_OPERATOR,EQUAL,     /* equal */
"=",		3,	3,	BINARY_OPERATOR,EQUAL,     /* equal */
""
};

/*
 * Element-table entry for "fetch" operation.  This element is used for all
 * named variables.  Currently, letters A-Z (double) and AA-ZZ (string) are
 * allowed.  Lower and upper case letters mean the same thing.
 */
static struct expression_element	fetch_element = {
"A",		0,	0,	OPERAND,	FETCH,   /* fetch var */
};

static struct expression_element	fetch_string_element = {
"AA",		0,	0,	OPERAND,	SFETCH,   /* fetch var */
};

static int strncasecmp(char *s1, char *s2, size_t n)
{
	short i;
	for (i=0; i<n && (*s1 || *s2); i++, s1++, s2++) {
		if (toupper((int)*s1) > toupper((int)*s2)) return(1);
		if (toupper((int)*s1) < toupper((int)*s2)) return(-1);
	}
	return(0);
}
/*
 * FIND_ELEMENT
 *
 * find the pointer to an entry in the element table
 */
static int find_element(pbuffer, pelement, pno_bytes, parg)
 register char	*pbuffer;
 register struct expression_element	**pelement;
 register short	*pno_bytes, *parg;
 {
	*parg = 0;

 	/* compare the string to each element in the element table */
 	*pelement = &elements[0];
 	while ((*pelement)->element[0] != NULL){
 		if (strncasecmp(pbuffer,(*pelement)->element, strlen((*pelement)->element)) == 0){
 			*pno_bytes += strlen((*pelement)->element);
 			return(TRUE);
 		}
 		*pelement += 1;
 	}

	/* look for a variable reference */
	/* double variables: ["a" - "z"], numbered 1-26 */
	if (isalpha((int)*pbuffer)) {
		*pelement = &fetch_element; /* fetch means "variable reference" (fetch or store) */
		*parg = *pbuffer - (isupper((int)*pbuffer) ? 'A' : 'a');
		*pno_bytes += 1;
		/* string variables: ["aa" - "zz"], numbered 1-26 */
		if (pbuffer[1] == pbuffer[0]) {
			*pelement = &fetch_string_element;
			*pno_bytes += 1;
		}
 		return(TRUE);
	}
	if (sCalcPostfixDebug) printf("find_element: can't find '%s'\n", pbuffer);
 	return(FALSE);
 }
 
/*
 * GET_ELEMENT
 *
 * get an expression element
 */
static int get_element(pinfix, pelement, pno_bytes, parg)
register char	*pinfix;
register struct expression_element	**pelement;
register short	*pno_bytes, *parg;
{

	/* get the next expression element from the infix expression */
	if (*pinfix == NULL) return(END);
	*pno_bytes = 0;
	while (*pinfix == 0x20){
		*pno_bytes += 1;
		pinfix++;
	}
	if (*pinfix == NULL) return(END);
	if (!find_element(pinfix, pelement, pno_bytes, parg))
		return(UNKNOWN_ELEMENT);
	if (sCalcPostfixDebug > 5) printf("get_element: found element '%s', arg=%d\n", (*pelement)->element, *parg);

	return(FINE);

	
}

#if DEBUG
/* Override standard EPICS expression evaluator (if we're loaded after it). */
long epicsShareAPI postfix(char *pinfix,char *ppostfix,short *perror)
{
	char *my_ppostfix = NULL, *s, *d;
	long retval;

	retval = sCalcPostfix(pinfix, &my_ppostfix, perror);
	if (*my_ppostfix == BAD_EXPRESSION) {
		*ppostfix = BAD_EXPRESSION;
	} else {
		for (s = my_ppostfix, d = ppostfix; *s != END_STACK; ) {
			*d++=*s++;
		}
		*d = *s;
	}
	free(my_ppostfix);
	return(retval);
}
#endif

/*
 * sCalcPostFix
 *
 * convert an infix expression to a postfix expression
 */
long epicsShareAPI sCalcPostfix(char *pinfix, char **pp_postfix, short *perror)
{
	short no_bytes;
	register short operand_needed;
	register short new_expression;
	struct expression_element stack[80];
	struct expression_element *pelement;
	register struct expression_element *pstacktop;
	double constant;
	register char c, *pposthold;	
	char in_stack_pri, in_coming_pri, code;
	char *ppostfix, *ppostfixStart;
	short arg;

	if (sCalcPostfixDebug) printf("sCalcPostfix: entry\n");

	/* Allocate a buffer for the postfix expression. */
	if (*pp_postfix) free(*pp_postfix);	/* Free old buffer. */
	ppostfix = calloc(5*strlen(pinfix)+7, 1);
	*pp_postfix = ppostfix;
	ppostfixStart = ppostfix++;
	*ppostfixStart = BAD_EXPRESSION;
	*ppostfix = END_STACK;

	/* place the expression elements into postfix */
	operand_needed = TRUE;
	new_expression = TRUE;
	*perror = 0;
	if (*pinfix == 0) {
		return(0);
	}
	pstacktop = &stack[0];
	while (get_element(pinfix, &pelement, &no_bytes, &arg) != END){
		pinfix += no_bytes;
		code = pelement->code;
		if ((*ppostfixStart != USES_STRING) && ((code == TO_STRING) ||
			(code == PRINTF) || (code == SSCANF) || (code == SLITERAL) ||
			(code == SUBRANGE) || (code == SFETCH))) {
				*ppostfixStart = USES_STRING;
		}

		switch (pelement->type){

	    case OPERAND:
			if (!operand_needed){
				*perror = 5;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operand to the expression */
			*ppostfix++ = pelement->code;

			/* if this is a variable reference, append variable number */
			if ((pelement->code == FETCH) || (pelement->code == SFETCH)) {
				*ppostfix++ = arg;
			}

			operand_needed = FALSE;
			new_expression = FALSE;
			break;

		case FLOAT_PT_CONST:
			if (!operand_needed){
				*perror = 5;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add constant to postfix expression */
			*ppostfix++ = pelement->code;
			pposthold = ppostfix;

			pinfix-=no_bytes;
			while (*pinfix == ' ') *ppostfix++ = *pinfix++;
			while (TRUE) {
				if ( ( *pinfix >= '0' && *pinfix <= '9' ) || *pinfix == '.' ) {
					*ppostfix++ = *pinfix;
					pinfix++;
				} else if ( *pinfix == 'E' || *pinfix == 'e' ) {
					*ppostfix++ = *pinfix;
					pinfix++;
						if (*pinfix == '+' || *pinfix == '-' ) {
							*ppostfix++ = *pinfix;
							pinfix++;
						}
				} else break;
			}
			*ppostfix++ = '\0';

			ppostfix = pposthold;
			if ( sscanf(ppostfix,"%lg",&constant) != 1) {
				*ppostfix = '\0';
			} else {
				memcpy(ppostfix,(void *)&constant,8);
			}
			ppostfix+=8;

			operand_needed = FALSE;
			new_expression = FALSE;
			break;

		case STRING_CONST:
			if (!operand_needed){
				*perror = 5;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add string literal to the postfix expression */
			*ppostfix++ = pelement->code;
			c = pinfix[-1];
			while (*pinfix != c && *pinfix) *ppostfix++ = *pinfix++;
			*ppostfix++ = '\0';
			if (*pinfix) pinfix++;

			operand_needed = FALSE;
			new_expression = FALSE;
			break;

		case BINARY_OPERATOR:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators of higher or equal priority to	postfix expression */
			while ((pstacktop->in_stack_pri >= pelement->in_coming_pri)
					&& (pstacktop >= &stack[1])){
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new operator to stack */
			pstacktop++;
			*pstacktop = *pelement;

			operand_needed = TRUE;
			break;

		case UNARY_OPERATOR:
			if (!operand_needed){
				*perror = 5;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators of higher or equal priority to	postfix expression */
			while ((pstacktop->in_stack_pri >= pelement->in_coming_pri)
					&& (pstacktop >= &stack[1])){
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new operator to stack */
			pstacktop++;
			*pstacktop = *pelement;

			new_expression = FALSE;
			break;

		case MINUS_OPERATOR:
			if (operand_needed) {
				/* then assume minus was intended as a unary operator */
				in_coming_pri = UNARY_MINUS_I_C_P;
				in_stack_pri = UNARY_MINUS_I_S_P;
				code = UNARY_MINUS_CODE;
				new_expression = FALSE;
			} else {
				/* then assume minus was intended as a binary operator */
				in_coming_pri = BINARY_MINUS_I_C_P;
				in_stack_pri = BINARY_MINUS_I_S_P;
				code = BINARY_MINUS_CODE;
				operand_needed = TRUE;
			}

			/* add operators of higher or equal priority to	postfix expression */
			while ((pstacktop->in_stack_pri >= in_coming_pri)
					&& (pstacktop >= &stack[1])){
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new operator to stack */
			pstacktop++;
			*pstacktop = *pelement;
			pstacktop->in_stack_pri = in_stack_pri;
			pstacktop->code = code;

			break;

		case SEPARATOR:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators to postfix until open paren */
			while ((pstacktop->element[0] != '(') && (pstacktop->element[0] != '[')
					&& (pstacktop->element[0] != '{')) {
				if (pstacktop == &stack[1] || pstacktop == &stack[0]){
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}
			operand_needed = TRUE;
			break;

		case CLOSE_PAREN:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators to postfix until matching paren */
			while (pstacktop->element[0] != '(') {
				if (pstacktop == &stack[1] || pstacktop == &stack[0]) {
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}
			pstacktop--;	/* remove ( from stack */
			break;

		case CLOSE_BRACKET:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators to postfix until matching bracket */
			while (pstacktop->element[0] != '[') {
				if (pstacktop == &stack[1] || pstacktop == &stack[0]) {
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}
			/* add SUBRANGE operator to postfix */
			if (pstacktop == &stack[0]) {
				*perror = 6;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}
			*ppostfix++ = pstacktop->code;
			pstacktop--;
			break;

		case CLOSE_CURLY:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators to postfix until matching bracket */
			while (pstacktop->element[0] != '{') {
				if (pstacktop == &stack[1] || pstacktop == &stack[0]) {
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}
			/* add REPLACE operator to postfix */
			if (pstacktop == &stack[0]) {
				*perror = 6;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}
			*ppostfix++ = pstacktop->code;
			pstacktop--;
			break;

		case CONDITIONAL:
			if (operand_needed){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add operators of higher priority to postfix expression */
			while ((pstacktop->in_stack_pri > pelement->in_coming_pri)
					&& (pstacktop >= &stack[1])){
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new element to the postfix expression */
			*ppostfix++ = pelement->code;

			/* add : operator with COND_END code to stack */
			if (pelement->element[0] == ':'){
				pstacktop++;
				*pstacktop = *pelement;
				pstacktop->code = COND_END;
			}

			operand_needed = TRUE;
			break;

		case EXPR_TERM:
			if (operand_needed && !new_expression){
				*perror = 4;
				*ppostfixStart = BAD_EXPRESSION; return(-1);
			}

			/* add all operators on stack to postfix */
			while (pstacktop >= &stack[1]){
				if (pstacktop->element[0] == '('){
					*perror = 6;
					*ppostfixStart = BAD_EXPRESSION; return(-1);
				}
				*ppostfix++ = pstacktop->code;
				pstacktop--;
			}

			/* add new element to the postfix expression */
			*ppostfix++ = pelement->code;

			operand_needed = TRUE;
			new_expression = TRUE;
			break;

		default:
			*perror = 8;
			*ppostfixStart = BAD_EXPRESSION; return(-1);
		}
	}
	if (operand_needed){
		*perror = 4;
		*ppostfixStart = BAD_EXPRESSION; return(-1);
	}

	/* add all operators on stack to postfix */
	while (pstacktop >= &stack[1]){
		if (pstacktop->element[0] == '('){
			*perror = 6;
			*ppostfixStart = BAD_EXPRESSION; return(-1);
		}
		*ppostfix++ = pstacktop->code;
		pstacktop--;
	}
	*ppostfix++ = END_STACK;
	*ppostfix = '\0';

	if (ppostfixStart[1] == END_STACK)
		*ppostfixStart = BAD_EXPRESSION;
	else if (*ppostfixStart != USES_STRING)
		*ppostfixStart = NO_STRING;

	return(0);
}
