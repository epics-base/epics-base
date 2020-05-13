/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * @file envDefs.h
 * @author Roger A. Cole
 *
 * @brief Routines to get and set EPICS environment parameters.
 *
 * This file defines environment parameters used by EPICS and the
 * routines used to get and set those parameter values.
 *
 * The ENV_PARAM's declared here are defined in the generated file
 * envData.c by the bldEnvData.pl Perl script, which parses this file
 * and the build system's CONFIG_ENV and CONFIG_SITE_ENV files.
 * User programs can define their own environment parameters for their
 * own use--the only caveat is that such parameters aren't automatically
 * setup by EPICS.
 *
 * @note bldEnvData.pl looks for "epicsShareExtern const ENV_PARAM <name>;"
 */

#ifndef envDefsH
#define envDefsH

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

/**
 * @brief A structure to hold a single environment parameter
 */
typedef struct envParam {
    char *name;     /**< @brief Name of the parameter */
    char *pdflt;    /**< @brief Default value */
} ENV_PARAM;

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
epicsShareExtern const ENV_PARAM EPICS_CA_BEACON_PERIOD; /**< @brief deprecated */
epicsShareExtern const ENV_PARAM EPICS_CAS_BEACON_PERIOD;
epicsShareExtern const ENV_PARAM EPICS_CAS_BEACON_PORT;
epicsShareExtern const ENV_PARAM EPICS_BUILD_COMPILER_CLASS;
epicsShareExtern const ENV_PARAM EPICS_BUILD_OS_CLASS;
epicsShareExtern const ENV_PARAM EPICS_BUILD_TARGET_ARCH;
epicsShareExtern const ENV_PARAM EPICS_TZ;
epicsShareExtern const ENV_PARAM EPICS_TS_NTP_INET;
epicsShareExtern const ENV_PARAM EPICS_IOC_IGNORE_SERVERS;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_PORT;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_INET;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_FILE_LIMIT;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_FILE_NAME;
epicsShareExtern const ENV_PARAM EPICS_IOC_LOG_FILE_COMMAND;
epicsShareExtern const ENV_PARAM IOCSH_PS1;
epicsShareExtern const ENV_PARAM IOCSH_HISTSIZE;
epicsShareExtern const ENV_PARAM IOCSH_HISTEDIT_DISABLE;
epicsShareExtern const ENV_PARAM *env_param_list[];

struct in_addr;

/**
 * @brief Get value of a configuration parameter
 *
 * Gets the value of a configuration parameter from the environment
 * and copies it into the caller's buffer. If the parameter isn't
 * set in the environment, the default value for the parameter is
 * copied instead. If neither provides a non-empty string the buffer
 * is set to '\0' and NULL is returned.
 *
 * @param pParam Pointer to config param structure.
 * @param bufDim Dimension of parameter buffer
 * @param pBuf Pointer to parameter buffer
 * @return Pointer to the environment variable value string, or
 * NULL if no parameter value and default value was empty.
 */
epicsShareFunc char * epicsShareAPI
	envGetConfigParam(const ENV_PARAM *pParam, int bufDim, char *pBuf);

/**
 * @brief Get a configuration parameter's value or default string.
 *
 * @param pParam Pointer to config param structure.
 * @return Pointer to the environment variable value string, or to
 * the default value for the parameter, or NULL if those strings
 * were empty or not set.
 */
epicsShareFunc const char * epicsShareAPI
	envGetConfigParamPtr(const ENV_PARAM *pParam);

/**
 * @brief Print the value of a configuration parameter.
 *
 * @param pParam Pointer to config param structure.
 * @return 0
 */
epicsShareFunc long epicsShareAPI
	envPrtConfigParam(const ENV_PARAM *pParam);

/**
 * @brief Get value of an inet addr config parameter.
 *
 * Gets the value of a configuration parameter and copies it into
 * the caller's (struct in_addr) buffer. If the configuration parameter
 * isn't found in the environment, the default value for that parameter
 * will be used. The resulting string is converted from a dotted-quad
 * format or looked up using the DNS and copied into the inet structure.
 *
 * If no parameter is found and there is no default, then -1 is
 * returned and the callers buffer is unmodified.
 *
 * @param pParam Pointer to config param structure.
 * @param pAddr Pointer to struct to receive inet addr.
 * @return 0, or -1 if an error is encountered
 */
