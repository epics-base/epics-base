/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSymb.h - Header for vxWorks global var device support routines */

/* $Id$
 *
 * Author:	Andrew Johnson (RGO)
 * Date:	11 October 1996
 */

/* This is the device private structure */

struct vxSym {
    void **ppvar;
    void *pvar;
    long index;
};


/* This is the call to create it */

extern int devSymbFind(char *name, struct link *plink, void *pdpvt);
