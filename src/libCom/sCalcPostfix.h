/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* sCalcPostfix.h
 *      Author:          Bob Dalesio
 *      Date:            9-21-88
 */

#ifndef INCsCalcPostfixH
#define INCsCalcPostfixH

#include "shareLib.h"

#define		BAD_EXPRESSION	0
#define		END_STACK	127

epicsShareFunc long epicsShareAPI sCalcPostfix (char *pinfix, 
        char **pp_postfix, short *perror);

epicsShareFunc long epicsShareAPI 
	sCalcPerform (double *parg, int numArgs, 
    char **psarg, int numSArgs, double *presult, 
    char *psresult, int lenSresult, char *post);

#endif /* INCsCalcPostfixH */

