/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * $Id$
 *
 * Author: Eric Norum
 */

#ifndef osdTimeh
#define osdTimeh

#include <sys/types.h>
#include <sys/time.h>

/* from win32 */
typedef u_int32_t DWORD;
typedef struct _FILETIME {
   DWORD dwLowDateTime;   /* low 32 bits  */
   DWORD dwHighDateTime;  /* high 32 bits */
} FILETIME;

#endif /* ifndef osdTimeh */

