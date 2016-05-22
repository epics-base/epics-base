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

#ifndef casEventRegistryh
#define casEventRegistryh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casEventRegistryh
#   undef epicsExportSharedSymbols
#endif

#include "tsSLList.h"

#ifdef epicsExportSharedSymbols_casEventRegistryh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casEventMask.h"

class casEventMaskEntry : public tsSLNode < casEventMaskEntry >,
	public casEventMask, public stringId {
public:
	casEventMaskEntry (casEventRegistry &regIn,
	    casEventMask maskIn, const char *pName);
	virtual ~casEventMaskEntry();
	void show (unsigned level) const;

	virtual void destroy();
private:
	casEventRegistry &reg;
	casEventMaskEntry ( const casEventMaskEntry & );
	casEventMaskEntry & operator = ( const casEventMaskEntry & );
};

class casEventRegistry : 
    private resTable < casEventMaskEntry, stringId > {
    friend class casEventMaskEntry;
public:
    casEventRegistry () : maskBitAllocator ( 0 ) {}
    virtual ~casEventRegistry();
    casEventMask registerEvent ( const char * pName );
    void show ( unsigned level ) const;
private:
    unsigned maskBitAllocator;

    casEventMask maskAllocator();
	casEventRegistry ( const casEventRegistry & );
	casEventRegistry & operator = ( const casEventRegistry & );
};

#endif // casEventRegistryh
