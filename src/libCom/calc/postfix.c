/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 * Subroutines used to convert an infix expression to a postfix expression
 *
 *      Author:          Bob Dalesio
 *      Date:            12-12-86
*/

/* 
 * Subroutines
 *
 *	Public
 *
 * postfix		convert an algebraic expression to symbolic postfix
 *	args
 *		pinfix		the algebraic expression
 *		ppostfix	the symbolic postfix expression
 *	returns
 *		0		successful
 *		-1		not successful
 * Private routines for postfix
 *
 * find_element		finds a symbolic element in the expression element tbl
 *	args
 *		pbuffer		pointer to the infox expression element
 *		pelement	pointer to the expression element table entry
 *		pno_bytes	pointer to the size of this element
 *	returns
 *		TRUE		element found
 *		FALSE		element not found
 * get_element		finds the next expression element in the infix expr
 *	args
 *		pinfix		pointer into the infix expression
 *		pelement	pointer to the expression element table
 *		pno_bytes	size of the element in the infix expression
 *		plink		pointer to a resolved database reference (N/A)
 *	returns
 *		FINE		found an expression element
 *		VARIABLE	found a database reference
 *		UNKNOWN_ELEMENT	unknown element found in the infix expression
 * match_element	finds an alpha element in the expression table
 *	args
 *		pbuffer		pointer to an alpha expression element
 *		pelement	pointer to the expression element table
 *	returns
 *		TRUE		found the element in the element table
 *		FLASE		expression element not found
 */

#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>

#define epicsExportSharedSymbols
#include	"dbDefs.h"
#include	"epicsStdlib.h"
#include	"postfix.h"
#include	"postfixPvt.h"


/* declarations for postfix */
/* element types */
#define	OPERAND		0
#define UNARY_OPERATOR	1
#define	BINARY_OPERATOR	2
#define	EXPR_TERM	3
#define	COND		4
#define	CLOSE_PAREN	5
#define	CONDITIONAL	6
#define	ELSE		7
#define	SEPERATOR	8
#define	TRASH		9
#define	FLOAT_PT_CONST	10
#define	MINUS_OPERATOR	11

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
	char	type;	/* element type */
	char	code;			/* postfix representation */
};

/*
 * NOTE: DO NOT CHANGE WITHOUT READING THIS NOTICE !!!!!!!!!!!!!!!!!!!!
 * Because the routine that looks for a match in this table takes the first 
 * match it finds, elements whose designations are contained in other elements
 * MUST come first in this list. (e.g. ABS will match A if A preceeds ABS and
 * then try to find BS therefore ABS must be first in this list
 */
