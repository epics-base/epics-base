/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * $Id$
 *
 */

#ifndef osdTimeh
#define osdTimeh

#include <sys/types.h>

/* from win32 */
typedef u_int32_t DWORD;
typedef struct _FILETIME {
   DWORD dwLowDateTime;   /* low 32 bits  */
   DWORD dwHighDateTime;  /* high 32 bits */
} FILETIME;

#endif /* ifndef osdTimeh */
