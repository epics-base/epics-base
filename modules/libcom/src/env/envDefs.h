/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file envDefs.h
 * \author Roger A. Cole
 *
 * \brief Routines to get and set EPICS environment parameters.
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
 * \note bldEnvData.pl looks for "LIBCOM_API extern const ENV_PARAM <name>;"
 */

#ifndef envDefsH
#define envDefsH

#ifdef __cplusplus
extern "C" {
#endif

#include "libComAPI.h"

/**
 * \brief A structure to hold a single environment parameter
 */
typedef struct envParam {
    char *name;     /**< \brief Name of the parameter */
    char *pdflt;    /**< \brief Default value */
} ENV_PARAM;

LIBCOM_API extern const ENV_PARAM EPICS_CA_ADDR_LIST;
LIBCOM_API extern const ENV_PARAM EPICS_CA_CONN_TMO;
LIBCOM_API extern const ENV_PARAM EPICS_CA_AUTO_ADDR_LIST;
LIBCOM_API extern const ENV_PARAM EPICS_CA_REPEATER_PORT;
LIBCOM_API extern const ENV_PARAM EPICS_CA_SERVER_PORT;
LIBCOM_API extern const ENV_PARAM EPICS_CA_MAX_ARRAY_BYTES;
LIBCOM_API extern const ENV_PARAM EPICS_CA_AUTO_ARRAY_BYTES;
LIBCOM_API extern const ENV_PARAM EPICS_CA_MAX_SEARCH_PERIOD;
LIBCOM_API extern const ENV_PARAM EPICS_CA_NAME_SERVERS;
LIBCOM_API extern const ENV_PARAM EPICS_CA_MCAST_TTL;
LIBCOM_API extern const ENV_PARAM EPICS_CAS_INTF_ADDR_LIST;
LIBCOM_API extern const ENV_PARAM EPICS_CAS_IGNORE_ADDR_LIST;
LIBCOM_API extern const ENV_PARAM EPICS_CAS_AUTO_BEACON_ADDR_LIST;
LIBCOM_API extern const ENV_PARAM EPICS_CAS_BEACON_ADDR_LIST;
LIBCOM_API extern const ENV_PARAM EPICS_CAS_SERVER_PORT;
LIBCOM_API extern const ENV_PARAM EPICS_CA_BEACON_PERIOD; /**< \brief deprecated */
LIBCOM_API extern const ENV_PARAM EPICS_CAS_BEACON_PERIOD;
LIBCOM_API extern const ENV_PARAM EPICS_CAS_BEACON_PORT;
LIBCOM_API extern const ENV_PARAM EPICS_BUILD_COMPILER_CLASS;
LIBCOM_API extern const ENV_PARAM EPICS_BUILD_OS_CLASS;
LIBCOM_API extern const ENV_PARAM EPICS_BUILD_TARGET_ARCH;
LIBCOM_API extern const ENV_PARAM EPICS_TZ;
LIBCOM_API extern const ENV_PARAM EPICS_TS_NTP_INET;
LIBCOM_API extern const ENV_PARAM EPICS_IOC_IGNORE_SERVERS;
LIBCOM_API extern const ENV_PARAM EPICS_IOC_LOG_PORT;
LIBCOM_API extern const ENV_PARAM EPICS_IOC_LOG_INET;
LIBCOM_API extern const ENV_PARAM EPICS_IOC_LOG_FILE_LIMIT;
LIBCOM_API extern const ENV_PARAM EPICS_IOC_LOG_FILE_NAME;
LIBCOM_API extern const ENV_PARAM EPICS_IOC_LOG_FILE_COMMAND;
LIBCOM_API extern const ENV_PARAM IOCSH_PS1;
LIBCOM_API extern const ENV_PARAM IOCSH_HISTSIZE;
LIBCOM_API extern const ENV_PARAM IOCSH_HISTEDIT_DISABLE;
LIBCOM_API extern const ENV_PARAM *env_param_list[];

struct in_addr;

/**
 * \brief Get value of a configuration parameter
 *
 * Gets the value of a configuration parameter from the environment
 * and copies it into the caller's buffer. If the parameter isn't
 * set in the environment, the default value for the parameter is
 * copied instead. If neither provides a non-empty string the buffer
 * is set to '\0' and NULL is returned.
 *
 * \param pParam Pointer to config param structure.
 * \param bufDim Dimension of parameter buffer
 * \param pBuf Pointer to parameter buffer
 * \return Pointer to the environment variable value string, or
 * NULL if no parameter value and default value was empty.
 */
LIBCOM_API char * epicsStdCall
    envGetConfigParam(const ENV_PARAM *pParam, int bufDim, char *pBuf);

/**
 * \brief Get a configuration parameter's value or default string.
 *
 * \param pParam Pointer to config param structure.
 * \return Pointer to the environment variable value string, or to
 * the default value for the parameter, or NULL if those strings
 * were empty or not set.
 */
LIBCOM_API const char * epicsStdCall
    envGetConfigParamPtr(const ENV_PARAM *pParam);

/**
 * \brief Print the value of a configuration parameter.
 *
 * \param pParam Pointer to config param structure.
 * \return 0
 */
LIBCOM_API long epicsStdCall
    envPrtConfigParam(const ENV_PARAM *pParam);

/**
 * \brief Get value of an inet addr config parameter.
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
 * \param pParam Pointer to config param structure.
 * \param pAddr Pointer to struct to receive inet addr.
 * \return 0, or -1 if an error is encountered
 */
LIBCOM_API long epicsStdCall
    envGetInetAddrConfigParam(const ENV_PARAM *pParam, struct in_addr *pAddr);

/**
 * \brief Get value of a double configuration parameter.
 *
 * Gets the value of a configuration parameter, converts it into a
 * \c double value and copies that into the caller's buffer. If the
 * configuration parameter isn't found in the environment, the
 * default value for the parameter is used instead.
 *
 * If no parameter is found and there is no default, then -1 is
 * returned and the callers buffer is unmodified.
 *
 * \param pParam Pointer to config param structure.
 * \param pDouble Pointer to place to store value.
 * \return 0, or -1 if an error is encountered
 */
LIBCOM_API long epicsStdCall
    envGetDoubleConfigParam(const ENV_PARAM *pParam, double *pDouble);

/**
 * \brief Get value of a long configuration parameter.
 *
 * Gets the value of a configuration parameter, converts it into a
 * \c long value and copies that into the caller's buffer. If the
 * configuration parameter isn't found in the environment, the
 * default value for the parameter is used instead.
 *
 * If no parameter is found and there is no default, then -1 is
 * returned and the callers buffer is unmodified.
 *
 * \param pParam Pointer to config param structure.
 * \param pLong Pointer to place to store value.
 * \return 0, or -1 if an error is encountered
 */
LIBCOM_API long epicsStdCall
    envGetLongConfigParam(const ENV_PARAM *pParam, long *pLong);

/**
 * \brief Get value of a port number configuration parameter.
 *
 * Returns the value of a configuration parameter that represents
 * an inet port number. If no environment variable is found the
 * default parameter value is used, and if that is also unset the
 * \c defaultPort argument returned instead. The integer value must
 * fall between the values IPPORT_USERRESERVED and USHRT_MAX or the
 * \c defaultPort argument will be substituted instead.
 *
 * \param pEnv Pointer to config param structure.
 * \param defaultPort Port number to be used if environment settings invalid.
 * \return Port number.
 */
LIBCOM_API unsigned short epicsStdCall envGetInetPortConfigParam
    (const ENV_PARAM *pEnv, unsigned short defaultPort);
/**
 * \brief Get value of a boolean configuration parameter.
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
 * \param pParam Pointer to config param structure.
 * \param pBool Pointer to place to store value.
 * \return 0, or -1 if an error is encountered
 */
LIBCOM_API long epicsStdCall
    envGetBoolConfigParam(const ENV_PARAM *pParam, int *pBool);

/**
 * \brief Prints all configuration parameters and their current value.
 *
 * \return 0
 */
LIBCOM_API long epicsStdCall epicsPrtEnvParams(void);

/**
 * \brief Set an environment variable's value
 *
 * The setenv() routine is not available on all operating systems.
 * This routine provides a portable alternative for all EPICS targets.
 * \param name Environment variable name.
 * \param value New value for environment variable.
 */
LIBCOM_API void epicsStdCall epicsEnvSet (const char *name, const char *value);
/**
 * \brief Clear the value of an environment variable
 * \param name Environment variable name.
 */
LIBCOM_API void epicsStdCall epicsEnvUnset (const char *name);
/**
 * \brief Print value of an environment variable, or all variables
 *
 * \param name Environment variable name, or NULL to show all.
 */
LIBCOM_API void epicsStdCall epicsEnvShow (const char *name);

#ifdef __cplusplus
}
#endif

#endif /*envDefsH*/
