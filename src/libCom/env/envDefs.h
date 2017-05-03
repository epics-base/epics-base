/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	Author:	Roger A. Cole
 *	Date:	07-20-91
 *
 */
/****************************************************************************
* TITLE	envDefs.h - definitions for environment get/set routines
*
* DESCRIPTION
*	This file defines the environment parameters for EPICS.  These
*	ENV_PARAM's are created in envData.c by bldEnvData for
*	use by EPICS programs running under UNIX and VxWorks.
*
*	User programs can define their own environment parameters for their
*	own use--the only caveat is that such parameters aren't automatically
*	setup by EPICS.
*
*****************************************************************************/

#ifndef envDefsH
#define envDefsH

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

typedef struct envParam {
    char	*name;		/* text name of the parameter */
    char	*pdflt;
} ENV_PARAM;

/*
 * bldEnvData.pl looks for "epicsShareExtern const ENV_PARAM <name>;"
 */
epicsShareExtern const ENV_PARAM EPICS_CA_ADDR_LIST;
epicsShareExtern const ENV_PARAM EPICS_CA_CONN_TMO;
epicsShareExtern const ENV_PARAM EPICS_CA_AUTO_ADDR_LIST;
epicsShareExtern const ENV_PARAM EPICS_CA_REPEATER_PORT;
epicsShareExtern const ENV_PARAM EPICS_CA_SERVER_PORT;
epicsShareExtern const ENV_PARAM EPICS_CA_MAX_ARRAY_BYTES;
epicsShareExtern const ENV_PARAM EPICS_CA_AUTO_ARRAY_BYTES;
epicsShareExtern const ENV_PARAM EPICS_CA_MAX_SEARCH_PERIOD;
epicsShareExtern const ENV_PARAM EPICS_CA_NAME_SERVERS;
epicsShareExtern const ENV_PARAM EPICS_CA_MCAST_TTL;
epicsShareExtern const ENV_PARAM EPICS_CAS_INTF_ADDR_LIST;
epicsShareExtern const ENV_PARAM EPICS_CAS_IGNORE_ADDR_LIST;
epicsShareExtern const ENV_PARAM EPICS_CAS_AUTO_BEACON_ADDR_LIST;
epicsShareExtern const ENV_PARAM EPICS_CAS_BEACON_ADDR_LIST;
epicsShareExtern const ENV_PARAM EPICS_CAS_SERVER_PORT;
epicsShareExtern const ENV_PARAM EPICS_CA_BEACON_PERIOD; /* deprecated */
epicsShareExtern const ENV_PARAM EPICS_CAS_BEACON_PERIOD;
epicsShareExtern const ENV_PARAM EPICS_CAS_BEACON_PORT;
epicsShareExtern const ENV_PARAM EPICS_BUILD_COMPILER_CLASS;
epicsShareExtern const ENV_PARAM EPICS_BUILD_OS_CLASS;
epicsShareExtern const ENV_PARAM EPICS_BUILD_TARGET_ARCH;
epicsShareExtern const ENV_PARAM EPICS_TIMEZONE;
epicsShareExtern const ENV_PARAM EPICS_TS_NTP_INET;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_PORT;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_INET;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_FILE_LIMIT;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_FILE_NAME;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_FILE_COMMAND;
epicsShareExtern const ENV_PARAM EPICS_CMD_PROTO_PORT;
epicsShareExtern const ENV_PARAM EPICS_AR_PORT;
epicsShareExtern const ENV_PARAM IOCSH_PS1;
epicsShareExtern const ENV_PARAM IOCSH_HISTSIZE;
epicsShareExtern const ENV_PARAM IOCSH_HISTEDIT_DISABLE;

epicsShareExtern const ENV_PARAM *env_param_list[];

struct in_addr;

epicsShareFunc char * epicsShareAPI 
	envGetConfigParam(const ENV_PARAM *pParam, int bufDim, char *pBuf);
epicsShareFunc const char * epicsShareAPI 
	envGetConfigParamPtr(const ENV_PARAM *pParam);
epicsShareFunc long epicsShareAPI 
	envPrtConfigParam(const ENV_PARAM *pParam);
epicsShareFunc long epicsShareAPI 
	envGetInetAddrConfigParam(const ENV_PARAM *pParam, struct in_addr *pAddr);
epicsShareFunc long epicsShareAPI 
	envGetDoubleConfigParam(const ENV_PARAM *pParam, double *pDouble);
epicsShareFunc long epicsShareAPI 
	envGetLongConfigParam(const ENV_PARAM *pParam, long *pLong);
epicsShareFunc unsigned short epicsShareAPI envGetInetPortConfigParam 
        (const ENV_PARAM *pEnv, unsigned short defaultPort);
epicsShareFunc long epicsShareAPI
    envGetBoolConfigParam(const ENV_PARAM *pParam, int *pBool);
epicsShareFunc long epicsShareAPI epicsPrtEnvParams(void);
epicsShareFunc void epicsShareAPI epicsEnvSet (const char *name, const char *value);
epicsShareFunc void epicsShareAPI epicsEnvShow (const char *name);

#ifdef __cplusplus
}
#endif

#endif /*envDefsH*/