epicsShareFunc long epicsShareAPI
	envGetInetAddrConfigParam(const ENV_PARAM *pParam, struct in_addr *pAddr);

/**
 * @brief Get value of a double configuration parameter.
 *
 * Gets the value of a configuration parameter, converts it into a
 * @c double value and copies that into the caller's buffer. If the
 * configuration parameter isn't found in the environment, the
 * default value for the parameter is used instead.
 *
 * If no parameter is found and there is no default, then -1 is
 * returned and the callers buffer is unmodified.
 *
 * @param pParam Pointer to config param structure.
 * @param pDouble Pointer to place to store value.
 * @return 0, or -1 if an error is encountered
 */
epicsShareFunc long epicsShareAPI
	envGetDoubleConfigParam(const ENV_PARAM *pParam, double *pDouble);

/**
 * @brief Get value of a long configuration parameter.
 *
 * Gets the value of a configuration parameter, converts it into a
 * @c long value and copies that into the caller's buffer. If the
 * configuration parameter isn't found in the environment, the
 * default value for the parameter is used instead.
 *
 * If no parameter is found and there is no default, then -1 is
 * returned and the callers buffer is unmodified.
 *
 * @param pParam Pointer to config param structure.
 * @param pLong Pointer to place to store value.
 * @return 0, or -1 if an error is encountered
 */
epicsShareFunc long epicsShareAPI
	envGetLongConfigParam(const ENV_PARAM *pParam, long *pLong);

/**
 * @brief Get value of a port number configuration parameter.
 *
 * Returns the value of a configuration parameter that represents
 * an inet port number. If no environment variable is found the
 * default parameter value is used, and if that is also unset the
 * @c defaultPort argument returned instead. The integer value must
 * fall between the values IPPORT_USERRESERVED and USHRT_MAX or the
 * @c defaultPort argument will be substituted instead.
 *
 * @param pEnv Pointer to config param structure.
 * @param defaultPort Port number to be used if environment settings invalid.
 * @return Port number.
 */
epicsShareFunc unsigned short epicsShareAPI envGetInetPortConfigParam
        (const ENV_PARAM *pEnv, unsigned short defaultPort);
/**
 * @brief Get value of a boolean configuration parameter.
 *
 * Gets the value of a configuration parameter, and puts the value
 * 0 or 1 into the caller's buffer depending on the value. If the
 * configuration parameter isn't found in the environment, the
 * default value for the parameter is used instead.
 *
 * A value is treated as True (1) if it compares equal to the
 * string "yes" with a case-independent string comparison. All
 * other strings are treated as False (0).
 *
 * If no parameter is found and there is no default, then -1 is
 * returned and the callers buffer is unmodified.
 *
 * @param pParam Pointer to config param structure.
 * @param pBool Pointer to place to store value.
 * @return 0, or -1 if an error is encountered
 */
epicsShareFunc long epicsShareAPI
    envGetBoolConfigParam(const ENV_PARAM *pParam, int *pBool);

/**
 * @brief Prints all configuration parameters and their current value.
 *
 * @return 0
 */
epicsShareFunc long epicsShareAPI epicsPrtEnvParams(void);

/**
 * @brief Set an environment variable's value
 *
 * The setenv() routine is not available on all operating systems.
 * This routine provides a portable alternative for all EPICS targets.
 * @param name Environment variable name.
 * @param value New value for environment variable.
 */
epicsShareFunc void epicsShareAPI epicsEnvSet (const char *name, const char *value);
/**
 * @brief Clear the value of an environment variable
 * @param name Environment variable name.
 */
epicsShareFunc void epicsShareAPI epicsEnvUnset (const char *name);
/**
 * @brief Print value of an environment variable, or all variables
 *
 * @param name Environment variable name, or NULL to show all.
 */
epicsShareFunc void epicsShareAPI epicsEnvShow (const char *name);

#ifdef __cplusplus
}
#endif

#endif /*envDefsH*/
