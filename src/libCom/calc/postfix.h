/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* postfix.h
 *      Author:          Bob Dalesio
 *      Date:            9-21-88
 */

#ifndef INCpostfixh
#define INCpostfixh

#include "shareLib.h"
#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc long epicsShareAPI 
	postfix (const char *pinfix, char *ppostfix, short *perror);

epicsShareFunc long epicsShareAPI 
	calcPerform(double *parg, double *presult, char  *post);

#ifdef __cplusplus
}
#endif

#endif /* INCpostfixh */
