/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */
/*
 *	Author: Julie Sander and Bob Dalesio
 *	Date:	07-27-87
 */

/* This module contains the code for processing the arithmetic
 * expressions defined in calculation records. postfix must be called
 * to convert a valid infix expression to postfix. CalcPerform
 * calculates the postfix expression.
 *
 * Subroutines
 *
 *	Public
 *
 * calcPerform		perform the calculation
 *	    args
 *		double *pargs	address of arguments (12)
 *		double *presult address of result
 *		char   *rpcl	address of reverse polish buffer
 *	    returns
 *		0		fetched successfully
 *		-1		fetch failed
 *
 * Private routine for calcPerform
 *	local_random		random number generator
 *	    returns
 *		double value between 0.00 and 1.00
 */

#include	<stdlib.h>
#include	<stddef.h>
#include	<stdio.h>
#include	<string.h>

#define epicsExportSharedSymbols
#include	"dbDefs.h"
#include	"epicsMath.h"
#include	"postfix.h"
#include	"postfixPvt.h"

static double	local_random(void);

#define	NOT_SET		0
#define	TRUE_COND	1
#define	FALSE_COND	2

#ifndef PI
#define PI 3.141592654
#endif

epicsShareFunc long epicsShareAPI 
	calcPerform(double *parg, double *presult, char *post)
{
	double *pstacktop;	/* stack of values	*/
	double		stack[80];
	short		temp1;
	double 		*top;
	int 		itop;		/* integer top value	*/
	int 		inexttop;	/* ineteger next to top value 	*/
	short 		cond_flag;	/* conditional else flag	*/
	short 		got_if;

	/* initialize flag  */
	cond_flag = NOT_SET;
	pstacktop = &stack[0];

/* DEBUG print statements
for (short i=0;i<184;i++){
printf ("%d_",post[i]);
if ( post[i] == END_STACK ) break;
if ( post[i] == 71 ) i=i+8;
}
printf ("*FINISHED*\n");
*/
        if(*post == BAD_EXPRESSION) return(-1);

	/* set post to postfix expression in calc structure */
	top = pstacktop;

	/* polish calculator loop */
	while (*post != END_STACK){

		switch (*post){
		case FETCH_A:
			++pstacktop;
			*pstacktop = parg[0];
			break;

		case FETCH_B:
			++pstacktop;
			*pstacktop = parg[1];
			break;

		case FETCH_C:
			++pstacktop;
			*pstacktop = parg[2];
			break;

		case FETCH_D:
			++pstacktop;
			*pstacktop = parg[3];
			break;

		case FETCH_E:
			++pstacktop;
			*pstacktop = parg[4];
			break;

		case FETCH_F:
			++pstacktop;
			*pstacktop = parg[5];
			break;

		case FETCH_G:
			++pstacktop;
			*pstacktop = parg[6];
			break;

		case FETCH_H:
			++pstacktop;
			*pstacktop = parg[7];
			break;

		case FETCH_I:
			++pstacktop;
			*pstacktop = parg[8];
			break;

		case FETCH_J:
			++pstacktop;
			*pstacktop = parg[9];
			break;

		case FETCH_K:
			++pstacktop;
			*pstacktop = parg[10];
			break;

		case FETCH_L:
			++pstacktop;
			*pstacktop = parg[11];
			break;

		case CONST_PI:
			++pstacktop;
			*pstacktop = PI;
			break;

		case CONST_D2R:
			++pstacktop;
			*pstacktop = PI/180.;
			break;

		case CONST_R2D:
			++pstacktop;
			*pstacktop = 180./PI;
			break;

		case ADD:
			--pstacktop;
			*pstacktop = *pstacktop + *(pstacktop+1);
			break;

		case SUB:
			--pstacktop;
			*pstacktop = *pstacktop - *(pstacktop+1);
			break;

		case MULT:
			--pstacktop;
			*pstacktop = *pstacktop * *(pstacktop+1);
			break;

		case DIV:
			--pstacktop;
			if (*(pstacktop+1) == 0) /* can't divide by zero */
				return(-1);
			*pstacktop = *pstacktop / *(pstacktop+1);
			break;

		case COND_IF:
			/* if false condition then skip true expression */
			if (*pstacktop == 0.0) {
				/* skip to matching COND_ELSE */
				for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
					if (*(post+1) == CONSTANT  ) post+=8;
					else if (*(post+1) == COND_IF  ) got_if++;
					else if (*(post+1) == COND_ELSE) got_if--;
				}
			}
			/* remove condition from stack top */
			--pstacktop;
			break;
				
		case COND_ELSE:
			/* result, true condition is on stack so skip false condition  */
			/* skip to matching COND_END */
			for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
				if (*(post+1) == CONSTANT  ) post+=8;
				else if (*(post+1) == COND_IF ) got_if++;
				else if (*(post+1) == COND_END) got_if--;
			}
			break;

		case COND_END:
			break;

		case ABS_VAL:
			if (*pstacktop < 0) *pstacktop = -*pstacktop;
			break;

		case UNARY_NEG:
			*pstacktop = -1* (*pstacktop);
			break;

		case SQU_RT:
			if (*pstacktop < 0) return(-1);	/* undefined */
			*pstacktop = sqrt(*pstacktop);
			break;

		case EXP:
			*pstacktop = exp(*pstacktop);
			break;

		case LOG_10:
			*pstacktop = log10(*pstacktop);
			break;

		case LOG_E:
			*pstacktop = log(*pstacktop);
			break;

		case RANDOM:
			++pstacktop;
			*pstacktop = local_random();
			break;

		case EXPON:
			--pstacktop;
			if (*pstacktop == 0) break;
			if (*pstacktop < 0){
				temp1 = (int) *(pstacktop+1);
				/* is exponent an integer */
				if ((*(pstacktop+1) - (double)temp1) != 0) return (-1);
        			*pstacktop = exp(*(pstacktop+1) * log(-*pstacktop));
				/* is value negative */
				if ((temp1 % 2) > 0) *pstacktop = -*pstacktop;
			}else{
        			*pstacktop = exp(*(pstacktop+1) * log(*pstacktop));
			}
			break;

		case MODULO:
			--pstacktop;
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop+1);
			if (inexttop == 0)
				return(-1);
			*pstacktop =  itop % inexttop;
			break;

		case REL_OR:
			--pstacktop;
			*pstacktop = (*pstacktop || *(pstacktop+1));
			break;

		case REL_AND:
			--pstacktop;
			*pstacktop = (*pstacktop && *(pstacktop+1));
			break;

		case BIT_OR:
			/* force double values into integers and or them */
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop | itop);
			break;

		case BIT_AND:
			/* force double values into integers and and them */
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop & itop);
			break;

		case BIT_EXCL_OR:
			/*force double values to integers to exclusive or them*/
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop ^ itop);
			break;

		case GR_OR_EQ:
			--pstacktop;
			*pstacktop = *pstacktop >= *(pstacktop+1);
			break;

		case GR_THAN:
			--pstacktop;
			*pstacktop = *pstacktop > *(pstacktop+1);
			break;

		case LESS_OR_EQ:
			--pstacktop;
			*pstacktop = *pstacktop <= *(pstacktop+1);
			break;

		case LESS_THAN:
			--pstacktop;
			*pstacktop = *pstacktop < *(pstacktop+1);
			break;

		case NOT_EQ:
			--pstacktop;
			*pstacktop = *pstacktop != *(pstacktop+1);
			break;

		case EQUAL:
			--pstacktop;
			*pstacktop = (*pstacktop == *(pstacktop+1));
			break;

		case RIGHT_SHIFT:
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop >> itop);
			break;

		case LEFT_SHIFT:
			itop = (int)*pstacktop;
			inexttop = (int)*(pstacktop-1);
			--pstacktop;
			*pstacktop = (inexttop << itop);
			break;

                case MAX:
                        --pstacktop;
                        if (*pstacktop < *(pstacktop+1))
                                *pstacktop = *(pstacktop+1);
                        break;
 
                case MIN:
                        --pstacktop;
                        if (*pstacktop > *(pstacktop+1))
                                *pstacktop = *(pstacktop+1);
                        break;
 

		case ACOS:
			*pstacktop = acos(*pstacktop);
			break;

		case ASIN:
			*pstacktop = asin(*pstacktop);
			break;

		case ATAN:
			*pstacktop = atan(*pstacktop);
			break;

		case ATAN2:
			--pstacktop;
			*pstacktop = atan2(*(pstacktop+1), *pstacktop);
			break;

		case COS:
			*pstacktop = cos(*pstacktop);
			break;

		case SIN:
			*pstacktop = sin(*pstacktop);
			break;

		case TAN:
			*pstacktop = tan(*pstacktop);
			break;

		case COSH:
			*pstacktop = cosh(*pstacktop);
			break;

		case SINH:
			*pstacktop = sinh(*pstacktop);
			break;

		case TANH:
			*pstacktop = tanh(*pstacktop);
			break;

		case CEIL:
			*pstacktop = ceil(*pstacktop);
			break;

		case FLOOR:
			*pstacktop = floor(*pstacktop);
			break;

		case NINT:
			*pstacktop = (double)(long)((*pstacktop) >= 0 ? (*pstacktop)+0.5 : (*pstacktop)-0.5);
			break;

		case REL_NOT:
			*pstacktop = ((*pstacktop)?0:1);
			break;

		case BIT_NOT:
			itop = (int)*pstacktop;
			*pstacktop = ~itop;
			break;

		case CONSTANT:
			++pstacktop;
			++post;
			if ( post == NULL ) {
				++post;
				printf("%.7s bad constant in expression\n",post);
				break;
			}
			memcpy((void *)pstacktop,post,8);
			post+=7;
			break;
		default:
			printf("%d bad expression element\n",*post);
			break;
		}

		/* move ahead in postfix expression */
		++post;
	}

	/* if everything is peachy,the stack should end at its first position */
	if (++top == pstacktop)
		*presult = *pstacktop;
	else
		return(-1);
	return(((isnan(*presult)||isinf(*presult)) ? -1 : 0));
}


/*
 * RAND
 *
 * generates a random number between 0 and 1 using the
 * seed = (multy * seed) + addy         Random Number Generator by Knuth
 *                                              SemiNumerical Algorithms
 *                                              Chapter 1
 * randy = seed / 65535.0          To normalize the number between 0 - 1
 */
static unsigned short seed = 0xa3bf;
static unsigned short multy = 191 * 8 + 5;  /* 191 % 8 == 5 */
static unsigned short addy = 0x3141;
static double local_random()
{
        double  randy;

        /* random number */
        seed = (seed * multy) + addy;
        randy = (float) seed / 65535.0;

        /* between 0 - 1 */
        return(randy);
}
