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
 * .02	08-07-91	joh	added config get for long and double C types
 * .03	08-07-91	joh	added config get for struct in_addr type
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
*  char *envGetConfigParam(          pParam,    bufDim,    pBuf                 )
*  int	 envGetIntegerConfigParam(   pParam,    pLong    			)
*  int	 envGetRealConfigParam(      pParam,    pDouble    			)
*  int	 envGetInetAddrConfigParam(  pParam,    pAddr    			)
*  long  envPrtConfigParam(          pParam                      		)
*  long  envSetConfigParam(          pParam,    valueString                     )
*
* SEE ALSO
*	$epics/share/bin/envSetupParams, envSubr.h
*
*-***************************************************************************/

#ifdef vxWorks
#   include <vxWorks.h>
#   include <stdioLib.h>
#   include <in.h>
#   include <types.h>
#else
#   include <stdio.h>
#   include <stdlib.h>
#   include <sys/types.h>
#   include <netinet/in.h>
#endif

#define ENV_PRIVATE_DATA
#include <envDefs.h>

#ifndef ERROR
#define ERROR 	(-1)
#endif
#ifndef OK
#define OK	1
#endif


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
*
* NAME	envGetIntegerConfigParam - get value of an integer configuration parameter
*
* Author:	Jeffrey O. Hill
* Date:		080791
*
* DESCRIPTION
*	Gets the value of a configuration parameter and copies it
*	into the caller's integer (long) buffer.  If the configuration 
*	parameter isn't found in the environment, then the default value for
*	the parameter is copied.  
*	If no parameter is found and there is no default, then ERROR is 
*	returned and the callers buffer is unmodified.
*
* RETURNS
*	OK if the parameter is found and it is an integer or ERROR
*
* EXAMPLES
* 1.	Get the value for the integer environment parameter EPICS_NUMBER_OF_ITEMS.
*
*	#include <envDefs.h>
*	long		count;
*	int		status;
*
*	status = envGetIntegerConfigParam(&EPICS_NUMBER_OF_ITEMS, &count);
*	if(status == OK){
*		printf("and the count is: %d\n", count);
*	}
*	else{
*		printf("%s could not be found or was not an integer\n"
*			EPICS_NUMBER_OF_ITEMS.name);
*	}
*			
*
*
*-*/
int
envGetIntegerConfigParam(pparam, pretval)
ENV_PARAM	*pparam;
long		*pretval;
{
	char	text[128];
	char	*ptext;
	int	count;

	ptext = envGetConfigParam(pparam, sizeof text, text);
	if(ptext){
		count = sscanf(text, "%ld", pretval);
		if(count == 1){
			return OK;
		}
	}
	return ERROR;
}

/*+/subr**********************************************************************
*
* NAME	envGetRealConfigParam - get value of an real configuration parameter
*
* Author:	Jeffrey O. Hill
* Date:		080791
*
* DESCRIPTION
*	Gets the value of a configuration parameter and copies it
*	into the caller's real (double) buffer.  If the configuration parameter
*	isn't found in the environment, then the default value for
*	the parameter is copied.  
*	If no parameter is found and there is no default, then ERROR is 
*	returned and the callers buffer is unmodified.
*
* RETURNS
*	OK if the parameter is found and it is a real number or ERROR
*
* EXAMPLES
* 1.	Get the value for the real environment parameter EPICS_THRESHOLD.
*
*	#include <envDefs.h>
*	double		threshold;
*	int		status;
*
*	status = envGetIntegerConfigParam(&EPICS_THRESHOLD, &threshold);
*	if(status == OK){
*		printf("and the threshold is: %lf\n", threshold);
*	}
*	else{
*		printf("%s could not be found or was not a real number\n"
*			EPICS_THRESHOLD.name);
*	}
*			
*
*
*-*/
int
envGetRealConfigParam(pparam, pretval)
ENV_PARAM	*pparam;
double		*pretval;
{
	char	text[128];
	char	*ptext;
	int	count;

	ptext = envGetConfigParam(pparam, sizeof text, text);
	if(ptext){
		count = sscanf(text, "%lf", pretval);
		if(count == 1){
			return OK;
		}
	}
	return ERROR;
}

/*+/subr**********************************************************************
*
* NAME	envGetInetAddrConfigParam - get value of an inet addr configuration parameter
*
* Author:	Jeffrey O. Hill
* Date:		080791
*
* DESCRIPTION
*	Gets the value of a configuration parameter and copies it
*	into the caller's (struct in_addr) buffer.  If the configuration parameter
*	isn't found in the environment, then the default value for
*	the parameter is copied.  
*	If no parameter is found and there is no default, then ERROR is 
*	returned and the callers buffer is unmodified.
*
* RETURNS
*	OK if the parameter is found and it is an inet address or ERROR
*
* EXAMPLES
* 1.	Get the value for the inet address environment parameter EPICS_INET.
*
*	#include <envDefs.h>
*	struct	in_addr	addr
*	int		status;
*
*	status = envGetInetAddrConfigParam(&EPICS_INET, &addr);
*	if(status == OK){
*		printf("and the threshold is: %x\n", addr.s_addr);
*	}
*	else{
*		printf("%s could not be found or was not an inet address\n"
*			EPICS_INET.name);
*	}
*			
*
*
*-*/
int
envGetInetAddrConfigParam(pparam, paddr)
ENV_PARAM	*pparam;
struct in_addr	*paddr;
{
	char	text[128];
	char	*ptext;
	long	status;

	ptext = envGetConfigParam(pparam, sizeof text, text);
	if(ptext){
		status = inet_addr(text);
		if(status != ERROR){
			paddr->s_addr = status;
			return OK;
		}
	}
	return ERROR;
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
