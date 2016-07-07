
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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef casAsyncIOIh
#define casAsyncIOIh

#include "casEvent.h"
#include "caHdrLargeArray.h"
#include "casCoreClient.h"

class casAsyncIOI : 
    public tsDLNode < casAsyncIOI >, 
    public casEvent {
public:
	casAsyncIOI ( const casCtx & ctx );
	epicsShareFunc virtual ~casAsyncIOI ();
	caServer * getCAS () const;
    virtual bool oneShotReadOP () const;
    void removeFromEventQueue ();
protected:
	caStatus insertEventQueue ();
	casCoreClient & client;
private:
	bool inTheEventQueue;
	bool posted;
	bool ioComplete;

	//
	// casEvent virtual call back function
	// (called when IO completion event reaches top of event queue)
	//
	epicsShareFunc caStatus cbFunc ( 
        casCoreClient &, 
        epicsGuard < casClientMutex > &,
        epicsGuard < evSysMutex > & );

    //
    // derived class specific call back
	// (called when IO completion event reaches top of event queue)
    //
    epicsShareFunc virtual caStatus cbFuncAsyncIO (
        epicsGuard < casClientMutex > & ) = 0;

	casAsyncIOI ( const casAsyncIOI & );
	casAsyncIOI & operator = ( const casAsyncIOI & );
};

inline void casAsyncIOI::removeFromEventQueue ()
{
    this->client.removeFromEventQueue ( *this, this->inTheEventQueue );
}

#endif // casAsyncIOIh
