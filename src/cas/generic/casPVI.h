
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
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef casPVIh
#define casPVIh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casPVIh
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "tsSLList.h"
#include "epicsMutex.h"
#include "caProto.h"

#ifdef epicsExportSharedSymbols_casPVIh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casdef.h"
#include "ioBlocked.h"

class chanIntfForPV;
class caServerI;
class casMonitor;

class casPVI : 
    public tsSLNode < casPVI >, // server resource table installation 
    public ioBlockedList // list of clients io blocked on this pv
{
public:
    casPVI ( casPV & );
    caServerI * getPCAS () const;
    caStatus attachToServer ( caServerI & cas );
    aitIndex nativeCount ();
	void clearOutstandingReads ( tsDLList < class casAsyncIOI > &);
    void destroyAllIO ( 
        tsDLList < casAsyncIOI > & );
    void installIO (
        tsDLList < casAsyncIOI > &, casAsyncIOI & );
    void uninstallIO ( 
        tsDLList < casAsyncIOI > &, casAsyncIOI & );
    void installChannel ( chanIntfForPV & chan );
    void removeChannel ( 
        chanIntfForPV & chan, tsDLList < casMonitor > & src,
        tsDLList < casMonitor > & dest );
    caStatus installMonitor ( 
        casMonitor & mon, tsDLList < casMonitor > & monitorList );
    casMonitor * removeMonitor ( 
        tsDLList < casMonitor > & list, ca_uint32_t clientIdIn );
    void deleteSignal ();
    void postEvent ( const casEventMask & select, const gdd & event );
    caServer * getExtServer () const;
    caStatus bestDBRType ( unsigned & dbrType );
    const gddEnumStringTable & enumStringTable () const;
    caStatus updateEnumStringTable ( casCtx & );
    void updateEnumStringTableAsyncCompletion ( const gdd & resp );
    casPV * apiPointer (); // retuns NULL if casPVI isnt a base of casPV
    void show ( unsigned level ) const;
    caStatus beginTransaction ();
    void endTransaction ();
    caStatus read ( const casCtx & ctx, gdd & prototype );
    caStatus write ( const casCtx & ctx, const gdd & value );
    casChannel * createChannel ( const casCtx & ctx,
        const char * const pUserName, const char * const pHostName );
    aitEnum bestExternalType () const;
    unsigned maxDimension () const; 
    aitIndex maxBound ( unsigned dimension ) const;
    const char * getName () const;
    static casPVI * attachPV ( casPV & );

protected:
    epicsShareFunc virtual ~casPVI (); 
private:
    mutable epicsMutex mutex;
    tsDLList < chanIntfForPV > chanList;
    gddEnumStringTable enumStrTbl;
    caServerI * pCAS;
    casPV & pv;
    unsigned nMonAttached;
    unsigned nIOAttached;

	casPVI ( const casPVI & );
	casPVI & operator = ( const casPVI & );
};

inline caServerI *casPVI::getPCAS() const
{
	return this->pCAS;
}

// CA only does 1D arrays for now 
inline aitIndex casPVI::nativeCount () 
{
	if ( this->maxDimension() == 0u ) {
		return 1u; // scalar
	}
	return this->maxBound(0u);
}

inline const gddEnumStringTable & casPVI::enumStringTable () const
{
    return this->enumStrTbl;
}

inline caStatus casPVI::beginTransaction ()
{
    return this->pv.beginTransaction ();
}

inline void casPVI::endTransaction ()
{
    this->pv.endTransaction ();
}

inline caStatus casPVI::read ( const casCtx & ctx, gdd & prototype )
{
    return this->pv.read ( ctx, prototype );
}

inline caStatus casPVI::write ( const casCtx & ctx, const gdd & value )
{
    return this->pv.write ( ctx, value );
}

inline casChannel * casPVI::createChannel ( const casCtx & ctx,
    const char * const pUserName, const char * const pHostName )
{
    return this->pv.createChannel ( ctx, pUserName, pHostName );
}

inline aitEnum casPVI::bestExternalType () const
{
    return this->pv.bestExternalType ();
}

inline unsigned casPVI::maxDimension () const
{
    return this->pv.maxDimension ();
}

inline aitIndex casPVI::maxBound ( unsigned dimension ) const
{
    return this->pv.maxBound ( dimension );
}

inline const char * casPVI::getName () const
{
    return this->pv.getName ();
}

#endif // casPVIh

