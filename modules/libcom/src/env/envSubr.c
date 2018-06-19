/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	Author:	Roger A. Cole
 *	Date:	07-20-91
 */
/*+/mod***********************************************************************
* TITLE	envSubr.c - routines to get and set EPICS environment parameters
*
* DESCRIPTION
*	These routines are oriented for use with EPICS environment
*	parameters under UNIX and VxWorks.  They may be used for other
*	purposes, as well.
*
*	Many EPICS environment parameters are predefined in envDefs.h.
*
* QUICK REFERENCE
* #include "envDefs.h"
* ENV_PARAM	param;
*  char *envGetConfigParamPtr(       pParam			      )
*  char *envGetConfigParam(          pParam,    bufDim,    pBuf       )
*  long  envGetLongConfigParam(      pParam,    pLong                 )
*  long  envGetDoubleConfigParam(    pParam,    pDouble               )
*  long  envGetInetAddrConfigParam(  pParam,    pAddr                 )
*  long  envPrtConfigParam(          pParam                           )
*
* SEE ALSO
*	$epics/share/bin/envSetupParams, envDefs.h
*
*-***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "epicsStdlib.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "errMdef.h"
#include "errlog.h"
#include "envDefs.h"
#include "epicsAssert.h"
#include "osiSock.h"


/*+/subr**********************************************************************
* NAME	envGetConfigParamPtr - returns a pointer to the configuration 
* 		parameter value string
*
* DESCRIPTION
*	Returns a pointer to a configuration parameter value. 
*	If the configuration parameter isn't found in the environment, 
*	then a pointer to the default value for the parameter is copied.  
* 	If no parameter is found and there is no default, then 
* 	NULL is returned.
*
* RETURNS
*	pointer to the environment variable value string, or
*	NULL if no parameter value and no default value was found
*
* EXAMPLES
* 1.	Get the value for the EPICS-defined environment parameter
*	EPICS_TS_NTP_INET.
*
*	#include "envDefs.h"
*	const char *pStr;
*
*	pStr = envGetConfigParamPtr(&EPICS_TS_NTP_INET);
*	if (pStr) {
*		printf("NTP time server is: %s\n", pStr);
*	}
*
*-*/
const char * epicsShareAPI envGetConfigParamPtr(
const ENV_PARAM *pParam /* I pointer to config param structure */
)
{
    const char *pEnv;		/* pointer to environment string */

    pEnv = getenv(pParam->name);

    if (pEnv == NULL) {
	pEnv = pParam->pdflt;
    }

    if (pEnv) {
	if (pEnv[0u] == '\0') {
	    pEnv = NULL;
	}
    }

    return pEnv;
}


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
*	EPICS_TS_NTP_INET.
*
*	#include "envDefs.h"
*	char       temp[80];
*
*	printf("NTP time server is: %s\n",
*			envGetConfigParam(&EPICS_TS_NTP_INET, sizeof(temp), temp));
*
* 2.	Get the value for the DISPLAY environment parameter under UNIX.
*
*	#include "envDefs.h"
*	char       temp[80];
*	ENV_PARAM  display={"DISPLAY",""}
*
*	if (envGetConfigParam(&display, sizeof(temp), temp) == NULL)
*	    printf("DISPLAY isn't defined\n");
*	else
*	    printf("DISPLAY is %s\n", temp);
*
*-*/
char * epicsShareAPI envGetConfigParam(
const ENV_PARAM *pParam,/* I pointer to config param structure */
int	bufDim,		/* I dimension of parameter buffer */
char	*pBuf		/* I pointer to parameter buffer  */
)
{
    const char *pEnv;		/* pointer to environment string */

    pEnv = envGetConfigParamPtr(pParam);
    if (!pEnv) {
	return NULL;
    }

    strncpy(pBuf, pEnv, bufDim-1);
    pBuf[bufDim-1] = '\0';

    return pBuf;
}

/*+/subr**********************************************************************
* NAME	envGetDoubleConfigParam - get value of a double configuration parameter
*
* DESCRIPTION
*	Gets the value of a configuration parameter and copies it into the
*	caller's real (double) buffer.  If the configuration parameter isn't
*	found in the environment, then the default value for the parameter
*	is copied.  
*
*	If no parameter is found and there is no default, then -1 is 
*	returned and the caller's buffer is unmodified.
*
* RETURNS
*	0, or
*	-1 if an error is encountered
*
* EXAMPLE
* 1.	Get the value for the real environment parameter EPICS_THRESHOLD.
*
*	#include "envDefs.h"
*	double	threshold;
*	long	status;
*
*	status = envGetDoubleConfigParam(&EPICS_THRESHOLD, &threshold);
*	if (status == 0) {
*	    printf("the threshold is: %lf\n", threshold);
*	}
*	else {
*	    printf("%s could not be found or was not a real number\n",
*			EPICS_THRESHOLD.name);
*	}
*
*-*/
long epicsShareAPI envGetDoubleConfigParam(
const ENV_PARAM *pParam,/* I pointer to config param structure */
double	*pDouble	/* O pointer to place to store value */
)
{
    char	text[128];
    char	*ptext;
    int		count;

    ptext = envGetConfigParam(pParam, sizeof text, text);
    if (ptext != NULL) {
	count = epicsScanDouble(text, pDouble);
	if (count == 1) {
	    return 0;
	}
        (void)fprintf(stderr,"Unable to find a real number in %s=%s\n", 
		pParam->name, text);
    }

    return -1;
}

