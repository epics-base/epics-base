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
 * @brief C and C++ definitions of functions for IOC shell programming.
 *
 * @details
 * The iocsh API provides an interface for running commands in the shell
 * of the IOC, as well as registering commands and variables for use in the shell.
 * It consists of 4 functions for the former and 2 functions for the latter.
 *
 * @par Command functions:
 *     - iocsh()
 *     - iocshLoad()
 *     - iocshCmd()
 *     - iocshRun()
 *
 * @par Registration functions:
 *     - iocshRegister()
 *     - iocshRegisterVariable()
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
 * This typedef lists the values that can be used as argument data types
 * when building the first parameter of iocshRegister().
 *
 * ```
 * static const iocshArg AsynGenericConfigArg0 = {
 *     // name
 *     "Port Name",
 *     // type
 *     iocshArgString,
 * };
 * static const iocshArg AsynGenericConfigArg1 = {
 *     // name
 *     "Number Devices",
 *     // type
 *     iocshArgInt,
 * };
 * ```
 */
typedef enum {
	/** The argument is converted to an integer value. */
    iocshArgInt,
	/** The argument is converted to a double-precision floating point value. */
    iocshArgDouble,
	/** The argument is left as a string.
	 *
	 * The memory used to hold the string is "owned" by iocsh
	 * and will be reused once the handler function returns.
	 */
    iocshArgString,
	/** The argument must be pdbbase. */
    iocshArgPdbbase,
	/** An arbitrary number of arguments is expected.
	 *
	 * Subsequent iocshArg structures will be ignored.
	 */
    iocshArgArgv,
	/** A copy of the argument will be made and a pointer to the copy will be passed to the handler.
	 *
	 * The called function should eventually release this copy
	 * by using the pointer as an argument to free().
	 */
    iocshArgPersistentString,
    /**
     * Equivalent to iocshArgString with a hint for tab completion that the
     * argument is a record name.
     * @since 7.0.8
     */
    iocshArgStringRecord,
    /**
     * Equivalent to iocshArgString with a hint for tab completion that the
     * argument is a file system path.
     * @since 7.0.8
     */
    iocshArgStringPath,
}iocshArgType;

/**
 * This union is used when building the func parameter of iocshRegister().
 * Each use should match the parameter iocshArgType of the parameters of the
 * function being registered
 *
 * ```
 * static void AsynGenericConfigCallFunc (const iocshArgBuf *args)
 * {
 *     AsynGenericConfig (args[0].sval, args[1].ival);
 * }
 * ```
 */
