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

#define comQueSendCopyDispatchSize 39

class epicsMutex;
template < class T > class epicsGuard;

class comQueSendMsgMinder {
public:
    comQueSendMsgMinder ( 
        class comQueSend &, epicsGuard < epicsMutex > & );
    ~comQueSendMsgMinder ();
    void commit ();
private:
    class comQueSend * pSendQue;
};

//
// Notes.
// o calling popNextComBufToSend() will clear any uncommitted bytes
//
class comQueSend {
public:
    comQueSend ( wireSendAdapter &, comBufMemoryManager & );
    ~comQueSend ();
    void clear ();
    unsigned occupiedBytes () const; 
    bool flushEarlyThreshold ( unsigned nBytesThisMsg ) const;
    bool flushBlockThreshold () const; 
    void pushUInt16 ( const ca_uint16_t value );
    void pushUInt32 ( const ca_uint32_t value );
    void pushFloat32 ( const ca_float32_t value );
    void pushString ( const char *pVal, unsigned nChar );
    void insertRequestHeader ( 
        ca_uint16_t request, ca_uint32_t payloadSize, 
        ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
        ca_uint32_t requestDependent, bool v49Ok );
    void insertRequestWithPayLoad (
        ca_uint16_t request, unsigned dataType, arrayElementCount nElem, 
        ca_uint32_t cid, ca_uint32_t requestDependent, 
        const void * pPayload, bool v49Ok );
    comBuf * popNextComBufToSend ();
private:
    comBufMemoryManager & comBufMemMgr;
    tsDLList < comBuf > bufs;
    tsDLIter < comBuf > pFirstUncommited;
    wireSendAdapter & wire;
    unsigned nBytesPending;

    typedef void ( comQueSend::*copyScalarFunc_t ) ( 
        const void * pValue );
    static const copyScalarFunc_t dbrCopyScalar [comQueSendCopyDispatchSize];
    void copy_dbr_string ( const void * pValue );
    void copy_dbr_short ( const void * pValue ); 
    void copy_dbr_float ( const void * pValue ); 
    void copy_dbr_char ( const void * pValue ); 
    void copy_dbr_long ( const void * pValue ); 
    void copy_dbr_double ( const void * pValue ); 
    void copy_dbr_invalid ( const void * pValue );

    typedef void ( comQueSend::*copyVectorFunc_t ) (  
        const void * pValue, unsigned nElem );
    static const copyVectorFunc_t dbrCopyVector [comQueSendCopyDispatchSize];
    void copy_dbr_string ( const void *pValue, unsigned nElem );
    void copy_dbr_short ( const void *pValue, unsigned nElem ); 
    void copy_dbr_float ( const void *pValue, unsigned nElem ); 
    void copy_dbr_char ( const void *pValue, unsigned nElem ); 
    void copy_dbr_long ( const void *pValue, unsigned nElem ); 
    void copy_dbr_double ( const void *pValue, unsigned nElem ); 
    void copy_dbr_invalid ( const void * pValue, unsigned nElem );

    void pushComBuf ( comBuf & ); 
    comBuf * newComBuf (); 

    void beginMsg (); 
    void commitMsg (); 
    void clearUncommitedMsg ();

    friend class comQueSendMsgMinder;

    //
    // visual C++ versions 6 & 7 do not allow out of 
    // class member template function definition
    //
    template < class T >
    inline void push ( const T *pVal, const unsigned nElem ) 
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
            comBuf * pComBuf = newComBuf ();
            nCopied += pComBuf->push 
                        ( &pVal[nCopied], nElem - nCopied );
            this->pushComBuf ( *pComBuf );
        }
    }

    //
    // visual C++ versions 6 and 7 do not allow out of 
    // class member template function definition
    //
    template < class T >
    inline void push ( const T & val ) 
    {
        comBuf * pComBuf = this->bufs.last ();
        if ( pComBuf && pComBuf->push ( val ) ) {
            return;
        }
        pComBuf = newComBuf ();
        bool success = pComBuf->push ( val );
        assert ( success );
        this->pushComBuf ( *pComBuf );
    }

    template < class T >
    inline void push ( const T * ); // disabled

    comQueSend ( const comQueSend & );
    comQueSend & operator = ( const comQueSend & );
};

extern const char cacNillBytes[];

inline comQueSendMsgMinder::comQueSendMsgMinder ( 
    class comQueSend & sendQueIn, epicsGuard < epicsMutex > & ) : 
        pSendQue ( & sendQueIn )
{
    sendQueIn.beginMsg ();
}

inline comQueSendMsgMinder::~comQueSendMsgMinder ()
{
    if ( this->pSendQue ) {
        this->pSendQue->clearUncommitedMsg ();
    }
}

inline void comQueSendMsgMinder::commit ()
{
    if ( this->pSendQue ) {
        this->pSendQue->commitMsg ();
        this->pSendQue = 0;
    }
}

inline void comQueSend::beginMsg () 
{
    this->pFirstUncommited = this->bufs.lastIter ();
}

inline void comQueSend::pushUInt16 ( const ca_uint16_t value ) 
{
    this->push ( value );
}

inline void comQueSend::pushUInt32 ( const ca_uint32_t value ) 
{
    this->push ( value );
}

inline void comQueSend::pushFloat32 ( const ca_float32_t value ) 
{
    this->push ( value );
}

inline void comQueSend::pushString ( const char *pVal, unsigned nChar ) 
{
    this->push ( pVal, nChar );
}

inline void comQueSend::pushComBuf ( comBuf & cb ) 
{
    this->bufs.add ( cb );
    if ( ! this->pFirstUncommited.valid() ) {
        this->pFirstUncommited = this->bufs.lastIter ();
    }
}

inline unsigned comQueSend::occupiedBytes () const 
{
    return this->nBytesPending;
}

inline bool comQueSend::flushBlockThreshold () const
{
    return ( this->nBytesPending > 16 * comBuf::capacityBytes () );
}

inline bool comQueSend::flushEarlyThreshold ( unsigned nBytesThisMsg ) const
{
    return ( this->nBytesPending + nBytesThisMsg > 4 * comBuf::capacityBytes () );
}

// wrapping this with a function avoids WRS T2.2 Cygnus GNU compiler bugs
inline comBuf * comQueSend::newComBuf ()
{
    return new ( this->comBufMemMgr ) comBuf;
}

#endif // ifndef comQueSendh
