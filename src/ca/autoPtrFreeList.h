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
 *  Copyright, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef autoPtrFreeListh
#define autoPtrFreeListh

#ifdef epicsExportSharedSymbols
#   define autoPtrFreeListh_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "tsFreeList.h"
#include "cxxCompilerDependencies.h"

#ifdef autoPtrFreeListh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

template < class T >
class autoPtrFreeList {
public:
    autoPtrFreeList ( tsFreeList < T > &, T * );
    ~autoPtrFreeList ();
    T & operator * () const;
    T * operator -> () const;
    T * get () const;
    T * release ();
private:
    T * p;
    tsFreeList < T > & freeList;
    // not implemented
    autoPtrFreeList & operator = ( const autoPtrFreeList & );
    autoPtrFreeList ( const autoPtrFreeList<T> & );
};

template < class T >
inline autoPtrFreeList<T>::autoPtrFreeList ( tsFreeList < T > & freeListIn, T * pIn ) :
    p ( pIn ), freeList ( freeListIn ) {}

template < class T >
inline autoPtrFreeList<T>::~autoPtrFreeList ()
{
    if ( this->p ) {
        this->p->~T();
        // its probably a good idea to require that the class has placement delete
        // by calling it during cleanup if the compiler supports it
#       if defined ( CXX_PLACEMENT_DELETE )
            T::operator delete ( this->p, this->freeList );
#       else
            this->freeList.release ( this->p );
#       endif
    }
}

template < class T >
inline T & autoPtrFreeList<T>::operator * () const
{
    return * this->p;
}

template < class T >
inline T * autoPtrFreeList<T>::operator -> () const
{
    return this->p;
}

template < class T >
inline T * autoPtrFreeList<T>::get () const
{
    return this->p;
}

template < class T >
inline T * autoPtrFreeList<T>::release ()
{
    T *pTmp = this->p;
    this->p = 0;
    return pTmp;
}

#endif // #ifdef autoPtrFreeListh
