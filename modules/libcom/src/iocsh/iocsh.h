/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* iocsh.h  ioc: call registered function*/
/* Author:  Marty Kraimer Date: 27APR2000 */

/**
 * @file iocsh.h
 * 
 * @brief C and C++ defintions of functions for IOC shell programming.
 * 
 * @details
 * The iocsh API provides an interface for running commands in the shell
 * of the IOC, as well as registering commands and variables for use in the shell.
 * It consists of 4 functions for the former and 2 functions for the latter.
 * 
 * @par Command functions:
 *     int iocsh (const char *pathname)@n 
 *     int iocshLoad (const char *pathname, const char *macros)@n 
 *     int iocshCmd (const char *cmd)@n 
 *     int iocshRun (const char *cmd, const char *macros)
 * 
 * @par Registration functions:
 *     void iocshRegister (const iocshFuncDef * piocshFuncDef, iocshCallFunc func)@n 
 *     void epicsStdCall iocshRegisterVariable (const iocshVarDef *piocshVarDef)
 */

#ifndef INCiocshH
#define INCiocshH

#include <stdio.h>
#include "compilerDependencies.h"
#include "libComAPI.h"

#if defined(vxWorks) || defined(__rtems__)
#define IOCSH_STATIC_FUNC
#else
#define IOCSH_STATIC_FUNC static EPICS_ALWAYS_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum iocshArgType
 *
 * This typedef lists the values that can be used as argument data types
 * when building the piocshFuncDef parameter of iocshRegister().
 * 
 * @code {.cpp}
 * static const iocshArg AsynGenericConfigArg0 = {"Port Name", iocshArgString};
 * static const iocshArg AsynGenericConfigArg1 = {"Number Devices", iocshArgInt};
 * @endcode
 */
typedef enum {
    iocshArgInt,
    iocshArgDouble,
    iocshArgString,
    iocshArgPdbbase,
    iocshArgArgv,
    iocshArgPersistentString,
    /** Equivalent to iocshArgString with a hint for tab completion as a record name.
     *  @since UNRELEASED
     */
    iocshArgStringRecord,
    /** Equivalent to iocshArgString with a hint for tab completion as a file system path.
     *  @since UNRELEASED
     */
    iocshArgStringPath,
}iocshArgType;

/**
 * @union iocshArgBuf
 * 
 * This union is used when building the func paramter of iocshRegister().
 * Each use should match the parameter type of the parameters of the
 * function being registered
 * 
 * @code {.cpp}
 * static void AsynGenericConfigCallFunc (const iocshArgBuf *args)
 * {
 *     AsynGenericConfig (args[0].sval, args[1].ival);
 * }
 * @endcode
 */
typedef union iocshArgBuf {
    int    ival;
    double dval;
    char  *sval;
    void  *vval;
    struct {
      int    ac;
      char **av;
    }aval;
}iocshArgBuf;

/**
 * @struct iocshVarDef
 * 
 * This struct is used with the function iocshRegisterVariable. Each
 * instance includes a name, a data type (see iocshArgType), and a
 * pointer to the value.
 */
typedef struct iocshVarDef {
    const char *name;
    iocshArgType type;
    void * pval;
}iocshVarDef;

/**
 * @struct iocshArg
 * 
 * This struct is used to indicate data types of function parameters
 * for iocshRegister().  The name element is used by the help command to print
 * a synopsis for the command.  The type element describes the data type of
 * the argument and takes a value from iocshArgType.
 * 
 * @code {.cpp}
 * static const iocshArg AsynGenericConfigArg0 = {"Port Name", iocshArgString};
 * static const iocshArg AsynGenericConfigArg1 = {"Number Devices", iocshArgInt};
 * static const iocshArg* const AsynGenericConfigArgs[]
    = { &AsynAXEConfigArg0, &AsynAXEConfigArg1 };

 * @endcode
 */
typedef struct iocshArg {
    const char *name;
    iocshArgType type;
}iocshArg;

/**
 * @struct iocshFuncDef
 * 
 * This struct is used with iocshRegister to define the function that
 * is being registered.
 * 
 * name - the name of the command or function@n 
 * nargs - the number of entries in the array of pointers to argument descriptions@n 
 * arg - an array of pointers to structs of type iocshArg@n 
 * 
 * @code {.cpp}
 * static const iocshFuncDef AsynGenericConfigFuncDef
 *  = { "AsynGenericConfig", 2, AsynGenericConfigArgs };
 * @endcode
 * 
 */
typedef struct iocshFuncDef {
    const char *name;
    int nargs;
    const iocshArg * const *arg;
    const char* usage;
}iocshFuncDef;
#define IOCSHFUNCDEF_HAS_USAGE

