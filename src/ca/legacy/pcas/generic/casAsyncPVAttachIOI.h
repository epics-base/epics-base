
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

#ifndef casAsyncPVAttachIOIh
#define casAsyncPVAttachIOIh

#include "casAsyncIOI.h"

class casAsyncPVAttachIOI : public casAsyncIOI {
public:
	casAsyncPVAttachIOI ( casAsyncPVAttachIO &, const casCtx & ctx );
	~casAsyncPVAttachIOI (); 
	caStatus postIOCompletion ( const pvAttachReturn & retValIn );
	caServer *getCAS () const;
private:
	caHdrLargeArray const msg;
    class casAsyncPVAttachIO & asyncPVAttachIO;
	pvAttachReturn retVal;

	caStatus cbFuncAsyncIO ( epicsGuard < casClientMutex > & );
	casAsyncPVAttachIOI ( const casAsyncPVAttachIOI & );
	casAsyncPVAttachIOI & operator = ( const casAsyncPVAttachIOI & );
};

inline casAsyncPVAttachIOI::~casAsyncPVAttachIOI ()
{
    this->asyncPVAttachIO.serverInitiatedDestroy ();
}

#endif // casAsyncPVAttachIOIh
