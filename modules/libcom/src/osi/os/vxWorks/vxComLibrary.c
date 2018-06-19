/*vxComLibrary.c */
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*  Author: Marty Kraimer
 *  August 2003
 */

/* The purpose of this module is to force various vxWorks specific
 * modules top be loaded
 */

#include "epicsDynLink.h"
extern int logMsgToErrlog(void);
extern int veclist(int);

void vxComLibrary(int callit)
{
    if(callit) symFindByNameEPICS(0,0,0,0);
    if(callit) logMsgToErrlog();
    if(callit) veclist(0);
}
