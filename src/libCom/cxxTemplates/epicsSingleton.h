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
#include "cxxCompilerDependencies.h"

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

    // inline mf def for class within a template required by visual c++ 7
    class reference {
    public:
        reference ( TYPE & tIn ) epicsThrows (()) :
            instance ( tIn ) 
        {
        }

        ~reference () epicsThrows (()) 
        {
        }

        TYPE * operator -> () epicsThrows (()) 
        { 
            return & this->instance; 
        }

        const TYPE * operator -> () const epicsThrows (())
        {
            typename epicsSingleton<TYPE>::reference & ref = 
                const_cast < typename epicsSingleton<TYPE>::reference & > ( *this );
            return ref.operator -> ();
        }

        TYPE & operator * () epicsThrows (())
        {
            return * this->operator -> ();
        }

        const TYPE & operator * () const epicsThrows (())
        {
            return * this->operator -> ();
        }

    private:
        TYPE & instance;
    };

    // lock overhead every time these are called
    typename epicsSingleton<TYPE>::reference getReference ();
    const typename epicsSingleton<TYPE>::reference getReference () const;

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

// borland 5.5 is unable to build this function optimized if it is inline
template < class TYPE >
typename epicsSingleton<TYPE>::reference epicsSingleton<TYPE>::getReference ()
{
    {
        epicsGuard < epicsMutex > guard ( epicsSingletonPrivateMutex() );
        if ( ! this->pSingleton ) {
            this->pSingleton = new TYPE;
        }
    }
    return reference ( * this->pSingleton );
}

template < class TYPE >
inline const typename epicsSingleton<TYPE>::reference epicsSingleton<TYPE>::getReference () const
{
    epicsSingleton < TYPE > * pConstCastAway = 
        const_cast < epicsSingleton < TYPE > * > ( this );
    return pConstCastAway->getReference ();
}

#endif // epicsSingleton_h

