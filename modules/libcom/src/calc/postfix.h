/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* postfix.h
 *      Original Author: Bob Dalesio
 *      Date:            9-21-88
 */

#ifndef INCpostfixh
#define INCpostfixh

#include "shareLib.h"

#define CALCPERFORM_NARGS 12
#define CALCPERFORM_STACK 80

#define INFIX_TO_POSTFIX_SIZE(n) ((n)*21/6)
/* The above expression is an estimate of the maximum postfix buffer
 * size needed for a given infix expression buffer (the argument must count
 * the trailing nil byte in the input expression string). The actual size
 * needed is never larger than this value, although it is actually a
 * few bytes smaller for some sizes.
 *
 * The maximum expansion from infix to postfix is for the sub-expression
 *    .1?.1:
 * which is 6 characters long and results in 21 bytes of postfix:
 *    .1 => LITERAL_DOUBLE + 8 byte value
 *    ?  => COND_IF
 *    .1 => LITERAL_DOUBLE + 8 byte value
 *    :  => COND_ELSE
 *    ...
 *       => COND_END
 * For other short expressions the factor 21/6 always gives a big enough
 * postfix buffer (proven by hand, look at '1+' and '.1+' as well).
 */

/* These are not hard limits, just default sizes for the database */
#define MAX_INFIX_SIZE 100
#define MAX_POSTFIX_SIZE INFIX_TO_POSTFIX_SIZE(MAX_INFIX_SIZE)


/* Error numbers from postfix */

#define CALC_ERR_NONE            0 /* No error */
#define CALC_ERR_TOOMANY         1 /* Too many results returned */
#define CALC_ERR_BAD_LITERAL     2 /* Bad numeric literal */
#define CALC_ERR_BAD_ASSIGNMENT  3 /* Bad assignment target */
#define CALC_ERR_BAD_SEPERATOR   4 /* Comma without parentheses */
#define CALC_ERR_PAREN_NOT_OPEN  5 /* Close parenthesis without open */
#define CALC_ERR_PAREN_OPEN      6 /* Open parenthesis at end of expression */
#define CALC_ERR_CONDITIONAL     7 /* Unbalanced conditional ?: operators */
#define CALC_ERR_INCOMPLETE      8 /* Incomplete expression, operand missing */
#define CALC_ERR_UNDERFLOW       9 /* Runtime stack would underflow */
#define CALC_ERR_OVERFLOW       10 /* Runtime stack would overflow */
#define CALC_ERR_SYNTAX         11 /* Syntax error */
#define CALC_ERR_NULL_ARG       12 /* NULL or empty input argument */
#define CALC_ERR_INTERNAL       13 /* Internal error, bad element type */
/* Changes in the above errors must also be made in calcErrorStr() */


#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc long
    postfix(const char *pinfix, char *ppostfix, short *perror);

epicsShareFunc long
    calcPerform(double *parg, double *presult, const char *ppostfix);

epicsShareFunc long
    calcArgUsage(const char *ppostfix, unsigned long *pinputs, unsigned long *pstores);

epicsShareFunc const char *
    calcErrorStr(short error);

epicsShareFunc void
    calcExprDump(const char *pinst);

#ifdef __cplusplus
}
#endif

#endif /* INCpostfixh */
