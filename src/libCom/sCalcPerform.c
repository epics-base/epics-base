/* $Id$ */
/*
 *	Author: Julie Sander and Bob Dalesio
 *	Date:	07-27-87
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * 03-18-98 tmm Essentially rewritten to support string operators
 * 5/21/98 fixed ?: operator
 */

/* This module contains the code for processing the arithmetic
 * expressions defined in calculation records. postfix must be called
 * to convert a valid infix expression to postfix. sCalcPerform
 * calculates the postfix expression.
 *
 * Subroutines
 *
 *	Public
 *
 * sCalcPerform		perform the calculation
 *  args
 *    double *parg       address of arguments
 *    int    numArgs     number of arguments in pargs array
 *    double *psarg      address of string arguments
 *    int    numSArgs    number of string arguments in psargs array
 *    double *presult    address of double result
 *    char   *psresult   address of string-result buffer
 *    int    lenSresult  length of string-result buffer
 *    char   *rpcl       address of postfix buffer
 *  returns
 *    0   fetched successfully
 *    -1  fetch failed
 *
 * Private routine for sCalcPerform
 * local_random  random number generator
 *  returns
 *    double value between 0.00 and 1.00
 */

#ifdef vxWorks
#include <vxWorks.h>
#endif

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<math.h>

#include	"dbDefs.h"
#include	"cvtFast.h"
#define epicsExportSharedSymbols
#include	"sCalcPostfix.h"
#include	"sCalcPostfixPvt.h"

static double	local_random();

static int isnan(double d)
{
	union { long l[2]; double d; } u;
	u.d = d;
	if ((u.l[0] & 0x7ff00000) != 0x7ff00000) return(0);
	if (u.l[1] || (u.l[0] & 0x000fffff)) return(1);
	return(0);
}
#define myNINT(a) ((int)((a) >= 0 ? (a)+0.5 : (a)-0.5))
#ifndef PI
#define PI 3.141592654
#endif
#define MAX(a,b) (a)>(b)?(a):(b)
#define MIN(a,b) (a)<(b)?(a):(b)


#define STACKSIZE 30	/* <-------------------------<<<<<<< */

struct stackElement {
	double d;
	char *s;
};

#define OVERRIDESTDCALC 0
#define DEBUG 0
volatile int sCalcPerformDebug = 0;
#if DEBUG
int sCalcStackHW = 0;
int sCalcStackLW = 0;
#define INC(ps) {if ((int)(++(ps)-top) > sCalcStackHW) sCalcStackHW = (int)((ps)-top);}
#define DEC(ps) {if ((int)(--(ps)-top) < sCalcStackLW) sCalcStackLW = (int)((ps)-top);}
#else
#define INC(ps) ++ps
#define DEC(ps) ps--
#endif


#define isDouble(ps) ((ps)->s==NULL)
#define isString(ps) ((ps)->s)

#define cleanStringElement(ps) {free((ps)->s); (ps)->s=NULL;}

static void cleanStackElement(struct stackElement *ps)
{
	if (isString(ps)) {
		free(ps->s);
		ps->s = NULL;
	}
}

static void cleanup(struct stackElement *ps, struct stackElement *pe)
{
	for (; ps <= pe; ps++) {
		if (isString(ps)) {
			free(ps->s);
			ps->s = NULL;
		}
	}
}

#define toDouble(ps) {if (isString(ps)) to_double(ps);}

/* convert stack element to double */
static void to_double(struct stackElement *ps)
{
	ps->d = atof(ps->s);
	free(ps->s);
	ps->s = NULL;
}

#define toString(ps) {if (isDouble(ps)) to_string(ps);}

/* convert stack element to string */
static void to_string(struct stackElement *ps)
{
	ps->s = calloc(20, 1);
	/* any precision greater than 8 results in (slow) sprintf call */
	if (isnan(ps->d))
		strcpy(ps->s,"NaN");
	else
		(void)cvtDoubleToString(ps->d, ps->s, 8);
}

