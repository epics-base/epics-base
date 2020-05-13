/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * @file epicsReadline.h
 * @brief Command-line editing functions
 * @author Eric Norum
 *
 * Provides a generalized API for command line history and line-editing.
 * The implementation of this API can call GNU Readline, libtecla, and on
 * VxWorks the ledLib routines, according to the EPICS build configuration.
 */
#ifndef INC_epicsReadline_H
#define INC_epicsReadline_H

#ifdef __cplusplus
extern "C" {
#endif

#include <shareLib.h>
#include <stdio.h>
/**
 * @brief Create a command-line context
 * @param in Filehandle to read from
 * @returns Command-line context
 */
epicsShareFunc void * epicsShareAPI epicsReadlineBegin (FILE *in);
/**
 * @brief Read a line of input
 * @param prompt Prompt string
 * @param context To read from
 * @returns Line read
 */
epicsShareFunc char * epicsShareAPI epicsReadline (const char *prompt, void *context);
/**
 * @brief Destroy a command-line context
 * @param context Command-line context to destroy
 */
epicsShareFunc void   epicsShareAPI epicsReadlineEnd (void *context);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsReadline_H */
