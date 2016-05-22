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
 *  Author: Jeffrey O. Hill
 */

#ifndef epicsSingleton_h
#define epicsSingleton_h

#include <new>
#include <cstddef>

#include "shareLib.h"
#include "epicsAssert.h"

class epicsShareClass SingletonUntyped {
public:
    SingletonUntyped ();
    ~SingletonUntyped ();
    typedef void * ( * PBuild ) ();
    void incrRefCount ( PBuild );
    typedef void ( * PDestroy ) ( void * );
    void decrRefCount ( PDestroy );
    void * pInstance () const;
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
        reference ( epicsSingleton & );
        reference ( const reference & );
        ~reference ();
	// this somewhat convoluted reference of the return
	// type ref through the epicsSingleton template is
	// required for the archaic Tornado gnu compiler
        typename epicsSingleton < TYPE > :: reference & 
	    operator = ( const reference & );
        TYPE * operator -> ();
        const TYPE * operator -> () const;
        TYPE & operator * ();
        const TYPE & operator * () const;
    private:
        epicsSingleton * _pSingleton;
    };
    friend class reference;
    epicsSingleton () {}
    // mutex lock/unlock pair overhead incured
    // when either of these are called
    reference getReference ();
    const reference getReference () const;
private:
    SingletonUntyped _singletonUntyped;
    static void * _build ();
    static void _destroy ( void * );
    epicsSingleton ( const epicsSingleton & );
    epicsSingleton & operator = ( const epicsSingleton & );
};

template < class TYPE >
inline epicsSingleton < TYPE > :: reference :: 
    reference ( epicsSingleton & es ):
    _pSingleton ( & es ) 
{
    es._singletonUntyped.
        incrRefCount ( & epicsSingleton < TYPE > :: _build );
}

template < class TYPE >
inline epicsSingleton < TYPE > :: reference :: 
    reference ( const reference & ref ) :
    _pSingleton ( ref._pSingleton )
{
    assert ( _pSingleton );
    _pSingleton->_singletonUntyped.
        incrRefCount ( & epicsSingleton < TYPE > :: _build );
}

template < class TYPE >
inline epicsSingleton < TYPE > :: reference :: 
    ~reference ()
{
    assert ( _pSingleton );
    _pSingleton->_singletonUntyped.
        decrRefCount ( & epicsSingleton < TYPE > :: _destroy );
}

template < class TYPE >
typename epicsSingleton < TYPE > :: reference & 
    epicsSingleton < TYPE > :: reference :: 
        operator = ( const reference & ref )
{
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

template < class TYPE >
inline TYPE * 
    epicsSingleton < TYPE > :: reference :: 
        operator -> ()
{ 
    assert ( _pSingleton );
    return reinterpret_cast < TYPE * > 
            ( _pSingleton->_singletonUntyped.pInstance () ); 
}

template < class TYPE >
inline const TYPE * 
    epicsSingleton < TYPE > :: reference :: 
        operator -> () const 
{
    assert ( _pSingleton );
    return reinterpret_cast < const TYPE * > 
            ( _pSingleton->_singletonUntyped.pInstance () ); 
}

template < class TYPE >
inline TYPE & 
    epicsSingleton < TYPE > :: reference :: 
        operator * () 
{
    return * this->operator -> ();
}

template < class TYPE >
inline const TYPE & 
    epicsSingleton < TYPE > :: reference :: 
            operator * () const 
{
    return * this->operator -> ();
}

inline SingletonUntyped :: SingletonUntyped () : 
    _pInstance ( 0 ), _refCount ( 0 )
{
}

inline void * SingletonUntyped :: pInstance () const
{
    return _pInstance;
}

inline SingletonUntyped :: ~SingletonUntyped ()
{
    // we dont assert fail on non-zero _refCount
    // and or non nill _pInstance here because this
    // is designed to tolarate situations where 
    // file scope epicsSingleton objects (which 
    // theoretically dont have storage lifespan 
    // issues) are deleted in a non-determanistic 
    // order
#   if 0
        assert ( _refCount == 0 );
        assert ( _pInstance == 0 );
#   endif
}

template < class TYPE >
void * epicsSingleton < TYPE > :: _build ()
{
    return new TYPE ();
}

template < class TYPE >
void epicsSingleton < TYPE > :: 
    _destroy ( void * pDestroyTypeless )
{
    TYPE * pDestroy = 
        reinterpret_cast < TYPE * > ( pDestroyTypeless );
    delete pDestroy;
}

template < class TYPE >
inline typename epicsSingleton < TYPE > :: reference 
    epicsSingleton < TYPE > :: getReference ()
{
    return reference ( * this );
}

template < class TYPE >
inline const typename epicsSingleton < TYPE > :: reference 
    epicsSingleton < TYPE > :: getReference () const
{
    epicsSingleton < TYPE > * pConstCastAway = 
        const_cast < epicsSingleton < TYPE > * > ( this );
    return pConstCastAway->getReference ();
}

#endif // epicsSingleton_h

