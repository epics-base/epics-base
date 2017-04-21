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
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef dbPutNotifyBlockerh
#define dbPutNotifyBlockerh

#ifdef epicsExportSharedSymbols
#define dbPutNotifyBlockerh_restore_epicsExportSharedSymbols
#undef epicsExportSharedSymbols
#endif

#include "tsFreeList.h"
#include "compilerDependencies.h"

#ifdef dbPutNotifyBlockerh_restore_epicsExportSharedSymbols
#define epicsExportSharedSymbols
#endif

class dbPutNotifyBlocker : public dbBaseIO {
public:
    dbPutNotifyBlocker ( epicsMutex & );
    void destructor ( CallbackGuard &, epicsGuard < epicsMutex > & );
    void initiatePutNotify ( epicsGuard < epicsMutex > &,
            cacWriteNotify &, struct dbChannel *,
            unsigned type, unsigned long count, const void * pValue );
    void cancel ( CallbackGuard &, epicsGuard < epicsMutex > & );
    void show ( epicsGuard < epicsMutex > &, unsigned level ) const;
    void show ( unsigned level ) const;
    void * operator new ( size_t size,
        tsFreeList < dbPutNotifyBlocker, 64, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *,
        tsFreeList < dbPutNotifyBlocker, 64, epicsMutexNOOP > & ))
private:
    processNotify pn;
    //
    // Include a union of all scalar types
    // including fixed length strings so
    // that in many cases we can avoid
    // allocating another buffer
    //
    union {
        dbr_string_t strval;
        dbr_short_t shrtval;
        dbr_short_t intval;
        dbr_float_t fltval;
        dbr_enum_t enmval;
        dbr_char_t charval;
        dbr_long_t longval;
        dbr_double_t doubleval;
    } dbrScalarValue;
    epicsEvent block;
    epicsMutex & mutex;
    cacWriteNotify * pNotify;
    unsigned long maxValueSize;
    // arguments for db_put_field
    void *pbuffer;
    long nRequest;
    short dbrType;
    // end arguments for db_put_field
    dbSubscriptionIO * isSubscription ();
    void expandValueBuf (
        epicsGuard < epicsMutex > &, unsigned long newSize );
    friend void putNotifyCompletion ( processNotify * ppn );
    friend int  putNotifyPut ( processNotify *ppn, notifyPutType type );
	dbPutNotifyBlocker ( const dbPutNotifyBlocker & );
	dbPutNotifyBlocker & operator = ( const dbPutNotifyBlocker & );
    virtual ~dbPutNotifyBlocker ();
    void operator delete ( void * );
};

#endif // ifndef dbPutNotifyBlockerh

