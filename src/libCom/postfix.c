/* share/src/libCom/postfix.c  $Id$ */
 
/* Subroutines used to convert an infix expression to a postfix expression */
/*
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

#ifdef vxWorks
#   include <stdioLib.h>
#   include <strLib.h>
#else
#   include <stdio.h>
#   include <string.h>
#endif

#include	<dbDefs.h>
#include	<post.h>

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
#define	TRASH		8

/* flags end of element table */
#define	END_ELEMENTS	-1

/* parsing return values */
#define	FINE		0
#define	UNKNOWN_ELEMENT	-1
#define	END		-2

struct link{
	short	field1;
	short	field2;
};

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
"ABS",		7,	8,	UNARY_OPERATOR,	ABS_VAL,   /* absolute value */
"NOT",		7,	8,	UNARY_OPERATOR,	UNARY_NEG, /* unary negate */
"SQR",		7,	8,	UNARY_OPERATOR,	SQU_RT,    /* square root */
"LOGE",		7,	8,	UNARY_OPERATOR,	LOG_E,     /* log E */
"LOG",		7,	8,	UNARY_OPERATOR,	LOG_10,    /* log 10 */
"ACOS",		7,	8,	UNARY_OPERATOR,	ACOS,      /* arc cosine */
"ASIN",		7,	8,	UNARY_OPERATOR,	ASIN,      /* arc sine */
"ATAN",		7,	8,	UNARY_OPERATOR,	ATAN,      /* arc tangent */
"COSH",		7,	8,	UNARY_OPERATOR,	COSH,      /* hyperbolic cosine */
"COS",		7,	8,	UNARY_OPERATOR,	COS,       /* cosine */
"SINH",		7,	8,	UNARY_OPERATOR,	SINH,      /* hyperbolic sine */
"SIN",		7,	8,	UNARY_OPERATOR,	SIN,       /* sine */
"TANH",		7,	8,	UNARY_OPERATOR,	TANH,      /* hyperbolic tangent*/
"TAN",		7,	8,	UNARY_OPERATOR,	TAN,       /* tangent */
"!",		7,	8,	UNARY_OPERATOR, REL_NOT,   /* not */
"~",		7,	8,	UNARY_OPERATOR, BIT_NOT,   /* and */
"RNDM",    	0,	0,	OPERAND,	RANDOM,    /* Random Number */
"OR",		1,	1,	BINARY_OPERATOR,BIT_OR,    /* or */
"AND",		2,	2,	BINARY_OPERATOR,BIT_AND,   /* and */
"XOR",		1,	1,	BINARY_OPERATOR,BIT_EXCL_OR, /* exclusive or */
"A",		0,	0,	OPERAND,	FETCH_A,   /* fetch var A */
"B",		0,	0,	OPERAND,	FETCH_B,   /* fetch var B */
"C",		0,	0,	OPERAND,	FETCH_C,   /* fetch var C */
"D",		0,	0,	OPERAND,	FETCH_D,   /* fetch var D */
"E",		0,	0,	OPERAND,	FETCH_E,   /* fetch var E */
"F",		0,	0,	OPERAND,	FETCH_F,   /* fetch var F */
"G",		0,	0,	OPERAND,	FETCH_G,   /* fetch var G */
"H",		0,	0,	OPERAND,	FETCH_H,   /* fetch var H */
"I",		0,	0,	OPERAND,	FETCH_I,   /* fetch var I */
"J",		0,	0,	OPERAND,	FETCH_J,   /* fetch var J */
"K",		0,	0,	OPERAND,	FETCH_K,   /* fetch var K */
"L",		0,	0,	OPERAND,	FETCH_L,   /* fetch var L */
"a",		0,	0,	OPERAND,	FETCH_A,   /* fetch var A */
"b",		0,	0,	OPERAND,	FETCH_B,   /* fetch var B */
"c",		0,	0,	OPERAND,	FETCH_C,   /* fetch var C */
"d",		0,	0,	OPERAND,	FETCH_D,   /* fetch var D */
"e",		0,	0,	OPERAND,	FETCH_E,   /* fetch var E */
"f",		0,	0,	OPERAND,	FETCH_F,   /* fetch var F */
"g",		0,	0,	OPERAND,	FETCH_G,   /* fetch var G */
"h",		0,	0,	OPERAND,	FETCH_H,   /* fetch var H */
"i",		0,	0,	OPERAND,	FETCH_I,   /* fetch var I */
"j",		0,	0,	OPERAND,	FETCH_J,   /* fetch var J */
"k",		0,	0,	OPERAND,	FETCH_K,   /* fetch var K */
"l",		0,	0,	OPERAND,	FETCH_L,   /* fetch var L */
"?",		0,	0,	CONDITIONAL,	COND_ELSE, /* conditional */
":",		0,	0,	ELSE,		COND_ELSE, /* else */
"(",		0,	8,	UNARY_OPERATOR,	PAREN,        /* open paren */
"^",		6,	6,	BINARY_OPERATOR,EXPON,     /* exponentiation */
"**",		6,	6,	BINARY_OPERATOR,EXPON,     /* exponentiation */
"+",		4,	4,	BINARY_OPERATOR,ADD,       /* addition */
"-",		4,	4,	BINARY_OPERATOR,SUB,       /* subtraction */
"*",		5,	5,	BINARY_OPERATOR,MULT,      /* multiplication */
"/",		5,	5,	BINARY_OPERATOR,DIV,       /* division */
"%",		5,	5,	BINARY_OPERATOR,MODULO,    /* modulo */
")",		0,	0,	CLOSE_PAREN,	PAREN,     /* close paren */
"||",		1,	1,	BINARY_OPERATOR,REL_OR,    /* or */
"|",		1,	1,	BINARY_OPERATOR,BIT_OR,    /* or */
"&&",		2,	2,	BINARY_OPERATOR,REL_AND,   /* and */
"&",		2,	2,	BINARY_OPERATOR,BIT_AND,   /* and */
">>",		2,	2,	BINARY_OPERATOR,RIGHT_SHIFT, /* right shift */
">=",		3,	3,	BINARY_OPERATOR,GR_OR_EQ,  /* greater or equal*/
">",		3,	3,	BINARY_OPERATOR,GR_THAN,    /* greater than */
"<<",		2,	2,	BINARY_OPERATOR,LEFT_SHIFT, /* left shift */
"<=",		3,	3,	BINARY_OPERATOR,LESS_OR_EQ,/* less or equal to*/
"<",		3,	3,	BINARY_OPERATOR,LESS_THAN, /* less than */
"#",		3,	3,	BINARY_OPERATOR,NOT_EQ,    /* not equal */
"=",		3,	3,	BINARY_OPERATOR,EQUAL,     /* equal */
""
};

