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
 *	Author Jeff Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef epicsOnceh
#define epicsOnceh

#include "shareLib.h"

class epicsOnceNotify {
public:
    epicsShareFunc virtual ~epicsOnceNotify ();
    virtual void initialize () = 0;
};

class epicsOnce {
public:
    epicsShareFunc static epicsOnce & create ( epicsOnceNotify & notifyIn );
    virtual ~epicsOnce (); // use destroy
    virtual void once () = 0; // run notifyIn.initialize() once only
    virtual void destroy () = 0; // destroy this object
};

#endif // epicsOnceh