static char *findConversionIndicator(char *s)
{
	char *cc=NULL, *s1, *retval;

	while (s && *s) {
		if ((s1 = strstr(s, "%%")) != NULL) {
			/* not a conversion/assignment indicator; skip over */
			s = s1+2; continue;
		}
		if ((s = strchr(s, (int)'%')) == NULL) {
			return(NULL);
		}
		if ((cc = strpbrk(s, "pwn$c[deEfgGiousxX")) == NULL) {
			return(NULL);
		}
		/*
		 * (*cc) is a conversion character; look for suppressed assignment
		 * ('*' occurring after '%' and before conversion character)
		 */
		s1 = strchr(s, (int)'*');
		if (s1 && (s1 < cc)) {
			/* suppressed assignment; skip past conversion character */
			s = cc+1;
			if (*cc == '[') {
				/* skip character set ([..], []..], or [^]..]) */
				if (cc[1] == ']') {
					s = &(cc[2]);
				} else if ((cc[1] == '^') && (cc[2] == ']')) {
					s = &(cc[3]);
				}
				s = strchr(s, (int)']');
				if (s == NULL) {
					/* bad character-set syntax */
					return(NULL);
				}
				s++; /* skip past ']' */
			}
			/* keep looking for conversion/assignment character */

			continue;
		} else {
			/* (*cc) is a conversion/assignment character */
			break;
		}
	}
	if (cc == NULL) return(NULL);
	retval = cc;
	/*
	 * (*cc) is a conversion/assignment indicator.  Make sure there
	 * aren't any more in the format string.
	 */
	s = cc+1;
	while (s && *s) {
		if ((s1 = strstr(s, "%%")) != NULL) {
			/* not a conversion/assignment indicator; skip over */
			s = s1+2; continue;
		}
		if ((s = strchr(s, (int)'%')) == NULL) return(retval);
		if ((cc = strpbrk(s, "pwn$c[deEfgGiousxX")) == NULL) return(retval);
		/*
		 * (*cc) is a conversion character; look for suppressed assignment
		 * ('*' occurring after '%' and before conversion character)
		 */
		s1 = strchr(s, (int)'*');
		if (s1 && (s1 < cc)) {
			/* suppressed assignment; skip past conversion character */
			s = cc+1;
			if (*cc == '[') {
				/* skip character set ([..], []..], or [^]..]) */
				if (cc[1] == ']') {
					s = &(cc[2]);
				} else if ((cc[1] == '^') && (cc[2] == ']')) {
					s = &(cc[3]);
				}
				s = strchr(s, (int)']');
				if (s == NULL) return(NULL); /* bad character-set syntax */
				s++; /* skip past ']' */
			}
			continue;
		} else {
			/* (*cc) assignment is not suppressed */
			return(NULL);
		}
	}
	return(retval);
}

#if OVERRIDESTDCALC
/* Override standard EPICS expression evaluator (if we're loaded after it) */
epicsShareFunc long epicsShareAPI 
	calcPerform(double *parg, double *presult, char *post)
{
	return(sCalcPerform(parg, 12, NULL, 0, presult, NULL, 0, post));
}
#endif

