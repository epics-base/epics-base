

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
    comQueSend ( wireSendAdapter & );
    ~comQueSend ();
    void clear ();
    void beginMsg ();
    void commitMsg ();
    unsigned occupiedBytes () const;
    bool flushEarlyThreshold ( unsigned nBytesThisMsg ) const;
    bool flushBlockThreshold ( unsigned nBytesThisMsg ) const;
    bool dbr_type_ok ( unsigned type );
    void pushUInt16 ( const ca_uint16_t value );
    void pushUInt32 ( const ca_uint32_t value );
    void pushFloat32 ( const ca_float32_t value );
    void pushString ( const char *pVal, unsigned nChar );
    void insertRequestHeader (
        ca_uint16_t request, ca_uint32_t payloadSize, 
        ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
        ca_uint32_t requestDependent, bool v49Ok );
    void insertRequestWithPayLoad (
        ca_uint16_t request, unsigned dataType, ca_uint32_t nElem, 
        ca_uint32_t cid, ca_uint32_t requestDependent, const void * pPayload,
        bool v49Ok );
    void push_dbr_type ( unsigned type, const void *pVal, unsigned nElem );
    comBuf * popNextComBufToSend ();
private:
    tsDLList < comBuf > bufs;
    tsDLIterBD < comBuf > pFirstUncommited;
    wireSendAdapter & wire;
    unsigned nBytesPending;
    void copy_dbr_string ( const void *pValue, unsigned nElem );
    void copy_dbr_short ( const void *pValue, unsigned nElem );
    void copy_dbr_float ( const void *pValue, unsigned nElem );
    void copy_dbr_char ( const void *pValue, unsigned nElem );
    void copy_dbr_long ( const void *pValue, unsigned nElem );
    void copy_dbr_double ( const void *pValue, unsigned nElem );
    typedef void ( comQueSend::*copyFunc_t ) (  
        const void *pValue, unsigned nElem );
    static const copyFunc_t dbrCopyVector [39];

    void clearUncommitted ();

    //
    // visual C++ version 6.0 does not allow out of 
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
            comBuf * pComBuf = new comBuf;
            unsigned nNew = pComBuf->push ( &pVal[nCopied], nElem - nCopied );
            nCopied += nNew;
            this->bufs.add ( *pComBuf );
            if ( ! this->pFirstUncommited.valid() ) {
                this->pFirstUncommited = this->bufs.lastIter ();
            }
        }
    }

    //
    // visual C++ version 6.0 does not allow out of 
    // class member template function definition
    //
    template < class T >
    inline void push ( const T & val )
    {
        comBuf * pComBuf = this->bufs.last ();
        if ( pComBuf ) {
            if ( pComBuf->push ( val ) ) {
                return;
            }
        }
        pComBuf = new comBuf;
        assert ( pComBuf->push ( val ) );
        this->bufs.add ( *pComBuf );
        if ( ! this->pFirstUncommited.valid() ) {
            this->pFirstUncommited = this->bufs.lastIter ();
        }
    }
	comQueSend ( const comQueSend & );
	comQueSend & operator = ( const comQueSend & );
};

extern const char cacNillBytes[];

inline bool comQueSend::dbr_type_ok ( unsigned type )
{
    if ( type >= ( sizeof ( this->dbrCopyVector ) / sizeof ( this->dbrCopyVector[0] )  ) ) {
        return false;
    }
    if ( ! this->dbrCopyVector [type] ) {
        return false;
    }
    return true;
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

// it is assumed that dbr_type_ok() was called prior to calling this routine
// to check the type code
inline void comQueSend::push_dbr_type ( unsigned type, const void *pVal, unsigned nElem )
{
    ( this->*dbrCopyVector [type] ) ( pVal, nElem );
}

inline unsigned comQueSend::occupiedBytes () const
{
    return this->nBytesPending;
}

inline bool comQueSend::flushBlockThreshold ( unsigned nBytesThisMsg ) const
{
    return ( this->nBytesPending + nBytesThisMsg > 16 * comBuf::capacityBytes () );
}

inline bool comQueSend::flushEarlyThreshold ( unsigned nBytesThisMsg ) const
{
    return ( this->nBytesPending + nBytesThisMsg > 4 * comBuf::capacityBytes () );
}

inline void comQueSend::beginMsg ()
{
    if ( this->pFirstUncommited.valid() ) {
        this->clearUncommitted ();
    }
    this->pFirstUncommited = this->bufs.lastIter ();
}

#endif // ifndef comQueSendh
