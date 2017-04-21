/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef ipIgnoreEntryh
#define ipIgnoreEntryh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_ipIgnoreEntryh
#   undef epicsExportSharedSymbols
#endif

#include "tsSLList.h"
#include "tsFreeList.h"
#include "resourceLib.h"

#ifdef epicsExportSharedSymbols_ipIgnoreEntryh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class ipIgnoreEntry : public tsSLNode < ipIgnoreEntry > {
public:
    ipIgnoreEntry ( unsigned ipAddr );
    void show ( unsigned level ) const;
    bool operator == ( const ipIgnoreEntry & ) const;
    resTableIndex hash () const;
    void * operator new ( size_t size, 
        tsFreeList < class ipIgnoreEntry, 128 > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < class ipIgnoreEntry, 128 > & ))
private:
    unsigned ipAddr;
	ipIgnoreEntry ( const ipIgnoreEntry & );
	ipIgnoreEntry & operator = ( const ipIgnoreEntry & );
    void operator delete ( void * );
};

#endif // ipIgnoreEntryh

