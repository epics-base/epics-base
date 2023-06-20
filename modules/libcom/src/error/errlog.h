/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_errlog_H
#define INC_errlog_H

/** \file errlog.h
 * \brief Functions for interacting with the errlog task
 *
 * This file contains functions for passing error messages with varying severity,
 * registering and un-registering listeners and modifying the log buffer size and
 * max message size.
 *
 * Some of these functions are similar to the standard C library functions printf
 * and vprintf. For details on the arguments and return codes it is useful to consult
 * any book that describes the standard C library such as
 * `The C Programming Language ANSI C Edition` by Kernighan and Ritchie.
 *
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "libComAPI.h"
#include "compilerDependencies.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * errlogListener function type.
 *
 * This is used when adding or removing log listeners in ::errlogAddListener
 * and ::errlogRemoveListeners.
 */
typedef void (*errlogListener)(void *pPrivate, const char *message);

/** errlog severity enums */
typedef enum {
    errlogInfo,
    errlogMinor,
    errlogMajor,
    errlogFatal
} errlogSevEnum;

/** Boolean to control whether some messages include more detail */
LIBCOM_API extern int errVerbose;


#ifdef ERRLOG_INIT
    const char *errlogSevEnumString[] = {
        "info",
        "minor",
        "major",
        "fatal"
    };
#else
    /** String representation of errlog severity values */
    LIBCOM_API extern const char * errlogSevEnumString[];
#endif

/**
 * errMessage is a macro so it can get the file and line number. It prints the message,
 * the status symbol and string values, and the name of the task which invoked errMessage.
 * It also prints the name of the source file and the line number from which the call was issued.
 *
 * The message to print should not include a newline as one is added implicitly.
 *
 * The status code used for the 1st argument is:
 * - 0: Find latest vxWorks or Unix error (errno value).
 * - -1: Don't report status.
 * - Other: Use this status code and lookup the string value
 *
 * \param S Status code
 * \param PM The message to print
 */
#define errMessage(S, PM) \
     errPrintf(S, __FILE__, __LINE__, " %s\n", PM)

/** epicsPrintf is an old name for errlog routines */
#define epicsPrintf errlogPrintf

/** epicsVprintf is an old name for errlog routines */
#define epicsVprintf errlogVprintf

/**
 * errlogPrintf is like the printf function provided by the standard C library, except
 * that the output is sent to the errlog task. Unless configured not to, the output
 * will appear on the console as well.
 */
LIBCOM_API int errlogPrintf(EPICS_PRINTF_FMT(const char *pformat), ...)
    EPICS_PRINTF_STYLE(1,2);

/**
 * errlogVprintf is like the vprintf function provided by the standard C library, except
 * that the output is sent to the errlog task. Unless configured not to, the output
 * will appear on the console as well.
 */
LIBCOM_API int errlogVprintf(const char *pformat, va_list pvar);

/**
 * This function is like ::errlogPrintf except that it adds the severity to the beginning
 * of the message in the form `sevr=<value>` where value is one of the enumerated
 * severities in ::errlogSevEnum. Also the message is suppressed if severity is less than
 * the current severity to suppress.
 *
 * \param severity One of the severity enums from ::errlogSevEnum
 * \param pformat The message to log or print
 * \return int Consult printf documentation in C standard library
 */
LIBCOM_API int errlogSevPrintf(const errlogSevEnum severity,
    EPICS_PRINTF_FMT(const char *pformat), ...
) EPICS_PRINTF_STYLE(2,3);

/**
 * This function is like ::errlogVprintf except that it adds the severity to the beginning
 * of the message in the form `sevr=<value>` where value is one of the enumerated
 * severities in ::errlogSevEnum. Also the message is suppressed if severity is less than
 * the current severity to suppress. If epicsThreadIsOkToBlock is true, which is
 * true during iocInit, errlogSevVprintf does NOT send output to the
 * errlog task.
 *
 * \param severity One of the severity enums from ::errlogSevEnum
 * \param pformat The message to log or print
 * \param pvar va_list
 * \return int Consult printf documentation in C standard library
 */
LIBCOM_API int errlogSevVprintf(const errlogSevEnum severity,
    const char *pformat, va_list pvar);

/**
 * Sends message to the errlog task.
 *
 * \param message The message to send
 */
LIBCOM_API int errlogMessage(const char *message);

/**
 * Gets the string value of severity.
 *
 * \param severity The severity from ::errlogSevEnum
 * \return The string value
 */
LIBCOM_API const char * errlogGetSevEnumString(errlogSevEnum severity);

/**
 * Sets the severity to log
 *
 * \param severity The severity from ::errlogSevEnum
 */
LIBCOM_API void errlogSetSevToLog(errlogSevEnum severity);

/**
 * Gets the current severity to log
 *
 * \return ::errlogSevEnum
 */
LIBCOM_API errlogSevEnum errlogGetSevToLog(void);

/**
 * Any code can receive errlog message. This function will add a listener callback.
 *
 * \param listener Function pointer of type ::errlogListener
 * \param pPrivate This will be passed as the first argument of listener()
 */
LIBCOM_API void errlogAddListener(errlogListener listener, void *pPrivate);