/*
 * FIND_ELEMENT
 *
 * find the pointer to an entry in the element table
 */
static find_element(pbuffer,pelement,pno_bytes)
 register char	*pbuffer;
 register struct expression_element	**pelement;
 register short	*pno_bytes;
 {

 	/* compare the string to each element in the element table */
 	*pelement = &elements[0];
 	while ((*pelement)->element[0] != NULL){
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
static get_element(pinfix,pelement,pno_bytes,plink)
register char	*pinfix;
register struct expression_element	**pelement;
register short		*pno_bytes;
struct link		*plink;
{
	char	buffer[40];
	register short	i;

	/* get the next expression element from the infix expression */
	if (*pinfix == NULL) return(END);
	*pno_bytes = 0;
	while (*pinfix == 0x20){
		*pno_bytes += 1;
		pinfix++;
	}
	if (!find_element(pinfix,pelement,pno_bytes))
		return(UNKNOWN_ELEMENT);
	return(FINE);

	
}

/*
 * POSTFIX
 *
 * convert an infix expression to a postfix expression
 */
long postfix(pinfix,ppostfix,perror)
register char	*pinfix;
register char	*ppostfix;
short		*perror;
{
	short		got_if;
	short		got_else;
	short		no_bytes;
	register short	operand_needed;
	register short	new_expression;
	register short	link_inx;	/* index into variable table	*/
	struct link	new_link;
	struct expression_element	stack[80];
	struct expression_element	*pelement;
	register struct expression_element	*pstacktop;

	/* place the expression elements into postfix */
	got_if = FALSE;
	got_else = FALSE;
	operand_needed = TRUE;
	new_expression = TRUE;
	pstacktop = &stack[0];
	while (get_element(pinfix,&pelement,&no_bytes,&new_link) != END){

	    pinfix += no_bytes;
	    switch (pelement->type){

	    case OPERAND:
		if (!operand_needed){
		    *perror = 5;
		    return(-1);
		}

		/* add operand to the expression */
		*ppostfix++ = pelement->code;

		operand_needed = FALSE;
		new_expression = FALSE;
		break;

	    case BINARY_OPERATOR:
		if (operand_needed){
		    *perror = 4;
		    return(-1);
		}

		/* add operators of higher or equal priority to	*/
		/* postfix notation				*/
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
		    return(-1);
		}

		/* add operators of higher or equal priority to	*/
		/* postfix notation 				*/
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

	    case CLOSE_PAREN:
		if (operand_needed){
		    *perror = 4;
		    return(-1);
		}

		/* add operators to postfix until matching paren */
		while (pstacktop->element[0] != '('){
		    if (pstacktop == &stack[1]){
			*perror = 6;
			return(-1);
		    }
		    *ppostfix++ = pstacktop->code;
		    pstacktop--;
		}
		pstacktop--;	/* remove ( from stack */
		break;

	    /* conditional includes expression termination */
	    case CONDITIONAL:
		if (got_if){
		    *perror = 9;
		    return(-1);
		}
		got_if = TRUE;

	    /* else includes expression termination */
	    case ELSE:
		if (pelement->type == ELSE){
		    if (!got_if){
			*perror = 11;
			return(-1);
		    }
		    if (got_else){
			*perror = 10;
			return(-1);
		    }
		    got_else = TRUE;
		}

	    case EXPR_TERM:
		if (operand_needed && !new_expression){
		    *perror = 4;
		    return(-1);
		}

		/* add all operators on stack to postfix */
		while (pstacktop >= &stack[1]){
		    if (pstacktop->element[0] == '('){
			*perror = 6;
			return(-1);
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
		return(-1);
	    }
	}
	if (operand_needed){
		*perror = 4;
		return(-1);
	}

	/* add all operators on stack to postfix */
	while (pstacktop >= &stack[1]){
	    if (pstacktop->element[0] == '('){
		*perror = 6;
		return(-1);
	    }
	    *ppostfix++ = pstacktop->code;
	    pstacktop--;
	}
	*ppostfix = END_STACK;

	return(0);
}
