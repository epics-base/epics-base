
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

#ifndef casAsyncPVExistIOIh
#define casAsyncPVExistIOIh

#include "casAsyncIOI.h"

class casAsyncPVExistIOI : public casAsyncIOI  {
public:
	casAsyncPVExistIOI ( casAsyncPVExistIO &, const casCtx & ctx );
	~casAsyncPVExistIOI (); 
	caStatus postIOCompletion ( const pvExistReturn & retValIn );
	caServer * getCAS() const;
private:
	caHdrLargeArray const msg;
    class casAsyncPVExistIO & asyncPVExistIO;
	pvExistReturn retVal;
	const caNetAddr dgOutAddr;
    const ca_uint16_t protocolRevision;
    const ca_uint32_t sequenceNumber;

	caStatus cbFuncAsyncIO ( epicsGuard < casClientMutex > & );
	casAsyncPVExistIOI ( const casAsyncPVExistIOI & );
	casAsyncPVExistIOI & operator = ( const casAsyncPVExistIOI & );
};

inline casAsyncPVExistIOI::~casAsyncPVExistIOI ()
{
    this->asyncPVExistIO.serverInitiatedDestroy ();
}

#endif // casAsyncPVExistIOIh
