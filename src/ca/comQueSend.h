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
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef comQueSendh  
#define comQueSendh

#include <new> 

#include "tsDLList.h"
#include "comBuf.h"

//
// Notes.
// o calling popNextComBufToSend() will clear 
// any uncommitted bytes
//
class comQueSend {
public:
    comQueSend ( wireSendAdapter &, comBufMemoryManager & ) epics_throws (());
    ~comQueSend () epics_throws (());
    void clear () epics_throws (());
    void beginMsg () epics_throws (());
    void commitMsg () epics_throws (());
    unsigned occupiedBytes () const epics_throws (());
    bool flushEarlyThreshold ( unsigned nBytesThisMsg ) const epics_throws (());
    bool flushBlockThreshold ( unsigned nBytesThisMsg ) const epics_throws (());
    bool dbr_type_ok ( unsigned type ) epics_throws (());
    void pushUInt16 ( const ca_uint16_t value ) epics_throws (());
    void pushUInt32 ( const ca_uint32_t value ) epics_throws (());
    void pushFloat32 ( const ca_float32_t value ) epics_throws (());
    void pushString ( const char *pVal, unsigned nChar ) epics_throws (());
    void insertRequestHeader (
        ca_uint16_t request, ca_uint32_t payloadSize, 
        ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
        ca_uint32_t requestDependent, bool v49Ok ) 
            epics_throws (( cacChannel::outOfBounds ));
    void insertRequestWithPayLoad (
        ca_uint16_t request, unsigned dataType, ca_uint32_t nElem, 
        ca_uint32_t cid, ca_uint32_t requestDependent, 
        const void * pPayload, bool v49Ok ) 
            epics_throws (( cacChannel::outOfBounds ));
    void push_dbr_type ( unsigned type, const void *pVal, unsigned nElem ) epics_throws (());
    comBuf * popNextComBufToSend () epics_throws (());
private:
    comBufMemoryManager & comBufMemMgr;
    tsDLList < comBuf > bufs;
    tsDLIter < comBuf > pFirstUncommited;
    wireSendAdapter & wire;
    unsigned nBytesPending;
    void copy_dbr_string ( const void *pValue, unsigned nElem ) epics_throws (());
    void copy_dbr_short ( const void *pValue, unsigned nElem ) epics_throws (());
    void copy_dbr_float ( const void *pValue, unsigned nElem ) epics_throws (());
    void copy_dbr_char ( const void *pValue, unsigned nElem ) epics_throws (());
    void copy_dbr_long ( const void *pValue, unsigned nElem ) epics_throws (());
    void copy_dbr_double ( const void *pValue, unsigned nElem ) epics_throws (());
    void pushComBuf ( comBuf & ) epics_throws (());
    typedef void ( comQueSend::*copyFunc_t ) (  
        const void *pValue, unsigned nElem );
    static const copyFunc_t dbrCopyVector [39];

    void clearUncommitted () epics_throws (());

    //
    // visual C++ versions 6 & 7 do not allow out of 
    // class member template function definition
    //
    template < class T >
    inline void push ( const T *pVal, const unsigned nElem ) epics_throws (())
    {
        comBuf * pLastBuf = this->bufs.last ();
        unsigned nCopied;
        if ( pLastBuf ) {
            nCopied = pLastBuf->push ( pVal, nElem );
        }
        else {
            nCopied = 0u;
        }
        while ( nElem > nCopied ) {
            comBuf * pComBuf = new ( this->comBufMemMgr ) comBuf;
            unsigned nNew = pComBuf->push 
                        ( &pVal[nCopied], nElem - nCopied );
            nCopied += nNew;
            this->pushComBuf ( *pComBuf );
        }
    }

    //
    // visual C++ versions 6 and 7 do not allow out of 
    // class member template function definition
    //
    template < class T >
    inline void push ( const T & val ) epics_throws (())
    {
        comBuf * pComBuf = this->bufs.last ();
        if ( pComBuf && pComBuf->push ( val ) ) {
            return;
        }
        pComBuf = new ( this->comBufMemMgr ) comBuf;
        assert ( pComBuf->push ( val ) );
        this->pushComBuf ( *pComBuf );
    }

	comQueSend ( const comQueSend & ) epics_throws (());
	comQueSend & operator = ( const comQueSend & ) epics_throws (());
};

extern const char cacNillBytes[];

inline bool comQueSend::dbr_type_ok ( unsigned type ) epics_throws (())
{
    if ( type >= ( sizeof ( this->dbrCopyVector ) / sizeof ( this->dbrCopyVector[0] )  ) ) {
        return false;
    }
    if ( ! this->dbrCopyVector [type] ) {
        return false;
    }
    return true;
}

inline void comQueSend::pushUInt16 ( const ca_uint16_t value ) epics_throws (())
{
    this->push ( value );
}

inline void comQueSend::pushUInt32 ( const ca_uint32_t value ) epics_throws (())
{
    this->push ( value );
}

inline void comQueSend::pushFloat32 ( const ca_float32_t value ) epics_throws (())
{
    this->push ( value );
}

inline void comQueSend::pushString ( const char *pVal, unsigned nChar ) epics_throws (())
{
    this->push ( pVal, nChar );
}

// it is assumed that dbr_type_ok() was called prior to calling this routine
// to check the type code
inline void comQueSend::push_dbr_type ( unsigned type, const void *pVal, unsigned nElem ) epics_throws (())
{
    ( this->*dbrCopyVector [type] ) ( pVal, nElem );
}

inline void comQueSend::pushComBuf ( comBuf & cb ) epics_throws (())
{
    this->bufs.add ( cb );
    if ( ! this->pFirstUncommited.valid() ) {
        this->pFirstUncommited = this->bufs.lastIter ();
    }
}

inline unsigned comQueSend::occupiedBytes () const epics_throws (())
{
    return this->nBytesPending;
}

inline bool comQueSend::flushBlockThreshold ( unsigned nBytesThisMsg ) const epics_throws (())
{
    return ( this->nBytesPending + nBytesThisMsg > 16 * comBuf::capacityBytes () );
}

inline bool comQueSend::flushEarlyThreshold ( unsigned nBytesThisMsg ) const epics_throws (())
{
    return ( this->nBytesPending + nBytesThisMsg > 4 * comBuf::capacityBytes () );
}

inline void comQueSend::beginMsg () epics_throws (())
{
    if ( this->pFirstUncommited.valid() ) {
        this->clearUncommitted ();
    }
    this->pFirstUncommited = this->bufs.lastIter ();
}

#endif // ifndef comQueSendh