/**
 * @typedef 
 * 
 * This typedef defines a function that is used as the *piocshFuncDef
 * parameter of iocshRegister().
 * 
 * @code {.cpp}
 * static void AsynGenericConfigCallFunc (const iocshArgBuf *args)
 * {
 *     AsynGenericConfig (args[0].sval, args[1].ival);
 * }
 * 
 * static void AsynGenericRegister(void)
 * {
 *     iocshRegister(&AsynGenericConfigFuncDef, AsynGenericConfigCallFunc);
 * }
 * @endcode
 */
typedef void (*iocshCallFunc)(const iocshArgBuf *argBuf);

/**
 * @struct
 * 
 * This struct is used as a return value for the function iocshFindCommand.
 */
typedef struct iocshCmdDef {
    iocshFuncDef const   *pFuncDef;
    iocshCallFunc         func;
}iocshCmdDef;

/**
 * @brief This function is used to register a command with the IOC shell
 * 
 * @param piocshFuncDef A pointer to a data structure that describes the command and its arguments.
 * @param func A pointer to a function which is called by iocsh() when the command is encountered.
 * @return void
 */
LIBCOM_API void epicsStdCall iocshRegister(
    const iocshFuncDef *piocshFuncDef, iocshCallFunc func);

/**
 * @brief 
 * 
 * @param piocshVarDef 
 * @return void 
 */
LIBCOM_API void epicsStdCall iocshRegisterVariable (
    const iocshVarDef *piocshVarDef);

/**
 * @brief Returns a struct of type iocshCmdDef whose element values are determined
 * by the name parameter.  This function calls the function registryFind, defined in
 * Registry.h
 * 
 * @param name 
 * @return const iocshCmdDef*
 */
LIBCOM_API const iocshCmdDef * epicsStdCall iocshFindCommand(
    const char* name) EPICS_DEPRECATED;

/**
 * @brief Returns a struct of type iocshVarDef whose element values are determined
 * by the name parameter.  This function calls the function registryFind, defined in
 * Registry.h
 * 
 * @param name 
 * @return const iocshVarDef*
 */
LIBCOM_API const iocshVarDef * epicsStdCall iocshFindVariable(
    const char* name);

/* iocshFree frees storage used by iocshRegister*/
/* This should only be called when iocsh is no longer needed*/
/**
 * @brief Frees all memory allocated to registered commands and variables.
 * 
 * @return void 
 */
LIBCOM_API void epicsStdCall iocshFree(void);

/**  
 * @brief This function is used to execute IOC shell commands from a file.
 * 
 * Commands are read from the file until and exit command is encountered or the 
 * end-of-file character is reached.
 * 
 * @param pathname A string that represents the path to a file from which commands are read.
 * @return 0 on success, non-zero on error
 * 
 * Equivalent to:
 *  @code iocshLoad(pathname, NULL) @endcode */
LIBCOM_API int epicsStdCall iocsh(const char *pathname);

/**
 * @brief This function is used to exectute a single IOC shell command.
 * 
 * @param cmd A string that represents the command to be executed.
 * @return 0 on success, non-zero on error
 *  
 * Equivalent to:
 * @code iocshRun(cmd, NULL) @endcode */
LIBCOM_API int epicsStdCall iocshCmd(const char *cmd);
/**  
 * @brief Read and evaluate IOC shell commands from the given file.  A list of macros
 * can be supplied as a parameter.  These macros are treated as environment variables during
 * exectution of the file's commands.
 * 
 * @param pathname A string that represents the path to a file from which commands are read.
 * @param macros NULL or a comma separated list of macro definitions.  eg. "VAR1=x,VAR2=y"
 * @return 0 on success, non-zero on error
 */
LIBCOM_API int epicsStdCall iocshLoad(const char *pathname, const char* macros);
/** 
 * @brief Evaluate a single IOC shell command. A list of macros can be supplied
 * as a parameter.  These macros are treated as environment variables during
 * exectution of the command.
 * 
 * @param cmd Command string.  eg. "echo \"something or other\""
 * @param macros NULL or a comma separated list of macro definitions.  eg. "VAR1=x,VAR2=y"
 * @return 0 on success, non-zero on error
 */
LIBCOM_API int epicsStdCall iocshRun(const char *cmd, const char* macros);

/** 
 * @brief Signal error from an IOC shell function.
 *
 * @param err 0 - success (no op), !=0 - error
 * @return The err argument value.
 * @since 7.0.3.1
 */
LIBCOM_API int iocshSetError(int err);

/* Makes macros that shadow environment variables work correctly with epicsEnvSet */
/**
 * @brief Unsets macro values.
 * 
 * This function sets the values of all macros passed to either iocshLoad or 
 * iocshRun to NULL.
 * 
 * @param name 
 * @return void
 */
LIBCOM_API void epicsStdCall iocshEnvClear(const char *name);

/* 'weak' link to pdbbase */
LIBCOM_API extern struct dbBase **iocshPpdbbase;

#ifdef EPICS_PRIVATE_API

LIBCOM_API
extern char** (*iocshCompleteRecord)(const char *word);
#endif

#ifdef __cplusplus
}
#endif

#endif /*INCiocshH*/
