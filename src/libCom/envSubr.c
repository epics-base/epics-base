/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	07-20-91
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
 * .01	07-20-91	rac	initial version
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	envSubr.c - routines to get and set EPICS environment parameters
*
* DESCRIPTION
*	These routines are oriented for use with EPICS environment
*	parameters under UNIX and VxWorks.  They may be used for other
*	purposes, as well.
*
* QUICK REFERENCE
* #include <envDefs.h>
* ENV_PARAM	param;
*  char *envGetConfigParam(   pParam,    bufDim,    pBuf                  )
*  long  envPrtConfigParam(   pParam                                      )
*  long  envSetConfigParam(   pParam,    valueString                      )
*
* SEE ALSO
*	$epics/share/bin/envSetupParams, envSubr.h
*
*-***************************************************************************/

#ifdef vxWorks
#   include <vxWorks.h>
#   include <stdioLib.h>
#else
#   include <stdio.h>
#   include <stdlib.h>
#endif

#define ENV_PRIVATE_DATA
#include <envDefs.h>


/*+/subr**********************************************************************
* NAME	envGetConfigParam - get value of a configuration parameter
*
* DESCRIPTION
*	Gets the value of a configuration parameter and copies it
*	into the caller's buffer.  If the configuration parameter
*	isn't found in the environment, then the default value for
*	the parameter is copied.  If no parameter is found and there
*	is no default, then '\0' is copied and NULL is returned.
*
* RETURNS
*	pointer to callers buffer, or
*	NULL if no parameter value or default value was found
*
* EXAMPLES
* 1.	Get the value for the EPICS-defined environment parameter
*	EPICS_TS_MIN_WEST.
*
*	#include <envDefs.h>
*	char       temp[80];
*
*	printf("minutes west of UTC is: %s\n",
*			envGetConfigParam(&EPICS_TS_MIN_WEST, 80, temp));
*
* 2.	Get the value for the DISPLAY environment parameter under UNIX.
*
*	#include <envDefs.h>
*	char       temp[80];
*	ENV_PARAM  display={"DISPLAY",""}
*
*	if (envGetConfigParam(&display, 80, temp) == NULL)
*	    printf("DISPLAY isn't defined\n");
*	else
*	    printf("DISPLAY is %s\n", temp);
*
*-*/
char *
envGetConfigParam(pParam, bufDim, pBuf)
ENV_PARAM *pParam;	/* I pointer to config param structure */
int	bufDim;		/* I dimension of parameter buffer */
char	*pBuf;		/* I pointer to parameter buffer  */
{
    char	*pEnv;		/* pointer to environment string */
    long	i;

#ifndef vxWorks
    pEnv = getenv(pParam->name);
#else
    pEnv = NULL;
#endif
    if (pEnv == NULL)
	pEnv = pParam->dflt;
    if (strlen(pEnv) <= 0)
	return NULL;
    if ((i = strlen(pEnv)) < bufDim)
	strcpy(pBuf, pEnv);
    else {
	strncpy(pBuf, pEnv, bufDim-1);
	pBuf[bufDim-1] = '\0';
    }
    return pBuf;
}

/*+/subr**********************************************************************
* NAME	envPrtConfigParam - print value of a configuration parameter
*
* DESCRIPTION
*	Prints the value of a configuration parameter.
*
* RETURNS
*	0
*
* EXAMPLE
* 1.	Print the value for the EPICS-defined environment parameter
*	EPICS_TS_MIN_WEST.
*
*	#include <envDefs.h>
*
*	envPrtConfigParam(&EPICS_TS_MIN_WEST);
*
*-*/
long
envPrtConfigParam(pParam)
ENV_PARAM *pParam;	/* I pointer to config param structure */
{
    char	text[80];
    if (envGetConfigParam(pParam, 80, text) == NULL)
	printf("%s is undefined\n", pParam->name);
    else
	printf("%s: %s\n", pParam->name, text);
    return 0;
}

/*+/subr**********************************************************************
* NAME	envSetConfigParam - set value of a configuration parameter
*
* DESCRIPTION
*	Sets the value of a configuration parameter.
*
* RETURNS
*	0
*
* NOTES
* 1.	Performs a useful function only under VxWorks.
*
* EXAMPLE
* 1.	Set the value for the EPICS-defined environment parameter
*	EPICS_TS_MIN_WEST to 360, for USA central time zone.
*
*	Under UNIX:
*
*		% setenv EPICS_TS_MIN_WEST 360
*
*	In a program running under VxWorks:
*
*		#include <envDefs.h>
*
*		envSetConfigParam(&EPICS_TS_MIN_WEST, "360");
*
*	Under the VxWorks command shell:
*
*		envSetConfigParam &EPICS_TS_MIN_WEST,"360"
*
*-*/
long
envSetConfigParam(pParam, value)
ENV_PARAM *pParam;	/* I pointer to config param structure */
char	*value;		/* I pointer to value string */
{
#ifndef vxWorks
    printf("envSetConfigParam can't be used in UNIX\n");
#else
    if (strlen(value) < 80)
	strcpy(pParam->dflt, value);
    else {
	strncpy(pParam->dflt, value, 79);
	pParam->dflt[79] = '\0';
    }
#endif
    return 0;
}
