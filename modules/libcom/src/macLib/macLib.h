/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file macLib.h
 * \brief Text macro substitution routines
 * \author William Lupton, W. M. Keck Observatory
 *
 * This general purpose macro substitution library
 * is used for all macro substitutions in EPICS Base.
 *
 * Most routines return 0 (OK) on success, -1 (ERROR) on failure,
 * or small positive values for extra info.
 * The macGetValue() and macExpandString() routines depart from this
 * and return information both on success / failure and on value length.
 * Errors and warnings are reported using errlogPrintf().
 */

#ifndef INCmacLibH
#define INCmacLibH

#include "ellLib.h"
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Maximum size of a macro name or value
 */
#define MAC_SIZE 256

/** \brief Macro substitution context, for use by macLib routines only.
 *
 * An application may have multiple active contexts if desired.
 */
typedef struct {
    long        magic;          /**< \brief magic number (used for authentication) */
    int         dirty;          /**< \brief values need expanding from raw values? */
    int         level;          /**< \brief scoping level */
    int         debug;          /**< \brief debugging level */
    ELLLIST     list;           /**< \brief macro name / value list */
    int         flags;          /**< \brief operating mode flags */
} MAC_HANDLE;

/** \name Core Library
 *  The core library provides a minimal set of basic operations.
 *  @{
 */

/**
 * \brief Creates a new macro substitution context.
 * \return 0 = OK; <0 = ERROR
 */
LIBCOM_API long
epicsStdCall macCreateHandle(
    MAC_HANDLE  **handle,       /**< pointer to variable to receive pointer
                                  to new macro substitution context */

    const char * pairs[]        /**< pointer to NULL-terminated array of
                                  {name,value} pair strings. A NULL
                                  value implies undefined; a NULL \c pairs
                                  argument implies no macros. */
);
/**
 * \brief Disable or enable warning messages.
 *
 * The macExpandString() routine prints warnings when it cant expand a macro.
 * This routine can be used to silence those warnings. A non zero value will
 * suppress the warning messages from subsequent library routines given the
 * same \c handle.
 */
LIBCOM_API void
epicsStdCall macSuppressWarning(
    MAC_HANDLE  *handle,        /**< opaque handle */

    int         falseTrue       /**< 0 means issue, 1 means suppress*/
);

/**
 * \brief Expand a string which may contain macro references.
 * \return Returns the length of the expanded string, <0 if any macro are
 * undefined
 *
 * This routine parses the \c src string looking for macro references and
 * passes any it finds to macGetValue() for translation.
 *
 * \note The return value is similar to that of macGetValue(). Its absolute
 * value is the number of characters copied to \c dest. If the return value
 * is negative, at least one undefined macro was left unexpanded.
 */
LIBCOM_API long
epicsStdCall macExpandString(
    MAC_HANDLE  *handle,        /**< opaque handle */

    const char  *src,           /**< source string */

    char        *dest,          /**< destination string */

    long        capacity        /**< capacity of destination buffer (dest) */
);

/**
 * \brief Sets the value of a specific macro.
 * \return Returns the length of the value string.
 * \note If \c value is NULL, all instances of \c name are undefined at
 * all scoping levels (the named macro doesn't have to exist in this case).
 * Macros referenced in \c value need not be defined at this point.
 */
LIBCOM_API long
epicsStdCall macPutValue(
    MAC_HANDLE  *handle,        /**< opaque handle */

    const char  *name,          /**< macro name */

    const char  *value          /**< macro value */
);

/**
 * \brief Returns the value of a macro
 * \return Returns the length of the value string, <0 if undefined
 *
 * \c value will be zero-terminated if the length of the value is less than
 * \c capacity. The return value is the number of characters copied to
 * \c value (see below for behavior if the macro is undefined). If \c capacity
 * is zero, no characters will be copied to \c value (which may be NULL)
 * and the call can be used to check whether the macro is defined.
 *
 * \note Truncation of the value is not reported, applications should assume
 * that truncation has occurred if the return value is equal to capacity.
 *
 * If the macro is undefined, the macro reference will be returned in
 * the value string (if permitted by maxlen) and the function value will
 * be _minus_ the number of characters copied. Note that treatment of
 * \c capacity is intended to be consistent with the strncpy() routine.
 *
 * If the value contains macro references, these references will be
 * expanded recursively. This expansion will detect a direct or indirect
 * self reference.
 *
 * Macro references begin with a "$" immediately followed by either a
 * "(" or a "{" character. The macro name comes next, and may optionally
 * be succeeded by an "=" and a default value, which will be returned if
 * the named macro is undefined at the moment of expansion. A reference
 * is terminated by the matching ")" or "}" character.
 */