/*+/subr**********************************************************************
* NAME	envGetInetAddrConfigParam - get value of an inet addr config parameter
*
* DESCRIPTION
*	Gets the value of a configuration parameter and copies it into
*	the caller's (struct in_addr) buffer.  If the configuration parameter
*	isn't found in the environment, then the default value for
*	the parameter is copied.  
*
*	If no parameter is found and there is no default, then -1 is 
*	returned and the callers buffer is unmodified.
*
* RETURNS
*	0, or
*	-1 if an error is encountered
*
* EXAMPLE
* 1.	Get the value for the inet address environment parameter EPICS_INET.
*
*	#include "envDefs.h"
*	struct	in_addr	addr;
*	long	status;
*
*	status = envGetInetAddrConfigParam(&EPICS_INET, &addr);
*	if (status == 0) {
*	    printf("the s_addr is: %x\n", addr.s_addr);
*	}
*	else {
*	    printf("%s could not be found or was not an inet address\n",
*			EPICS_INET.name);
*	}
*
*-*/
long epicsShareAPI envGetInetAddrConfigParam(
const ENV_PARAM *pParam,/* I pointer to config param structure */
struct in_addr *pAddr	/* O pointer to struct to receive inet addr */
)
{
    char	text[128];
    char	*ptext;
    long	status;
	struct sockaddr_in sin;

    ptext = envGetConfigParam(pParam, sizeof text, text);
    if (ptext) {
		status = aToIPAddr (text, 0u, &sin);
		if (status == 0) {
			*pAddr = sin.sin_addr;
			return 0;
		}
        (void)fprintf(stderr,"Unable to find an IP address or valid host name in %s=%s\n", 
						pParam->name, text);
    }
    return -1;
}

/*+/subr**********************************************************************
* NAME	envGetLongConfigParam - get value of an integer config parameter
*
* DESCRIPTION
*	Gets the value of a configuration parameter and copies it
*	into the caller's integer (long) buffer.  If the configuration 
*	parameter isn't found in the environment, then the default value for
*	the parameter is copied.  
*
*	If no parameter is found and there is no default, then -1 is 
*	returned and the callers buffer is unmodified.
*
* RETURNS
*	0, or
*	-1 if an error is encountered
*
* EXAMPLE
* 1.	Get the value as a long for the integer environment parameter
*	EPICS_NUMBER_OF_ITEMS.
*
*	#include "envDefs.h"
*	long	count;
*	long	status;
*
*	status = envGetLongConfigParam(&EPICS_NUMBER_OF_ITEMS, &count);
*	if (status == 0) {
*	    printf("and the count is: %d\n", count);
*	}
*	else {
*	    printf("%s could not be found or was not an integer\n",
*			EPICS_NUMBER_OF_ITEMS.name);
*	}
*
*-*/
long epicsShareAPI envGetLongConfigParam(
const ENV_PARAM *pParam,/* I pointer to config param structure */
long	*pLong		/* O pointer to place to store value */
)
{
    char	text[128];
    char	*ptext;
    int		count;

    ptext = envGetConfigParam(pParam, sizeof text, text);
    if (ptext) {
	count = sscanf(text, "%ld", pLong);
	if (count == 1)
		return 0;
        (void)fprintf(stderr,"Unable to find an integer in %s=%s\n", 
		pParam->name, text);
    }
    return -1;
}


long epicsShareAPI
envGetBoolConfigParam(const ENV_PARAM *pParam, int *pBool)
{
    char text[20];

    if(!envGetConfigParam(pParam, sizeof(text), text))
        return -1;
    *pBool = epicsStrCaseCmp(text, "yes")==0;
    return 0;
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
*	EPICS_TS_NTP_INET.
*
*	#include "envDefs.h"
*
*	envPrtConfigParam(&EPICS_TS_NTP_INET);
*
*-*/
long epicsShareAPI envPrtConfigParam(
const ENV_PARAM *pParam)	/* pointer to config param structure */
{
    const char *pVal;

    pVal = envGetConfigParamPtr(pParam);
    if (pVal == NULL)
	fprintf(stdout, "%s is undefined\n", pParam->name);
    else
	fprintf(stdout,"%s: %s\n", pParam->name, pVal);
    return 0;
}

/*+/subr**********************************************************************
* NAME  epicsPrtEnvParams - print value of all configuration parameters
*
* DESCRIPTION
*       Prints all configuration parameters and their current value.
*
* RETURNS
*       0
*
* EXAMPLE
* 1.    Print the value for all EPICS-defined environment parameters.
*
*       #include "envDefs.h"
*
*       epicsPrtEnvParams();
*
*-*/
long epicsShareAPI
epicsPrtEnvParams(void)
{
    const ENV_PARAM **ppParam = env_param_list;
     
    while (*ppParam != NULL)
	envPrtConfigParam(*(ppParam++));

    return 0;
}

/*
 * envGetInetPortConfigParam ()
 */
epicsShareFunc unsigned short epicsShareAPI envGetInetPortConfigParam 
                (const ENV_PARAM *pEnv, unsigned short defaultPort)
{
    long        longStatus;
    long        epicsParam;

    longStatus = envGetLongConfigParam (pEnv, &epicsParam);
    if (longStatus!=0) {
        epicsParam = (long) defaultPort;
        errlogPrintf ("EPICS Environment \"%s\" integer fetch failed\n", pEnv->name);
        errlogPrintf ("setting \"%s\" = %ld\n", pEnv->name, epicsParam);
    }

    if (epicsParam<=IPPORT_USERRESERVED || epicsParam>USHRT_MAX) {
        errlogPrintf ("EPICS Environment \"%s\" out of range\n", pEnv->name);
        /*
         * Quit if the port is wrong due coding error
         */
        assert (epicsParam != (long) defaultPort);
        epicsParam = (long) defaultPort;
        errlogPrintf ("Setting \"%s\" = %ld\n", pEnv->name, epicsParam);
    }

    /*
     * ok to clip to unsigned short here because we checked the range
     */
    return (unsigned short) epicsParam;
}

