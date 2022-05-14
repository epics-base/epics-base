/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 *  505 665 1831
 */

#include <unistd.h>
#include <ieeefp.h>

/*
 * Some systems fail to provide prototypes of these functions.
 * Others provide different prototypes.
 * There seems to be no way to handle this automatically, so
 * if you get compile errors, just make the appropriate changes here.
 */

#ifdef __cplusplus
extern "C" {
#endif

int putenv (char *);
char *strdup (const char *);
char *strtok_r(char*, const char*, char**);

int snprintf(char *str, size_t size, const char *format, ...);
#include <stdarg.h>
int vsnprintf(char *str, size_t size, const char *format, va_list ap);


#ifdef __cplusplus
}
#endif
