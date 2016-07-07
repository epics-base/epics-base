
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

#ifndef casAsyncReadIOIh
#define casAsyncReadIOIh

#include "casAsyncIOI.h"
#include "casdef.h"

class gdd;

class casAsyncReadIOI : public casAsyncIOI {
public:
	casAsyncReadIOI ( casAsyncReadIO &, const casCtx & ctx );
    ~casAsyncReadIOI ();
	caStatus postIOCompletion ( 
        caStatus completionStatusIn, const gdd &valueRead );
	caServer *getCAS () const;
private:
	caHdrLargeArray const msg;
    class casAsyncReadIO & asyncReadIO;
	class casChannelI & chan; 
	smartConstGDDPointer pDD;
	caStatus completionStatus;
    epicsShareFunc bool oneShotReadOP () const;
	epicsShareFunc caStatus cbFuncAsyncIO ( 
        epicsGuard < casClientMutex > & );
	casAsyncReadIOI ( const casAsyncReadIOI & );
	casAsyncReadIOI & operator = ( const casAsyncReadIOI & );
};

#endif // casAsyncReadIOIh
