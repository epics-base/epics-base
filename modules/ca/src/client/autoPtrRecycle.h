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
 *  Copyright, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef autoPtrRecycleh
#define autoPtrRecycleh

template < class T >
class autoPtrRecycle {
public:
    autoPtrRecycle ( 
        epicsGuard < epicsMutex > &, chronIntIdResTable < baseNMIU > &,
        cacRecycle &, T * );
    ~autoPtrRecycle ();
    T & operator * () const;
    T * operator -> () const;
    T * get () const;
    T * release ();
private:
    T * p;
    cacRecycle & r;
    chronIntIdResTable < baseNMIU > & ioTable;
    epicsGuard < epicsMutex > & guard;
    // not implemented
	autoPtrRecycle ( const autoPtrRecycle & );
	autoPtrRecycle & operator = ( const autoPtrRecycle & );
};

template < class T >
inline autoPtrRecycle<T>::autoPtrRecycle ( 
    epicsGuard < epicsMutex > & guardIn, chronIntIdResTable < baseNMIU > & tbl,
        cacRecycle & rIn, T * pIn ) :
    p ( pIn ), r ( rIn ), ioTable ( tbl ), guard ( guardIn ) {}

template < class T >
inline autoPtrRecycle<T>::~autoPtrRecycle ()
{
    if ( this->p ) {
        baseNMIU *pb = this->p;
        this->ioTable.remove ( *pb );
        pb->destroy ( this->guard, this->r );
    }
}

template < class T >
inline T & autoPtrRecycle<T>::operator * () const
{
    return * this->p;
}

template < class T >
inline T * autoPtrRecycle<T>::operator -> () const
{
    return this->p;
}

template < class T >
inline T * autoPtrRecycle<T>::get () const
{
    return this->p;
}

template < class T >
inline T * autoPtrRecycle<T>::release ()
{
    T *pTmp = this->p;
    this->p = 0;
    return pTmp;
}

#endif // #ifdef autoPtrRecycleh
