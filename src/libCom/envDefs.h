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
 * .02  08-07-91	joh	added ioc log env
 * .03  09-26-94	joh	ifdef out double inclusion	
 * .04  11-28-94	joh	new CA env var 
 * .05  04-20-95	anj	moved defaults to CONFIG_ENV
 * .06  09-11-96	joh 	ANSI prototypes	
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	envDefs.h - definitions for environment get/set routines
*
* DESCRIPTION
*	This file defines the environment parameters for EPICS.  These
*	ENV_PARAM's are initialized by $epics/share/bin/envSetupParams for
*	use by EPICS programs running under UNIX and VxWorks.
*
*	User programs can define their own environment parameters for their
*	own use--the only caveat is that such parameters aren't automatically
*	setup by EPICS.
*
* SEE ALSO
*	$epics/share/bin/envSetupParams, envSubr.c
*
*-***************************************************************************/

#ifndef envDefsH
#define envDefsH

#include <shareLib.h>

#ifdef WIN32
#       include <winsock.h>
#else
#       include <sys/types.h>
#       include <netinet/in.h>
#       include <arpa/inet.h>
#endif


typedef struct envParam {
    char	*name;		/* text name of the parameter */
    char	dflt[80];	/* default value for the parameter */
} ENV_PARAM;

/*
 * bldEnvData looks for "epicsShareExtern ENV_PARAM"
 */
epicsShareExtern ENV_PARAM EPICS_CA_ADDR_LIST; 
epicsShareExtern ENV_PARAM EPICS_CA_CONN_TMO; 
epicsShareExtern ENV_PARAM EPICS_CA_BEACON_PERIOD; 
epicsShareExtern ENV_PARAM EPICS_CA_AUTO_ADDR_LIST;
epicsShareExtern ENV_PARAM EPICS_CA_REPEATER_PORT;
epicsShareExtern ENV_PARAM EPICS_CA_SERVER_PORT;
epicsShareExtern ENV_PARAM EPICS_TS_MIN_WEST;
epicsShareExtern ENV_PARAM EPICS_TS_NTP_INET;
epicsShareExtern ENV_PARAM EPICS_IOC_LOG_PORT;
epicsShareExtern ENV_PARAM EPICS_IOC_LOG_INET;
epicsShareExtern ENV_PARAM EPICS_IOC_LOG_FILE_LIMIT;
epicsShareExtern ENV_PARAM EPICS_IOC_LOG_FILE_NAME;
epicsShareExtern ENV_PARAM EPICS_IOC_LOG_FILE_COMMAND;
epicsShareExtern ENV_PARAM EPICS_CMD_PROTO_PORT;
epicsShareExtern ENV_PARAM EPICS_AR_PORT;
#define EPICS_ENV_VARIABLE_COUNT 15

/*
 * N elements added here to satisfy microsoft development tools
 * (includes room for nill termination)
 *
 * bldEnvData looks for "epicsShareExtern ENV_PARAM" so
 * this always needs to be divided into two lines
 */
epicsShareExtern ENV_PARAM
	*env_param_list[EPICS_ENV_VARIABLE_COUNT+1];

#ifdef __STDC__
char * epicsShareAPI envGetConfigParam(ENV_PARAM *pParam, 
				int bufDim, char *pBuf);
long epicsShareAPI envPrtConfigParam(ENV_PARAM *pParam);
long epicsShareAPI envSetConfigParam(ENV_PARAM *pParam, 
			char *value);
long epicsShareAPI envGetInetAddrConfigParam(ENV_PARAM *pParam, 
			struct in_addr *pAddr);
long epicsShareAPI envGetDoubleConfigParam(ENV_PARAM *pParam, 
			double *pDouble);
long epicsShareAPI envGetLongConfigParam(ENV_PARAM *pParam, 
			long *pLong);
#else
char * epicsShareAPI envGetConfigParam();
long epicsShareAPI envPrtConfigParam();
long epicsShareAPI envSetConfigParam();
long epicsShareAPI envGetInetAddrConfigParam();
long epicsShareAPI envGetDoubleConfigParam();
long epicsShareAPI envGetLongConfigParam();
#endif

#endif /*envDefsH*/

