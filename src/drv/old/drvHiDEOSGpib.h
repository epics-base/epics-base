/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef EPICS_DRVHIDEOSGPIB_H
#define EPICS_DRVHIDEOSGPIB_H

typedef int (*GPIB_HIDEOS_WRITECMD_FUNC)(void *td, char *Buf, unsigned long BufLen, unsigned long TimeOut);
typedef int (*GPIB_HIDEOS_WRITE_FUNC)(void *td, char *Buf, unsigned long BufLen, unsigned long devAddr, unsigned long TimeOut);
typedef void *(*GPIB_HIDEOS_INIT_FUNC)(int BoardId, char *TaskName);
typedef int (*GPIB_HIDEOS_READ_FUNC)(void *td, char *Buf, unsigned long BufLen, unsigned long *Actual, unsigned long devAddr, unsigned long TimeOut, long Eos);
typedef int (*GPIB_HIDEOS_WRITEREAD_FUNC)(void *td, char *WBuf, unsigned long WBufLen, char *RBuf, unsigned long RBufLen, unsigned long *RActual, unsigned long devAddr, unsigned long TimeOut, long Eos);


#endif
