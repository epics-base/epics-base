/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* errInc.cc  */
/* share/epicsH $Id$ */
/* defines headers containing status codes for epics */

/* common to epics Unix and vxWorks */
#include "casdef.h"
#include "gddAppFuncTable.h"
#ifdef VXLIST
/* epics vxWorks  only*/
#endif /* VXLIST */
