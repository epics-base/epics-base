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

/*
 * Linux needs this dummy include file since the POSIX version
 * causes `struct timespec' to be defined in more than one place.
 */

#include <inttypes.h>

/* from win32 */
typedef uint32_t DWORD;
typedef struct _FILETIME {
   DWORD dwLowDateTime;   /* low 32 bits  */
   DWORD dwHighDateTime;  /* high 32 bits */
} FILETIME;

#endif /* ifndef osdTimeh */

