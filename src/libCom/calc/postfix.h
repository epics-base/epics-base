/* postfix.h
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
 * .01  01-11-89        lrd     add right and left shift
 * .02  02-01-89        lrd     add trig functions
 * .03  02-17-92        jba     add exp, CEIL, and FLOOR
 * .04  03-03-92        jba     added MAX, MIN, and comma
 * .05  03-06-92        jba     added multiple conditional expressions ?
 * .06  04-02-92        jba     added CONSTANT for floating pt constants in expression
 * .07  05-11-94        jba     added CONST_PI, CONST_D2R, and CONST_R2D
 */

#ifndef INCpostfixh
#define INCpostfixh

#include <shareLib.h>

epicsShareFunc long epicsShareAPI 
	postfix (char *pinfix, char *ppostfix, short *perror);

epicsShareFunc long epicsShareAPI 
	calcPerform(double *parg, double *presult, char  *post);


#endif /* INCpostfixh */
