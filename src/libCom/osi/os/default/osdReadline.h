/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef _OSD_READLINE_H_
#define _OSD_READLINE_H_

/*
 * Default version of osdReadline
 * Don't use readline library, but do provide epicsReadline routines
 *
 * This wastes a few hundred bytes of memory if you're not using iocsh.
 * If that many bytes is really important to you you can save it by
 * going to the os-dependent directory and creating an osdReadline.h
 * which defines neither of the following macros.
 *
 */

/* #define IOCSH_REAL_READLINE */
#define IOCSH_FAKE_READLINE

#endif /* _OSD_READLINE_H_ */