epicsShareFunc long epicsShareAPI 
	sCalcPerform(double *parg, int numArgs, char **psarg, int numSArgs, double *presult, char *psresult, int lenSresult, char *post)
{
	struct stackElement stack[STACKSIZE], *top;
	register struct stackElement *ps, *ps1, *ps2;
	char				*s2, tmpstr[1000];
	register char		*s, *s1;
	register int		i, j, k;
	long				l;
	unsigned short		ui;
	unsigned long		ul;
	float				f;
	double				d;
	register double		*topd, *pd;
	short 				h, got_if;

#if DEBUG
	if (sCalcPerformDebug>=10) {
		int	more;
		printf("sCalcPerform: postfix:");
		for (s=post, more=1; more;) {
			printf("%2d ", *s);
			switch (*s) {
			case END_STACK:
				more = 0;
				break;
			case LITERAL:
				printf("(0x");
				for (i=0, s++; i<8; i++, s++) printf("%2x ", (unsigned int)(unsigned char)*s);
				printf(") ");
				break;
			case SLITERAL:
				s++; /* point past code */
				printf("'");
				while (*s) printf("%c", *s++);
				printf("' ");
				s++;
				break;
			case FETCH:
				s++; /* point past code */
				printf("@%d ", *s++);
				break;
			case SFETCH:
				s++; /* point past code */
				printf("$%d ", *s++);
				break;
			default:
				if (*s == BAD_EXPRESSION) more=0;
				s++;
				break;
			}
		}
		printf("\n");
	}
#endif

	/* Make sure postfix expression exists and is nontrivial */
	/* if ((*post == END_STACK) || (*post == BAD_EXPRESSION)) return(-1);*/
	if (*post == BAD_EXPRESSION) return(-1);


	if (*post++ != USES_STRING) {

		topd = pd = (double *)&stack[10];
		pd--;

		/* No string expressions */
		while (*post != END_STACK) {

			switch (*post){

			case FETCH:
				++pd;
				++post;
				*pd = (*post < numArgs) ? parg[*post] : 0;
				break;

			case STORE:
				/* not implemented */
				return(-1);

			case CONST_PI:
				++pd;
				*pd = PI;
				break;

			case CONST_D2R:
				++pd;
				*pd = PI/180.;
				break;

			case CONST_R2D:
				++pd;
				*pd = 180./PI;
				break;

			case CONST_S2R:
				++pd;
				*pd = PI/(180.*3600);
				break;

			case CONST_R2S:
				++pd;
				*pd = (180.*3600)/PI;
				break;

			case ADD:
				--pd;
				*pd = *pd + pd[1];
				break;

			case SUB:
				--pd;
				*pd = *pd - pd[1];
				break;

			case MULT:
				--pd;
				*pd = *pd * pd[1];
				break;

			case DIV:
				--pd;
				if (pd[1] == 0) /* can't divide by zero */
					return(-1);
				*pd = *pd / pd[1];
				break;

			case COND_IF:
				/* if false condition then skip true expression */
				if (*pd == 0.0) {
					/* skip to matching COND_ELSE */
					for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
						switch(post[1]) {
						case LITERAL: post+=8; break;
						case COND_IF: got_if++; break;
						case COND_ELSE: got_if--; break;
						case FETCH: case SFETCH: post++; break;
						}
					}
				}
				/* remove condition from stack top */
				--pd;
				break;
				
			case COND_ELSE:
				/* result, true condition is on stack so skip false condition  */
				/* skip to matching COND_END */
				for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
					switch(post[1]) {
					case LITERAL: post+=8; break;
					case COND_IF: got_if++; break;
					case COND_END: got_if--; break;
					case FETCH: post++; break;
					}
				}
				break;

			case COND_END:
				break;

			case ABS_VAL:
				if (*pd < 0 ) *pd *= -1;
				break;

			case UNARY_NEG:
				*pd *= -1;
				break;

			case SQU_RT:
				/* check for neg number */
				if (*pd < 0) return(-1);	
				*pd = sqrt(*pd);
				break;

			case EXP:
				*pd = exp(*pd);
				break;

			case LOG_10:
				/* check for neg number */
				if (*pd < 0) return(-1);
				*pd = log10(*pd);
				break;

			case LOG_E:
				/* check for neg number */
				if (*pd < 0) return(-1);
				*pd = log(*pd);
				break;

			case RANDOM:
				++pd;
				*pd = local_random();
				break;

			case EXPON:
				--pd;
				if (*pd == 0) break;
				if (*pd < 0) {
					i = (int) pd[1];
					/* is exponent an integer? */
					if ((pd[1] - (double)i) != 0) return (-1);
        			*pd = exp(pd[1] * log(-(*pd)));
					/* is value negative */
					if ((i % 2) > 0) *pd = -(*pd);
				} else {
	        		*pd = exp(pd[1] * log(*pd));
				}
				break;

			case MODULO:
				--pd;
				if ((int)(pd[1]) == 0)
					return(-1);
				*pd = (double)((int)(*pd) % (int)(pd[1]));
				break;

			case REL_OR:
				--pd;
				*pd = *pd || pd[1];
				break;

			case REL_AND:
				--pd;
				*pd = *pd && pd[1];
				break;

			case BIT_OR:
				/* force double values into integers and or them */
				--pd;
				*pd = (int)(pd[1]) | (int)(*pd);
				break;

			case BIT_AND:
				/* force double values into integers and and them */
				--pd;
				*pd = (int)(pd[1]) & (int)(*pd);
				break;

			case BIT_EXCL_OR:
				/* force double values to integers to exclusive or them */
				--pd;
				*pd = (int)(pd[1]) ^ (int)(*pd);
				break;

			case GR_OR_EQ:
				--pd;
				*pd = *pd >= pd[1];
				break;

			case GR_THAN:
				--pd;
				*pd = *pd > pd[1];
				break;

			case LESS_OR_EQ:
				--pd;
				*pd = *pd <= pd[1];
				break;

			case LESS_THAN:
				--pd;
				*pd = *pd < pd[1];
				break;

			case NOT_EQ:
				--pd;
				*pd = *pd != pd[1];
				break;

			case EQUAL:
				--pd;
				*pd = *pd == pd[1];
				break;

			case RIGHT_SHIFT:
				--pd;
				*pd = (int)(*pd) >> (int)(pd[1]);
				break;

			case LEFT_SHIFT:
				--pd;
				*pd = (int)(*pd) << (int)(pd[1]);
				break;

			case MAX_VAL:
				--pd;
				if (*pd < pd[1]) *pd = pd[1];
				break;
 
			case MIN_VAL:
				--pd;
				if (*pd > pd[1]) *pd = pd[1];
				break;

			case ACOS:
				*pd = acos(*pd);
				break;

			case ASIN:
				*pd = asin(*pd);
				break;

			case ATAN:
				*pd = atan(*pd);
				break;

	 		case ATAN2:
				--pd;
	 			*pd = atan2(pd[1], *pd);
	 			break;

			case COS:
				*pd = cos(*pd);
				break;

			case SIN:
				*pd = sin(*pd);
				break;

			case TAN:
				*pd = tan(*pd);
				break;

			case COSH:
				*pd = cosh(*pd);
				break;

			case SINH:
				*pd = sinh(*pd);
				break;

			case TANH:
				*pd = tanh(*pd);
				break;

			case CEIL:
				*pd = ceil(*pd);
				break;

			case FLOOR:
				*pd = floor(*pd);
				break;

			case NINT:
				d = *pd;
				*pd = (double)(long)(d >= 0 ? d+0.5 : d-0.5);
				break;

			case REL_NOT:
				*pd = (*pd ? 0 : 1);
				break;

			case BIT_NOT:
				*pd = ~(int)(*pd);
				break;

			case LITERAL:
				++pd;
				++post;
				if (post == NULL) {
					++post;
					printf("%.7s bad constant in expression\n",post);
					*pd = 0.;
					break;
				}
				memcpy((void *)&(*pd),post,8);
				post += 7;
				break;

			default:
				break;
			}

			/* move ahead in postfix expression */
			++post;
		}

		/* if everything is peachy,the stack should end at its first position */
		if (pd != topd) return(-1);