typedef union iocshArgBuf {
	/** The value as an integer.
	 *
	 * Corresponds to the @ref iocshArgInt type.
	 */
    int    ival;
	/** The value as a double-precision floating point.
	 *
	 * Corresponds to the @ref iocshArgDouble type.
	 */
    double dval;
	/** The value as a string.
	 *
	 * Corresponds to the @ref iocshArgString and related types.
	 */
    char  *sval;
	/** The value as an untyped pointer.
	 *
	 * Can be used with the @ref iocshArgPdbbase type.
	 */
    void  *vval;
	/** The variadic arguments, for the @ref iocshArgArgv type. */
    struct {
	  /** Number of arguments passed to the IOC shell command.
	   *
	   * Provides the number of elements of the `av` array.
	   */
      int    ac;
	  /** The arguments, as an array of strings. */
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
 * Data types of function parameters for iocshRegister().
 *
 * @par Example:
 * ```
 * static const iocshArg AsynGenericConfigArg0 = {
 *     // name
 *     "Port Name",
 *     // type
 *     iocshArgString,
 * };
 * static const iocshArg AsynGenericConfigArg1 = {
 *     // name
 *     "Number Devices",
 *     // type
 *     iocshArgInt,
 * };
 * static const iocshArg* const AsynGenericConfigArgs[] = {
 *     &AsynGenericConfigArg0,
 *     &AsynGenericConfigArg1,
 * };
 * ```
 */
typedef struct iocshArg {
	/** Used by the `help` command to print a synopsis for the command. */
    const char *name;
	/** Data type of the argument. */
    iocshArgType type;
}iocshArg;

/**
 * Used with iocshRegister() to define the function that is being registered.
 *
 * @par Example:
 * ```
 * static const iocshFuncDef AsynGenericConfigFuncDef = {
 *     // name
 *     "AsynGenericConfig",
 *     // nargs
 *     2,
 *     // arg
 *     AsynGenericConfigArgs,
 *     // usage
 *     "Helpful message describing the command",
 * };
 * ```
 */
typedef struct iocshFuncDef {
	/** Name of the command or function. */
    const char *name;
 	/** Number of entries in the array of pointers to argument descriptions.
	 *
	 * If 0, `arg` can be `NULL`.
	 */
    int nargs;
	/** Array of pointers to structs of type iocshArg.
	 *
	 * Can be `NULL` if `nargs` is 0.
	 */
    const iocshArg * const *arg;
	/** Text displayed when using running `help <command>`. */
    const char* usage;
}iocshFuncDef;
#define IOCSHFUNCDEF_HAS_USAGE

/**
 * This typedef defines a function that is used
 * as the first parameter of iocshRegister().
 *
 * ```
 * static void AsynGenericConfigCallFunc (const iocshArgBuf *args)
 * {
 *     AsynGenericConfig (args[0].sval, args[1].ival);
 * }
 *
 * static void AsynGenericRegister(void)
 * {
 *     iocshRegister(&AsynGenericConfigFuncDef, AsynGenericConfigCallFunc);
 * }
 * ```
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
 * @brief Register a command with the IOC shell.
 *
 * @param[in] piocshFuncDef
 *     A pointer to a data structure that describes the command and its arguments.
 *     See the IOC Shell section of the Application Developer's Guide for more information.
 * @param[in] func A pointer to a function which is called by iocsh() when the command is encountered.
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
 * @brief Read and evaluate IOC shell commands from the given file.
 *
 * Equivalent to:
 * @code iocshLoad(pathname, NULL) @endcode
 *
 * @see iocshLoad()
 */
LIBCOM_API int epicsStdCall iocsh(const char *pathname);

/**
 * @brief Run a single IOC shell command.
 *
 * Equivalent to:
 * @code iocshRun(cmd, NULL) @endcode
 *
 * @see iocshRun()
 */
LIBCOM_API int epicsStdCall iocshCmd(const char *cmd);

/**
 * @brief Read and evaluate IOC shell commands from the given file.
 *
 * Commands are read from the file
 * until an `exit` command is encountered
 * or end-of-file is reached.
 *
 * A list of macros can be supplied as a parameter.
 * These macros are treated as environment variables
 * during execution of the file's commands.
 *
 * @sa iocsh()
 *
 * @param[in] pathname
 *     A string that represents the path to a file from which commands are read.
 *     If `NULL`, commands are read from the standard input.
 * @param[in] macros
 *     `NULL` or a comma separated list of macro definitions.
 * 	   eg. `"VAR1=x,VAR2=y"`
 *
 * @retval 0 on success
 * @retval non-zero on error
 * @retval -1 if the specified file can't be opened
 */
LIBCOM_API int epicsStdCall iocshLoad(const char *pathname, const char* macros);

/**
 * @brief Run a single IOC shell command.
 *
 * A list of macros can be supplied as a parameter.
 * These macros are treated as environment variables
 * during execution of the command.
 *
 * This function may be run from any thread,
 * but many IOC shell commands may not be thread-safe.
 *
 * @sa iocshCmd()
 *
 * @param[in] cmd
 *     Command string.
 *     eg. `"echo \"something or other\""`
 * @param[in] macros
 *     `NULL` or a comma separated list of macro definitions.
 *     eg. `"VAR1=x,VAR2=y"`
 *
 * @retval 0 on success
 * @retval non-zero on error
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
