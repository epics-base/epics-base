/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
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

