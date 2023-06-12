/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Jeffrey O. Hill
 */

#ifndef epicsSingleton_h
#define epicsSingleton_h

#include <new>
#include <cstddef>

#include "libComAPI.h"
#include "epicsAssert.h"

class SingletonUntyped {
public:
#if __cplusplus>=201103L
    constexpr
#endif
    SingletonUntyped () :_pInstance ( 0 ), _refCount ( 0 ) {}
#   if 0
    ~SingletonUntyped () {
        // we don't assert fail on non-zero _refCount
        // and or non nill _pInstance here because this
        // is designed to tolerate situations where
        // file scope epicsSingleton objects (which
        // theoretically don't have storage lifespan
        // issues) are deleted in a non-deterministic
        // order
        assert ( _refCount == 0 );
        assert ( _pInstance == 0 );
    }
#   endif
    typedef void * ( * PBuild ) ();
    LIBCOM_API void incrRefCount ( PBuild );
    typedef void ( * PDestroy ) ( void * );
    LIBCOM_API void decrRefCount ( PDestroy );
    inline void * pInstance () const { return _pInstance; }
private:
    void * _pInstance;
    std :: size_t _refCount;
    SingletonUntyped ( const SingletonUntyped & );
    SingletonUntyped & operator = ( const SingletonUntyped & );
};

// This class exists for the purpose of avoiding file scope
// object chicken and egg problems. It implements thread safe
// lazy initialization. To avoid locking overhead retain a
// copy of the epicsSingleton :: reference for future use.
template < class TYPE >
class epicsSingleton {
public:
    class reference {
    public:
        reference ( epicsSingleton & es)
            :_pSingleton ( & es )
        {
            es._singletonUntyped.
                incrRefCount ( & epicsSingleton < TYPE > :: _build );
        }
        reference ( const reference & ref)
            :_pSingleton ( ref._pSingleton )
        {
            assert ( _pSingleton );
            _pSingleton->_singletonUntyped.
                incrRefCount ( & epicsSingleton < TYPE > :: _build );
        }
        ~reference () {
            assert ( _pSingleton );
            _pSingleton->_singletonUntyped.
                decrRefCount ( & epicsSingleton < TYPE > :: _destroy );
        }
        // this somewhat convoluted reference of the return
        // type ref through the epicsSingleton template is
        // required for the archaic Tornado gnu compiler
        typename epicsSingleton < TYPE > :: reference &
            operator = ( const reference & ref) {
            if ( _pSingleton != ref._pSingleton ) {
                assert ( _pSingleton );
                _pSingleton->_singletonUntyped.
                    decrRefCount ( epicsSingleton < TYPE > :: _destroy );
                _pSingleton = ref._pSingleton;
                assert ( _pSingleton );
                _pSingleton->_singletonUntyped.
                    incrRefCount ( & epicsSingleton < TYPE > :: _build );
            }
            return *this;
        }
        TYPE * operator -> () {
            assert ( _pSingleton );
            return reinterpret_cast < TYPE * >
                    ( _pSingleton->_singletonUntyped.pInstance () );
        }
        const TYPE * operator -> () const {
            assert ( _pSingleton );
            return reinterpret_cast < const TYPE * >
                    ( _pSingleton->_singletonUntyped.pInstance () );
        }
        TYPE & operator * () {
            return * this->operator -> ();
        }
        const TYPE & operator * () const {
            return * this->operator -> ();
        }
    private:
        epicsSingleton * _pSingleton;
    };
    friend class reference;
#if __cplusplus>=201103L
    constexpr
#endif
    epicsSingleton () {}
    // mutex lock/unlock pair overhead incurred
    // when either of these are called
    reference getReference () {
        return reference ( * this );
    }
    const reference getReference () const {
        epicsSingleton < TYPE > * pConstCastAway =
            const_cast < epicsSingleton < TYPE > * > ( this );
        return pConstCastAway->getReference ();
    }
private:
    SingletonUntyped _singletonUntyped;
    static void * _build () { return new TYPE (); }
    static void _destroy ( void * pDestroyTypeless) {
        TYPE * pDestroy =
            reinterpret_cast < TYPE * > ( pDestroyTypeless );
        delete pDestroy;
    }
    epicsSingleton ( const epicsSingleton & );
    epicsSingleton & operator = ( const epicsSingleton & );
};

#endif // epicsSingleton_h