static struct expression_element	elements[] = {
/* element	i_s_p	i_c_p	type_element	internal_rep */
{"ABS",		7,	8,	UNARY_OPERATOR,	ABS_VAL}, /* absolute value */
{"NOT",		7,	8,	UNARY_OPERATOR,	UNARY_NEG}, /* unary negate */
{"-",		7,	8,	MINUS_OPERATOR,	UNARY_NEG}, /* unary negate (or binary op) */
{"SQRT",		7,	8,	UNARY_OPERATOR,	SQU_RT}, /* square root */
{"SQR",		7,	8,	UNARY_OPERATOR,	SQU_RT}, /* square root */
{"EXP",		7,	8,	UNARY_OPERATOR,	EXP}, /* exponential function */
{"LOGE",		7,	8,	UNARY_OPERATOR,	LOG_E}, /* log E */
{"LN",		7,	8,	UNARY_OPERATOR,	LOG_E}, /* log E */
{"LOG",		7,	8,	UNARY_OPERATOR,	LOG_10}, /* log 10 */
{"ACOS",		7,	8,	UNARY_OPERATOR,	ACOS}, /* arc cosine */
{"ASIN",		7,	8,	UNARY_OPERATOR,	ASIN}, /* arc sine */
{"ATAN2",	7,	8,	UNARY_OPERATOR,	ATAN2}, /* arc tangent */
{"ATAN",		7,	8,	UNARY_OPERATOR,	ATAN}, /* arc tangent */
{"MAX",		7,	8,	UNARY_OPERATOR,	MAX}, /* maximum of 2 args */
{"MIN",		7,	8,	UNARY_OPERATOR,	MIN}, /* minimum of 2 args */
{"CEIL",		7,	8,	UNARY_OPERATOR,	CEIL}, /* smallest integer >= */
{"FLOOR",	7,	8,	UNARY_OPERATOR,	FLOOR}, /* largest integer <=  */
{"NINT",		7,	8,	UNARY_OPERATOR,	NINT}, /* nearest integer */
{"COSH",		7,	8,	UNARY_OPERATOR,	COSH}, /* hyperbolic cosine */
{"COS",		7,	8,	UNARY_OPERATOR,	COS}, /* cosine */
{"SINH",		7,	8,	UNARY_OPERATOR,	SINH}, /* hyperbolic sine */
{"SIN",		7,	8,	UNARY_OPERATOR,	SIN}, /* sine */
{"TANH",		7,	8,	UNARY_OPERATOR,	TANH}, /* hyperbolic tangent*/
{"TAN",		7,	8,	UNARY_OPERATOR,	TAN}, /* tangent */
{"!",		7,	8,	UNARY_OPERATOR, REL_NOT}, /* not */
{"~",		7,	8,	UNARY_OPERATOR, BIT_NOT}, /* and */
{"RNDM",    	0,	0,	OPERAND,	RANDOM}, /* Random Number */
{"OR",		1,	1,	BINARY_OPERATOR,BIT_OR}, /* or */
{"AND",		2,	2,	BINARY_OPERATOR,BIT_AND}, /* and */
{"XOR",		1,	1,	BINARY_OPERATOR,BIT_EXCL_OR}, /* exclusive or */
{"PI",		0,	0,	OPERAND,	CONST_PI}, /* pi */
{"D2R",		0,	0,	OPERAND,	CONST_D2R}, /* pi/180 */
{"R2D",		0,	0,	OPERAND,	CONST_R2D}, /* 180/pi */
{"A",		0,	0,	OPERAND,	FETCH_A}, /* fetch var A */
{"B",		0,	0,	OPERAND,	FETCH_B}, /* fetch var B */
{"C",		0,	0,	OPERAND,	FETCH_C}, /* fetch var C */
{"D",		0,	0,	OPERAND,	FETCH_D}, /* fetch var D */
{"E",		0,	0,	OPERAND,	FETCH_E}, /* fetch var E */
{"F",		0,	0,	OPERAND,	FETCH_F}, /* fetch var F */
{"G",		0,	0,	OPERAND,	FETCH_G}, /* fetch var G */
{"H",		0,	0,	OPERAND,	FETCH_H}, /* fetch var H */
{"I",		0,	0,	OPERAND,	FETCH_I}, /* fetch var I */
{"J",		0,	0,	OPERAND,	FETCH_J}, /* fetch var J */
{"K",		0,	0,	OPERAND,	FETCH_K}, /* fetch var K */
{"L",		0,	0,	OPERAND,	FETCH_L}, /* fetch var L */
{"0",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"1",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"2",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"3",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"4",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"5",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"6",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"7",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"8",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"9",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{".",		0,	0,	FLOAT_PT_CONST,	CONSTANT}, /* flt pt constant */
{"?",		0,	0,	CONDITIONAL,	COND_IF}, /* conditional */
{":",		0,	0,	CONDITIONAL,	COND_ELSE}, /* else */
{"(",		0,	8,	UNARY_OPERATOR,	PAREN}, /* open paren */
{"^",		6,	6,	BINARY_OPERATOR,EXPON}, /* exponentiation */
{"**",		6,	6,	BINARY_OPERATOR,EXPON}, /* exponentiation */
{"+",		4,	4,	BINARY_OPERATOR,ADD}, /* addition */
#if 0
{"-",		4,	4,	BINARY_OPERATOR,SUB}, /* subtraction */
#endif
{"*",		5,	5,	BINARY_OPERATOR,MULT}, /* multiplication */
{"/",		5,	5,	BINARY_OPERATOR,DIV}, /* division */
{"%",		5,	5,	BINARY_OPERATOR,MODULO}, /* modulo */
{",",		0,	0,	SEPERATOR,	COMMA}, /* comma */
{")",		0,	0,	CLOSE_PAREN,	PAREN}, /* close paren */
{"||",		1,	1,	BINARY_OPERATOR,REL_OR}, /* or */
{"|",		1,	1,	BINARY_OPERATOR,BIT_OR}, /* or */
{"&&",		2,	2,	BINARY_OPERATOR,REL_AND}, /* and */
{"&",		2,	2,	BINARY_OPERATOR,BIT_AND}, /* and */
{">>",		2,	2,	BINARY_OPERATOR,RIGHT_SHIFT}, /* right shift */
{">=",		3,	3,	BINARY_OPERATOR,GR_OR_EQ}, /* greater or equal*/
{">",		3,	3,	BINARY_OPERATOR,GR_THAN}, /* greater than */
{"<<",		2,	2,	BINARY_OPERATOR,LEFT_SHIFT}, /* left shift */
{"<=",		3,	3,	BINARY_OPERATOR,LESS_OR_EQ},/* less or equal to*/
{"<",		3,	3,	BINARY_OPERATOR,LESS_THAN}, /* less than */
{"#",		3,	3,	BINARY_OPERATOR,NOT_EQ}, /* not equal */
{"==",		3,	3,	BINARY_OPERATOR,EQUAL}, /* equal */
{"=",		3,	3,	BINARY_OPERATOR,EQUAL}, /* equal */
{""}
};

/*
 * FIND_ELEMENT
 *
 * find the pointer to an entry in the element table
 */
static int find_element(pbuffer,pelement,pno_bytes)
 char	*pbuffer;
 struct expression_element	**pelement;
 short	*pno_bytes;
 {

 	/* compare the string to each element in the element table */
 	*pelement = &elements[0];
 	while ((*pelement)->element[0] != '\0'){
 		if (strncmp(pbuffer,(*pelement)->element,
		  strlen((*pelement)->element)) == 0){
 			*pno_bytes += strlen((*pelement)->element);
 			return(TRUE);
 		}
 		*pelement += 1;
 	}
 	return(FALSE);
 }
 
/*
 * GET_ELEMENT
 *
 * get an expression element
 */
static int get_element(pinfix,pelement,pno_bytes)
char	*pinfix;
struct expression_element	**pelement;
short		*pno_bytes;
{

	/* get the next expression element from the infix expression */
	if (*pinfix == '\0') return(END);
	*pno_bytes = 0;
	while (*pinfix == 0x20){
		*pno_bytes += 1;
		pinfix++;
	}
	if (*pinfix == '\0') return(END);
	if (!find_element(pinfix,pelement,pno_bytes))
		return(UNKNOWN_ELEMENT);
	return(FINE);

	
}

/*
 * POSTFIX
 *
 * convert an infix expression to a postfix expression
 */
#define MAX_POSTFIX_SIZE 100
long epicsShareAPI postfix(const char *pin,char *ppostfix,short *perror)
{
	short		no_bytes;
	short	operand_needed;
	short	new_expression;
	struct expression_element	stack[80];
	struct expression_element	*pelement;
	struct expression_element	*pstacktop;
	double		constant;
	char   *pposthold, *pc;	
	char in_stack_pri, in_coming_pri, code;
	char           *ppostfixStart = ppostfix;
        char infix[MAX_POSTFIX_SIZE];
        char *pinfix = &infix[0];
        int len;

        len = strlen(pin);
        if(len>=MAX_POSTFIX_SIZE) return(-1);
        strcpy(infix,pin);
	/* convert infix expression to upper case */
	for (pc=pinfix; *pc; pc++) {
		if (islower((int)*pc)) *pc = toupper((int)*pc);
	}

	/* place the expression elements into postfix */
	operand_needed = TRUE;
	new_expression = TRUE;
	*ppostfix = END_STACK;
	*perror = 0;
	if (* pinfix == 0 )
		return(0);
	pstacktop = stack;
	while (get_element(pinfix,&pelement,&no_bytes) != END){
	    pinfix += no_bytes;
	    switch (pelement->type){

	    case OPERAND:
		if (!operand_needed){
		    *perror = 5;
		    *ppostfixStart = BAD_EXPRESSION; return(-1);
		}

		/* add operand to the expression */
		*ppostfix++ = pelement->code;

		operand_needed = FALSE;
		new_expression = FALSE;
		break;

	    case FLOAT_PT_CONST:
		if (!operand_needed){
		    *perror = 5;
		    *ppostfixStart = BAD_EXPRESSION; return(-1);
		}

		/* add constant to the expression */
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
		if ( epicsScanDouble(ppostfix, &constant) != 1) {
			*ppostfix = '\0';
		} else {
			memcpy(ppostfix,(void *)&constant,8);
		}
		ppostfix+=8;

		operand_needed = FALSE;
		new_expression = FALSE;
		break;

	    case BINARY_OPERATOR:
		if (operand_needed){
		    *perror = 4;
		    *ppostfixStart = BAD_EXPRESSION; return(-1);
		}

		/* add operators of higher or equal priority to	*/
		/* postfix notation				*/
		while ((pstacktop >= stack+1) &&
		  (pstacktop->in_stack_pri >= pelement->in_coming_pri)) {
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

		/* add operators of higher or equal priority to	*/
		/* postfix notation 				*/
		while ((pstacktop >= stack+1) &&
		  (pstacktop->in_stack_pri >= pelement->in_coming_pri)) {
		      *ppostfix++ = pstacktop->code;
		      pstacktop--;
		 }

		/* add new operator to stack */
		pstacktop++;
		*pstacktop = *pelement;

		new_expression = FALSE;
		break;

	    case MINUS_OPERATOR:
		if (operand_needed){
			/* then assume minus was intended as a unary operator */
			in_coming_pri = UNARY_MINUS_I_C_P;
			in_stack_pri = UNARY_MINUS_I_S_P;
			code = UNARY_MINUS_CODE;
			new_expression = FALSE;
		}
		else {
			/* then assume minus was intended as a binary operator */
			in_coming_pri = BINARY_MINUS_I_C_P;
			in_stack_pri = BINARY_MINUS_I_S_P;
			code = BINARY_MINUS_CODE;
			operand_needed = TRUE;
		}

		/* add operators of higher or equal priority to	*/
		/* postfix notation				*/
		while ((pstacktop >= stack+1) &&
		  (pstacktop->in_stack_pri >= in_coming_pri)) {
		    *ppostfix++ = pstacktop->code;
		    pstacktop--;
		}

		/* add new operator to stack */
		pstacktop++;
		*pstacktop = *pelement;
		pstacktop->in_stack_pri = in_stack_pri;
		pstacktop->code = code;

		break;

	    case SEPERATOR:
		if (operand_needed){
		    *perror = 4;
		    *ppostfixStart = BAD_EXPRESSION; return(-1);
		}

		/* add operators to postfix until open paren */
		while (pstacktop->element[0] != '('){
		    if (pstacktop == stack+1 ||
		        pstacktop == stack){
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
		while (pstacktop->element[0] != '('){
		    if (pstacktop == stack+1 ||
		        pstacktop == stack){
			*perror = 6;
			*ppostfixStart = BAD_EXPRESSION; return(-1);
		    }
		    *ppostfix++ = pstacktop->code;
		    pstacktop--;
		}
		pstacktop--;	/* remove ( from stack */
		break;

	    case CONDITIONAL:
		if (operand_needed){
		    *perror = 4;
		    *ppostfixStart = BAD_EXPRESSION; return(-1);
		}

		/* add operators of higher priority to	*/
		/* postfix notation 				*/
		while ((pstacktop >= stack+1) &&
		  (pstacktop->in_stack_pri > pelement->in_coming_pri)) {
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
		while (pstacktop >= stack+1){
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
	while (pstacktop >= stack+1){
	    if (pstacktop->element[0] == '('){
		*perror = 6;
		*ppostfixStart = BAD_EXPRESSION; return(-1);
	    }
	    *ppostfix++ = pstacktop->code;
	    pstacktop--;
	}
	*ppostfix = END_STACK;

	return(0);
}