LIBCOM_API long
epicsStdCall macGetValue(
    MAC_HANDLE  *handle,        /**< opaque handle */

    const char  *name,          /**< macro name or reference */

    char        *value,         /**< string to receive macro value or name
                                argument if macro is undefined */

    long        capacity        /**< capacity of destination buffer (value) */
);
/**
 * \brief Marks a handle invalid, and frees all storage associated with it
 * \return 0 = OK; <0 = ERROR
 * \note Note that this does not free any strings into which macro values have
 *  been returned. Macro values are always returned into strings which
 *  were pre-allocated by the caller.
 */
LIBCOM_API long
epicsStdCall macDeleteHandle(
    MAC_HANDLE  *handle         /**< opaque handle */
);
/**
 * \brief Marks the start of a new scoping level
 * \return 0 = OK; <0 = ERROR
 *
 * Marks all macro definitions added after this call as belonging
 * to another scope. These macros will be lost on a macPopScope()
 * call and those at the current scope will be re-instated.
 */
LIBCOM_API long
epicsStdCall macPushScope(
    MAC_HANDLE  *handle         /**< opaque handle */
);
/**
 * \brief Retrieve the last pushed scope (like stack operations)
 * \return 0 = OK; <0 = ERROR
 *
 * See macPushScope()
 */
LIBCOM_API long
epicsStdCall macPopScope(
    MAC_HANDLE  *handle         /**< opaque handle */
);
/**
 * \brief Reports details of current definitions
 * \return 0 = OK; <0 = ERROR
 * This sends details of current definitions to standard output,
 * and is intended purely for debugging purposes.
 */
LIBCOM_API long
epicsStdCall macReportMacros(
    MAC_HANDLE  *handle         /**< opaque handle */
);
/** @} */

/** \name Utility Library
 * These convenience functions are intended for applications to use and
 * provide a more convenient interface for some purposes.
 * @{
 */

/**
 * \brief Parse macro definitions into an array of {name, value} pairs.
 * \return Number of macros found; <0 = ERROR
 *
 * This takes a set of macro definitions in "a=xxx,b=yyy" format and
 * converts them into an array of pointers to character strings which
 * are, in order, "first name", "first value", "second name", "second
 * value" etc. The array is terminated with two NULL pointers and all
 * storage is allocated contiguously so that it can be freed with a
 * single call to free().
 *
 * This routine is independent of any handle and provides a generally
 * useful service which may be used elsewhere. Any macro references in
 * values are not expanded at this point since the referenced macros may
 * be defined or redefined before the macro actually has to be
 * translated.
 *
 * Shell-style escapes and quotes are supported, as are things like
 * "A=B,B=$(C$(A)),CA=CA,CB=CB" (sets B to "CB"). White space is
 * significant within values but ignored elsewhere (i.e. surrounding "="
 * and "," characters).
 *
 * The function returns the number of definitions encountered, or -1 if
 * the supplied string is invalid.
 */
LIBCOM_API long
epicsStdCall macParseDefns(
    MAC_HANDLE  *handle,        /**< opaque handle; may be NULL if debug
                                messages are not required. */

    const char  *defns,         /**< macro definitions in "a=xxx,b=yyy"
                                format */

    char        **pairs[]       /**< address of variable to receive pointer
                                to NULL-terminated array of {name,
                                value} pair strings; all storage is
                                allocated contiguously */
);

/**
 * \brief Install set of {name, value} pairs as definitions
 * \return Number of macros defined; <0 = ERROR
 *
 * This takes an array of pairs as defined above and installs them as
 * definitions by calling macPutValue(). The pairs array is terminated
 * by a NULL pointer.
 */
LIBCOM_API long
epicsStdCall macInstallMacros(
    MAC_HANDLE  *handle,        /**< opaque handle */

    char        *pairs[]        /**< pointer to NULL-terminated array of
                                {name,value} pair strings; a NULL
                                value implies undefined; a NULL
                                argument implies no macros */
);

/**
 * \brief Expand environment variables in a string.
 * \return Expanded string; NULL if any undefined macros were used.
 *
 * This routine expands a string which may contain macros that are
 * environment variables. It parses the string looking for such
 * references and passes them to macGetValue() for translation. It uses
 * malloc() to allocate space for the expanded string and returns a
 * pointer to this null-terminated string. It returns NULL if the source
 * string contains any undefined references.
 */
LIBCOM_API char *
epicsStdCall macEnvExpand(
    const char *str             /**< string to be expanded */
);

/**
 * \brief Expands macros and environment variables in a string.
 * \return Expanded string; NULL if any undefined macros were used.
 *
 * This routine is similar to macEnvExpand() but allows an optional handle
 * to be passed in that may contain additional macro definitions.
 * These macros are appended to the set of macros from environment
 * variables when expanding the string.
 */
LIBCOM_API char *
epicsStdCall macDefExpand(
    const char *str,            /**< string to be expanded */
    MAC_HANDLE *macros          /**< opaque handle; may be NULL if only
                                environment variables are to be used */
);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /*INCmacLibH*/
