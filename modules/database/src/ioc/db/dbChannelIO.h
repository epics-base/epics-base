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
 *
 * NOTES:
 * 1) This interface is preliminary and will change in the future
 */

#ifndef dbChannelIOh
#define dbChannelIOh

#ifdef epicsExportSharedSymbols
#   define dbChannelIOh_restore_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "compilerDependencies.h"

#ifdef dbChannelIOh_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

class dbChannelIO : public cacChannel, public dbContextPrivateListOfIO {
public:
    dbChannelIO (
        epicsMutex &, cacChannelNotify &,
        dbChannel *, dbContext & );
    void destructor (
        CallbackGuard &,
        epicsGuard < epicsMutex > & );
    void destroy (
        CallbackGuard &,
        epicsGuard < epicsMutex > & mutualExclusionGuard );
    void callReadNotify (
        epicsGuard < epicsMutex > &,
        unsigned type, unsigned long count,
        cacReadNotify & notify );
    void callStateNotify (
        unsigned type, unsigned long count,
        const struct db_field_log * pfl, cacStateNotify & notify );
    void show (
        epicsGuard < epicsMutex > &, unsigned level ) const;
    unsigned getName (
        epicsGuard < epicsMutex > &,
        char * pBuf, unsigned bufLen ) const throw ();
    const char * pName (
        epicsGuard < epicsMutex > & ) const throw ();
    void * operator new ( size_t size,
        tsFreeList < dbChannelIO, 256, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *,
        tsFreeList < dbChannelIO, 256, epicsMutexNOOP > & ))
protected:
    ~dbChannelIO ();
private:
    epicsMutex & mutex;
    dbContext & serviceIO;
    dbChannel * dbch;

    void initiateConnect (
        epicsGuard < epicsMutex > & );
    unsigned requestMessageBytesPending (
        epicsGuard < epicsMutex > & );
    void flush (
        epicsGuard < epicsMutex > & );
    ioStatus read (
        epicsGuard < epicsMutex > &,
        unsigned type, unsigned long count,
        cacReadNotify &, ioid * );
    void write (
        epicsGuard < epicsMutex > &,
        unsigned type, unsigned long count,
        const void * pvalue );
    ioStatus write (
        epicsGuard < epicsMutex > &,
        unsigned type, unsigned long count,
        const void * pvalue, cacWriteNotify &, ioid * );
    void subscribe (
        epicsGuard < epicsMutex > &,
        unsigned type, unsigned long count,
        unsigned mask, cacStateNotify &notify, ioid * );
    void ioCancel (
        CallbackGuard &,
        epicsGuard < epicsMutex > &,
        const ioid & );
    void ioShow (
        epicsGuard < epicsMutex > &,
        const ioid &, unsigned level ) const;
    short nativeType (
        epicsGuard < epicsMutex > & ) const;
    unsigned long nativeElementCount (
        epicsGuard < epicsMutex > & ) const;
    dbChannelIO ( const dbChannelIO & );
    dbChannelIO & operator = ( const dbChannelIO & );
    void operator delete ( void * );
};

inline void dbChannelIO::callReadNotify (
    epicsGuard < epicsMutex > & guard, unsigned type, unsigned long count,
    cacReadNotify & notify )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->serviceIO.callReadNotify ( guard, this->dbch, type, count, notify );
}

inline void dbChannelIO::callStateNotify ( unsigned type, unsigned long count,
        const struct db_field_log *pfl, cacStateNotify &notify )
{
    this->serviceIO.callStateNotify ( this->dbch, type, count, pfl, notify );
}


#endif // dbChannelIOh