#if DEBUG > 1
/* check out floating rep of numbers like NaN, Inf */
{
union { unsigned char s[8]; double d; } u;
u.d = *pd;
printf("sCalcPerform: result: %g = (0x", *pd);
for (i=0; i<8; i++) printf("%2x ", u.s[i]);
printf(") \n");
}
#endif
		*presult = *pd;
		if (psresult && (lenSresult > 15)) {
			if (isnan(*pd))
				strcpy(psresult,"NaN");
			else
				(void)cvtDoubleToString(*pd, psresult, 8);
		}
	} else {

		/*** expression requires string operations ***/

		top = ps = &stack[10];
		ps--;  /* Expression handler assumes ps is pointing to a filled element */

		/* string expressions and values handled */
		while (*post != END_STACK) {

			switch (*post){

			case FETCH:
				INC(ps);
				++post;
				ps->s = NULL;
				ps->d = (*post < numArgs) ? parg[*post] : 0;
				break;

			case SFETCH:
				INC(ps);
				++post;
				if (*post < numSArgs) {
					/* fetch from string variable */
					ps->s = calloc(strlen(psarg[*post])+1, 1);
					strcpy(ps->s, psarg[*post]);
				} else {
					/* fetch from variable that caller did not supply */
					ps->s = calloc(1, 1);
					*(ps->s) = 0;
				}
				break;

			case STORE:
				/* not implemented */
				cleanup(top, ps);
				return(-1);

			case CONST_PI:
				INC(ps);
				ps->s = NULL;
				ps->d = PI;
				break;

			case CONST_D2R:
				INC(ps);
				ps->s = NULL;
				ps->d = PI/180.;
				break;

			case CONST_R2D:
				INC(ps);
				ps->s = NULL;
				ps->d = 180./PI;
				break;

			case CONST_S2R:
				INC(ps);
				ps->s = NULL;
				ps->d = PI/(180.*3600);
				break;

			case CONST_R2S:
				INC(ps);
				ps->s = NULL;
				ps->d = (180.*3600)/PI;
				break;

			case ADD:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d + ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d + ps1->d;
				} else {
					/* concatenate two strings */
					s = ps->s;
					ps->s = calloc(strlen(ps->s) + strlen(ps1->s) + 1, 1);
					strcpy(ps->s, s);
					strcat(ps->s, ps1->s);
					free(s);
					cleanStringElement(ps1);
				}
				break;

			case SUB:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d - ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d - ps1->d;
				} else {
					/* subtract ps1->s from ps->s */
					s = strstr(ps->s, ps1->s);
					s1 = ps->s;
					s2 = ps1->s;
					if (s && (strlen(s2) <= (strlen(s1) - (s - s1)))) {
						for (s1=s+strlen(s2); *s1; s++, s1++) *s = *s1;
						*s = '\0';
					}
					cleanStringElement(ps1);
				}
				break;

			case MULT:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d * ps1->d;
				break;

			case DIV:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				if (ps1->d == 0) /* can't divide by zero */
					return(-1);
				ps->d = ps->d / ps1->d;
				break;

			case COND_IF:
				/* if false condition then skip true expression */
				toDouble(ps);
				if (ps->d == 0.0) {
					/* skip to matching COND_ELSE */
					for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
						switch(post[1]) {
						case LITERAL:	post+=8; break;
						case SLITERAL:	post++; while (post[1]) post++; break;
						case COND_IF:	got_if++; break;
						case COND_ELSE: got_if--; break;
						case FETCH: case SFETCH: post++; break;
						}
					}
				}
				/* remove condition from stack top */
				DEC(ps);
				break;
				
			case COND_ELSE:
				/* result, true condition is on stack so skip false condition  */
				/* skip to matching COND_END */
				for (got_if=1; got_if>0 && *(post+1) != END_STACK; ++post) {
					switch(post[1]) {
					case LITERAL:	post+=8; break;
					case SLITERAL:	post++; while (post[1]) post++; break;
					case COND_IF:	got_if++; break;
					case COND_END:	got_if--; break;
					case FETCH: case SFETCH: post++; break;
					}
				}
				break;

			case COND_END:
				break;

			case ABS_VAL:
				toDouble(ps);
				if (ps->d < 0 ) ps->d *= -1;
				break;

			case UNARY_NEG:
				toDouble(ps);
				ps->d *= -1;
				break;

			case SQU_RT:
				toDouble(ps);
				/* check for neg number */
				if (ps->d < 0) {cleanup(top, ps); return(-1);}			
				ps->d = sqrt(ps->d);
				break;

			case EXP:
				toDouble(ps);
				ps->d = exp(ps->d);
				break;

			case LOG_10:
				toDouble(ps);
				/* check for neg number */
				if (ps->d < 0) {cleanup(top, ps); return(-1);}
				ps->d = log10(ps->d);
				break;

			case LOG_E:
				toDouble(ps);
				/* check for neg number */
				if (ps->d < 0) {cleanup(top, ps); return(-1);}
				ps->d = log(ps->d);
				break;

			case RANDOM:
				INC(ps);
				ps->d = local_random();
				ps->s = NULL;
				break;

			case EXPON:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				if (ps->d == 0) break;
				if (ps->d < 0) {
					i = (int) ps1->d;
					/* is exponent an integer? */
					if ((ps1->d - (double)i) != 0) return (-1);
        			ps->d = exp(ps1->d * log(-(ps->d)));
					/* is value negative */
					if ((i % 2) > 0) ps->d = -ps->d;
				} else {
					ps->d = exp(ps1->d * log(ps->d));
				}
				break;

			case MODULO:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				if ((int)ps1->d == 0)
					return(-1);
				ps->d = (double)((int)ps->d % (int)ps1->d);
				break;

			case REL_OR:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d || ps1->d;
				break;

			case REL_AND:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = ps->d && ps1->d;
				break;

			case BIT_OR:
				/* force double values into integers and or them */
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps1->d) | (int)(ps->d);
				break;

			case BIT_AND:
				/* force double values into integers and and them */
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps1->d) & (int)(ps->d);
				break;

			case BIT_EXCL_OR:
				/* force double values to integers to exclusive or them */
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps1->d) ^ (int)(ps->d);
				break;

			case GR_OR_EQ:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d >= ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d >= ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) >= 0);
					free(ps->s);
					ps->s = NULL;
					cleanStringElement(ps1);
				}
				break;

			case GR_THAN:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d > ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d > ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) > 0);
					free(ps->s);
					ps->s = NULL;
					cleanStringElement(ps1);
				}
				break;

			case LESS_OR_EQ:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d <= ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d <= ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) <= 0);
					free(ps->s);
					ps->s = NULL;
					cleanStringElement(ps1);
				}
				break;

			case LESS_THAN:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d < ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d < ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) < 0);
					free(ps->s);
					ps->s = NULL;
					cleanStringElement(ps1);
				}
				break;

			case NOT_EQ:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d != ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d != ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) != 0);
					free(ps->s);
					ps->s = NULL;
					cleanStringElement(ps1);
				}
				break;

			case EQUAL:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					ps->d = ps->d == ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					ps->d = ps->d == ps1->d;
				} else if ((isString(ps)) && (isString(ps1))) {
					/* compare ps->s to ps1->s */
					ps->d = (double)(strcmp(ps->s, ps1->s) == 0);
					free(ps->s);
					ps->s = NULL;
					cleanStringElement(ps1);
				}
				break;

			case RIGHT_SHIFT:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps->d) >> (int)(ps1->d);
				break;

			case LEFT_SHIFT:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
				ps->d = (int)(ps->d) << (int)(ps1->d);
				break;

			case MAX_VAL:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					if (ps->d < ps1->d) ps->d = ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					if (ps->d < ps1->d) ps->d = ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					if (strcmp(ps->s, ps1->s) < 0) {
						s = ps->s;
						ps->s = ps1->s;
						ps1->s = s;
					}
					cleanStringElement(ps1);
				}
				break;
 
			case MIN_VAL:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps)) {
					toDouble(ps1);
					if (ps->d > ps1->d) ps->d = ps1->d;
				} else if (isDouble(ps1)) {
					to_double(ps);
					if (ps->d > ps1->d) ps->d = ps1->d;
				} else {
					/* compare ps->s to ps1->s */
					if (strcmp(ps->s, ps1->s) > 0) {
						s = ps->s;
						ps->s = ps1->s;
						ps1->s = s;
					}
					cleanStringElement(ps1);
				}
				break;

			case ACOS:
				toDouble(ps);
				ps->d = acos(ps->d);
				break;

			case ASIN:
				toDouble(ps);
				ps->d = asin(ps->d);
				break;

			case ATAN:
				toDouble(ps);
				ps->d = atan(ps->d);
				break;

	 		case ATAN2:
				ps1 = ps;
				DEC(ps);
				toDouble(ps1);
				toDouble(ps);
	 			ps->d = atan2(ps1->d, ps->d);
	 			break;

			case COS:
				toDouble(ps);
				ps->d = cos(ps->d);
				break;

			case SIN:
				toDouble(ps);
				ps->d = sin(ps->d);
				break;

			case TAN:
				toDouble(ps);
				ps->d = tan(ps->d);
				break;

			case COSH:
				toDouble(ps);
				ps->d = cosh(ps->d);
				break;

			case SINH:
				toDouble(ps);
				ps->d = sinh(ps->d);
				break;

			case TANH:
				toDouble(ps);
				ps->d = tanh(ps->d);
				break;

			case CEIL:
				toDouble(ps);
				ps->d = ceil(ps->d);
				break;

			case FLOOR:
				toDouble(ps);
				ps->d = floor(ps->d);
				break;

			case NINT:
				if (isDouble(ps)) {
					d = ps->d;
					ps->d = (double)(long)(d >= 0 ? d+0.5 : d-0.5);
				} else {
					/* hunt down integer and convert */
					s = strpbrk(ps->s,"0123456789");
					if ((s > ps->s) && (s[-1] == '.')) s--;
					if ((s > ps->s) && (s[-1] == '-')) s--;
					d = s ? atof(s) : 0.0;
					free(ps->s); ps->s = NULL;
					ps->d = (double)(long)(d >= 0 ? d+0.5 : d-0.5);
				}
				break;

			case REL_NOT:
				toDouble(ps);
				ps->d = (ps->d ? 0 : 1);
				break;

			case BIT_NOT:
				toDouble(ps);
				ps->d = ~(int)(ps->d);
				break;

			case LITERAL:
				INC(ps);
				++post;
				if (post == NULL) {
					++post;
					printf("%.7s bad constant in expression\n",post);
					ps->s = NULL;
					ps->d = 0.;
					break;
				}
				memcpy((void *)&(ps->d),post,8);
				ps->s = NULL;
				post += 7;
				break;

			case SLITERAL:
				INC(ps);
				++post;
				if (post == NULL) {
					++post;
					printf("%.7s bad constant in expression\n",post);
					ps->s = NULL;
					ps->d = 0.;
					break;
				}
				ps->s = calloc(strlen(post)+1, 1);
				strcpy(ps->s, post);
				/* skip to end of string */
				while (*post) post++;
				break;

			case TO_DOUBLE:
				if (isString(ps)) {
					/* hunt down number and convert */
					s = strpbrk(ps->s,"0123456789");
					if ((s > ps->s) && (s[-1] == '.')) s--;
					if ((s > ps->s) && (s[-1] == '-')) s--;
					ps->d = s ? atof(s) : 0.0;
					free(ps->s); ps->s = NULL;
				}
				break;

			case TO_STRING:
				toString(ps);
				break;

	 		case PRINTF:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps))
					{cleanup(top, ps1); return(-1);}
				s = ps->s;
				while ((s1 = strstr(s, "%%"))) {s = s1+2;}
				if (((s = strpbrk(s, "%")) == NULL) ||
					((s = strpbrk(s+1, "*cdeEfgGiousxX")) == NULL)) {
					/* no printf arguments needed */
		 			sprintf(tmpstr, ps->s);
				} else {
					switch (*s) {
					default: case '*':
						cleanup(top, ps1);
						return(-1);
					case 'c': case 'd': case 'i': case 'o':
					case 'u': case 'x': case 'X':
						toDouble(ps1);
						l = myNINT(ps1->d);
	 					sprintf(tmpstr, ps->s, l);
						break;
					case 'e': case 'E': case 'f': case 'g': case 'G':
						toDouble(ps1);
	 					sprintf(tmpstr, ps->s, ps1->d);
						break;
					case 's':
						toString(ps1);
	 					sprintf(tmpstr, ps->s, ps1->s);
						break;
					}
				}
				if (strlen(ps->s) < strlen(tmpstr)) {
					free(ps->s);
					ps->s = calloc(strlen(tmpstr)+1, 1);
				}
				strcpy(ps->s, tmpstr);
	 			cleanStackElement(ps1);
				break;

	 		case SSCANF:
				ps1 = ps;
				DEC(ps);
				if (isDouble(ps) || isDouble(ps1))
					{cleanup(top, ps1); return(-1);}
				s = findConversionIndicator(ps1->s);
				if (s == NULL) {cleanup(top, ps1); return(-1);}
				switch (*s) {
				default: case 'p': case 'w': case 'n': case '$':
					cleanup(top, ps1);
					return(-1);
				case 'd': case 'i':
					if (s[-1] == 'h') {
		 				sscanf(ps->s, ps1->s, &h);
						ps->d = (double)h;
					} else {
		 				sscanf(ps->s, ps1->s, &l);
						ps->d = (double)l;
					}
					cleanStringElement(ps);
					ps->s = NULL;
					break;
				case 'o': case 'u': case 'x': case 'X':
					if (s[-1] == 'h') {
		 				sscanf(ps->s, ps1->s, &ui);
						ps->d = (double)ui;
					} else {
		 				sscanf(ps->s, ps1->s, &ul);
						ps->d = (double)ul;
					}
					cleanStringElement(ps);
					ps->s = NULL;
					break;
				case 'e': case 'E': case 'f': case 'g': case 'G':
					if (s[-1] == 'l') {
		 				sscanf(ps->s, ps1->s, &(ps->d));
					} else {
		 				sscanf(ps->s, ps1->s, &f);
						ps->d = (double)f;
					}
					cleanStringElement(ps);
					ps->s = NULL;
					break;
				case 'c': case '[': case 's':
		 			sscanf(ps->s, ps1->s, tmpstr);
					if (strlen(ps->s) < strlen(tmpstr)) {
						free(ps->s);
						ps->s = calloc(strlen(tmpstr)+1, 1);
					}
					strcpy(ps->s, tmpstr);
					break;
				}
	 			cleanStringElement(ps1);
				break;

			case SUBRANGE:
				ps2 = ps;
				DEC(ps);
				ps1 = ps;
				DEC(ps);
				toString(ps);
				k = strlen(ps->s);
				if (isDouble(ps1)) {
					i = (int)ps1->d;
					if (i < 0) i += k;
				} else {
					s = strstr(ps->s, ps1->s);
					i = s ? (s - ps->s) + strlen(ps1->s) : 0;
				}
				if (isDouble(ps2)) {
					j = (int)ps2->d;
					if (j < 0) j += k;
				} else {
					if (*(ps2->s)) {
						s = strstr(ps->s, ps2->s);
						j = s ? (s - ps->s) - 1 : k;
					} else {
						j = k;
					}
				}
				i = MAX(MIN(i,k),0);
				j = MIN(j,k);
				for (s=ps->s, s1=s+i, s2=s+j ; *s1 && s1 <= s2; )
					{*s++ = *s1++;}
				*s = 0;
				break;
 
			case REPLACE:
				ps2 = ps;
				DEC(ps);
				ps1 = ps;
				DEC(ps);
				toString(ps);
				toString(ps1);
				toString(ps2);
				i = strlen(ps->s);
				j = strlen(ps1->s);
				k = strlen(ps2->s);
				s1 = strstr(ps->s, ps1->s);
				s2 = ps2->s;
				if (s1 >= ps->s) {
					char *s_old, *s_dest;
					s_old = s = ps->s;
					if (k > j) ps->s = malloc(i - j + k + 1);
					s_dest = ps->s;
					while (s < s1) *s_dest++ = *s++;
					s += j;
					while (*s2) *s_dest++ = *s2++;
					while (*s) *s_dest++ = *s++;
					*s_dest = '\0';
					if (k > j) free(s_old);
				}
				cleanStringElement(ps1);
				cleanStringElement(ps2);
				break;
 
			default:
				break;
			}

			/* move ahead in postfix expression */
			++post;
		}

		/* if everything is peachy,the stack should end at its first position */
		if (ps != top)
			return(-1);
	
		if (isDouble(ps)) {
			if (presult) *presult = ps->d;
			if (psresult) {
				toString(ps);
				for (i=0, s=ps->s, s1=psresult; *s && i<lenSresult; i++, *s1++ = *s++);
				if (i<lenSresult)
					*s1 = 0;
				else
					*(--s1) = 0;
				/* strncpy(psresult, ps->s, lenSresult); */
				psresult[lenSresult-1] = 0;
				cleanStringElement(ps);
			}
		} else {
			if (psresult) {
				for (i=0, s=ps->s, s1=psresult; *s && i<lenSresult; i++)
					*s1++ = *s++;
				if (i < lenSresult)
					*s1 = 0;
				else
					*(--s1) = 0;
				psresult[lenSresult-1] = 0;
			}
			if (presult) {
				toDouble(ps);
				*presult = ps->d;
			} else {
				cleanStringElement(ps);
			}
		}

	} /* if (*post++ != USES_STRING) {} else */

	return(0);
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
