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
 *  Author: Jeffrey O. Hill
 *
 */

#ifndef epicsSingleton_h
#define epicsSingleton_h

#include <new>

#include "shareLib.h"
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsThread.h"

// This class exists for the purpose of avoiding file scope
// object chicken and egg problems. It implements thread safe
// lazy initialization. To avoid locking overhead retain a
// copy of the epicsSingleton::reference for future use. The
// class referenced by epicsSingleton::reference is _not_
// destroyed by ~epicsSingleton() because this would introduce
// additional file scope chicken and egg problems.
template < class TYPE >
class epicsSingleton {
public:
    epicsSingleton ();
    ~epicsSingleton ();

    // inline func def required by visual c++ 7
    class reference {
    public:
        reference ( TYPE & tIn ) throw () :
            instance ( tIn ) 
        {
        }

        ~reference () throw () 
        {
        }

        TYPE * operator -> () throw () 
        { 
            return & this->instance; 
        }

        const TYPE * operator -> () const throw ()
        {
            epicsSingleton<TYPE>::reference & ref = 
                const_cast < epicsSingleton<TYPE>::reference & > ( *this );
            return ref.operator -> ();
        }

        TYPE & operator * () throw ()
        {
            return * this->operator -> ();
        }

        const TYPE & operator * () const throw ()
        {
            return * this->operator -> ();
        }

    private:
        TYPE & instance;
    };

    // lock overhead every time these are called
    operator reference ();
    operator const reference () const;

private:
    TYPE * pSingleton;
};

template < class TYPE >
inline epicsSingleton<TYPE>::epicsSingleton () : 
    pSingleton ( 0 )
{
}

template < class TYPE >
inline epicsSingleton<TYPE>::~epicsSingleton ()
{
    // Deleting the singelton in the destructor causes problems when 
    // there are file scope objects that reference the singleton in 
    // their destructors. Since this class's purpose is to avoid these
    // sorts of problems then clean up is left to other classes.
}

epicsShareFunc epicsMutex & epicsSingletonPrivateMutex ();

template < class TYPE >
epicsSingleton<TYPE>::operator epicsSingleton<TYPE>::reference ()
{
    epicsGuard < epicsMutex > guard ( epicsSingletonPrivateMutex() );
    if ( ! this->pSingleton ) {
        this->pSingleton = new TYPE;
    }
    return reference ( * this->pSingleton );
}

template < class TYPE >
epicsSingleton<TYPE>::operator const epicsSingleton<TYPE>::reference () const
{
    epicsSingleton < TYPE > * pConstCastAway = 
        const_cast < epicsSingleton < TYPE > * > ( this );
    return *pConstCastAway;
}

#endif // epicsSingleton_h