/**
 * This function will remove a listener callback.
 *
 * \param listener Function pointer of type ::errlogListener
 * \param pPrivate This will be passed as the first argument of listener()
 *
 * \since UNRELEASED Safe to call from a listener callback.
 * \until UNRELEASED Self-removal from a listener callback caused corruption.
 */
LIBCOM_API int errlogRemoveListeners(errlogListener listener,
    void *pPrivate);

/**
 * Normally the errlog system displays all messages on the console.
 * During error message storms this function can be used to suppress console messages.
 * A argument of 0 suppresses the messages, any other value lets messages go to the console.
 *
 * \param yesno (0=No, 1=Yes)
 * \return 0
 */
LIBCOM_API int eltc(int yesno);

/**
 * Sets a new stream to write the messages to
 *
 * \param stream Pointer to file handle
 * \return 0
 */
LIBCOM_API int errlogSetConsole(FILE *stream);

/**
 * Can be used to initialize the error logging system with a larger buffer. The default buffer size is 1280 bytes.
 *
 * \param bufsize The desired buffer size
 */
LIBCOM_API int errlogInit(int bufsize);

/**
 * errlogInit2 can be used to initialize the error logging system with a larger buffer and maximum message size.
 * The default buffer size is 1280 bytes, and the default maximum message size is 256.
 *
 * \param bufsize The desired buffer size
 * \param maxMsgSize The desired max message size
 */
LIBCOM_API int errlogInit2(int bufsize, int maxMsgSize);

/** Wakes up the errlog task and then waits until all messages are flushed from the queue. */
LIBCOM_API void errlogFlush(void);

/**
 * Routine errPrintf is normally called as follows:
 * `errPrintf(status, __FILE__, __LINE__, "<fmt>", ...); `
 *
 * Where status is defined as:
 * - 0: Find latest vxWorks or Unix error.
 * - -1: Don't report status.
 * - Other: Use this status code and lookup the string value
 *
 * \param status See above
 * \param pFileName As shown or NULL if the file name and line number should not be printed.
 * \param lineno As shown
 * \param pformat The message to log or print
 *
 * The remaining arguments are just like the arguments to the C printf routine.
 * ::errVerbose determines if the filename and line number are shown.
 */
LIBCOM_API void errPrintf(
    long status, const char *pFileName, int lineno, 
    EPICS_PRINTF_FMT(const char *pformat), ...
) EPICS_PRINTF_STYLE(4,5);

LIBCOM_API int errlogPrintfNoConsole(
    EPICS_PRINTF_FMT(const char *pformat), ...
) EPICS_PRINTF_STYLE(1,2);
LIBCOM_API int errlogVprintfNoConsole(const char *pformat,va_list pvar);

/**
 * Lookup the status code and return the string value in pBuf
 *
 * \param status The status code to lookup
 * \param pBuf The char buffer to write the string value into
 * \param bufLength The max size of pBuf
 */
LIBCOM_API void errSymLookup(long status, char *pBuf, size_t bufLength);

/** @defgroup colormacros Color macros
 *
 * @brief Colorize string constants with ANSI terminal escapes.
 *
 * The ANSI_ESC_\* family of macros expand to individual escape sequences.
 * The ANSI_\* family expand around a string constant to prepend a color, and append a RESET.
 *
 * @code
 * // equivalent
 * errlogPrintf(ERL_ERROR ": something is amiss\n");
 * errlogPrintf(ANSI_RED("ERROR") ": something is amiss\n");
 * errlogPrintf(ANSI_ESC_RED "ERROR" ANSI_ESC_RESET ": something is amiss\n");
 * @endcode
 *
 * @since EPICS 7.0.7
 *
 * @see errlogPrintf()
 * @{
 */
#define ANSI_ESC_RED "\033[31;1m"
#define ANSI_ESC_GREEN "\033[32;1m"
#define ANSI_ESC_YELLOW "\033[33;1m"
#define ANSI_ESC_BLUE "\033[34;1m"
#define ANSI_ESC_MAGENTA "\033[35;1m"
#define ANSI_ESC_CYAN "\033[36;1m"
#define ANSI_ESC_BOLD "\033[1m"
#define ANSI_ESC_RESET "\033[0m"
#define ANSI_RED(STR)     ANSI_ESC_RED     STR ANSI_ESC_RESET
#define ANSI_GREEN(STR)   ANSI_ESC_GREEN   STR ANSI_ESC_RESET
#define ANSI_YELLOW(STR)  ANSI_ESC_YELLOW  STR ANSI_ESC_RESET
#define ANSI_BLUE(STR)    ANSI_ESC_BLUE    STR ANSI_ESC_RESET
#define ANSI_MAGENTA(STR) ANSI_ESC_MAGENTA STR ANSI_ESC_RESET
#define ANSI_CYAN(STR)    ANSI_ESC_CYAN    STR ANSI_ESC_RESET
#define ANSI_BOLD(STR)    ANSI_ESC_BOLD    STR ANSI_ESC_RESET
#define ERL_ERROR ANSI_RED("ERROR")
#define ERL_WARNING ANSI_MAGENTA("WARNING")
/** @} */

#ifdef __cplusplus
}
#endif

#endif /*INC_errlog_H*/
