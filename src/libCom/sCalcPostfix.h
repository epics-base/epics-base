/* sCalcPostfix.h
 *      Author:          Bob Dalesio
 *      Date:            9-21-88
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
 * .01  03-18-98 tmm derived from postfix.h
 * 
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

