/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// Smart Pointer Class for GDD
// ( handles referencing / unreferencing a gdd each time it is used )
//
// Author: Jeff Hill
//

#ifndef smartGDDPointer_h
#define smartGDDPointer_h

#include "gdd.h"
#include "shareLib.h"

//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !! WARNING WARNING WARNING WARNING WARNING WARNING WARNING !!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// // be careful to manually unreference here
// // (the reference count is initialized to one by the gdd ctor)
// Gdd * pDD0 = new gdd;
// smartGDDPointer pDD1 (pDD0);
// pDD0->unreference ();
// 
// // from here on down the smart pointer maintains the ref count
// smartGDDPointer pDD2 = pDD1;
// smartGDDPointer pDD3 = pDD2;
// pDD2 = pDD3;
//
template < class T >
class smartGDDPointerTemplate {
public:
	smartGDDPointerTemplate ();
	smartGDDPointerTemplate ( T & valueIn );
	smartGDDPointerTemplate ( T * pValueIn );
	smartGDDPointerTemplate ( const smartGDDPointerTemplate & ptrIn );
	~smartGDDPointerTemplate ();
	smartGDDPointerTemplate < T > & operator = ( T * rhs );
	smartGDDPointerTemplate < T > & operator = ( const smartGDDPointerTemplate & rhs );
    operator smartGDDPointerTemplate < const T > () const;
    void set ( T * pNewValue );
    T * get () const;
	T * operator -> () const;
	T & operator * () const;
	bool operator ! () const;
    bool valid () const;
    void swap ( smartGDDPointerTemplate < T > & );
    //
    // see Meyers, "more effective C++", for explanation on
    // why conversion to dumb pointer is ill advised here
    //
	//operator const gdd * () const;
protected: 
	T * pValue;
};

typedef smartGDDPointerTemplate < gdd > smartGDDPointer;
typedef smartGDDPointerTemplate < const gdd > smartConstGDDPointer;

template < class T >
inline smartGDDPointerTemplate < T >::smartGDDPointerTemplate () :
	pValue (0) {}

template < class T >
inline smartGDDPointerTemplate < T >::smartGDDPointerTemplate ( T & valueIn ) :
	pValue ( & valueIn )
{
    gddStatus status = this->pValue->reference ();
    assert ( ! status );
}

template < class T >
inline smartGDDPointerTemplate < T >::smartGDDPointerTemplate ( T * pValueIn ) :
	pValue ( pValueIn )
{
	if ( this->pValue != NULL ) {
        gddStatus status = this->pValue->reference ();
        assert ( ! status );
	}
}

template < class T >
inline smartGDDPointerTemplate < T > :: 
        smartGDDPointerTemplate ( const smartGDDPointerTemplate < T > & ptrIn ) :
	pValue ( ptrIn.pValue )
{
	if ( this->pValue != NULL ) {
        gddStatus status = this->pValue->reference ();
        assert ( ! status );
	}
}

template < class T >
inline smartGDDPointerTemplate < T > :: ~smartGDDPointerTemplate ()
{
	if ( this->pValue != NULL ) {
        gddStatus status = this->pValue->unreference ();
        assert ( ! status );
	}
}

template < class T >
inline void smartGDDPointerTemplate < T > :: set ( T * pNewValue ) 
{
    if ( this->pValue != pNewValue ) {
        if ( pNewValue ) {
            gddStatus status = pNewValue->reference ();
            assert ( ! status );
        }
        if ( this->pValue ) {
            this->pValue->unreference ();
        }
        this->pValue = pNewValue;
    }
}

template < class T >
inline T * smartGDDPointerTemplate < T > :: get () const
{
    return this->pValue;
}

template < class T >
inline smartGDDPointerTemplate < T > & 
    smartGDDPointerTemplate < T >::operator = ( T * rhs ) 
{
    this->set ( rhs );
	return *this;
}

template < class T >
inline smartGDDPointerTemplate < T > &
    smartGDDPointerTemplate < T >::operator = ( const smartGDDPointerTemplate < T > & rhs ) 
{
    this->set ( rhs.pValue );
	return *this;
}

template < class T >
inline T * smartGDDPointerTemplate < T > :: operator -> () const
{
	return this->pValue;
}

template < class T >
inline T & smartGDDPointerTemplate < T > :: operator * () const
{
	return * this->pValue;
}

template < class T >
inline bool smartGDDPointerTemplate < T > :: operator ! () const
{
    return this->pValue ? false : true;
}

template < class T >
inline bool smartGDDPointerTemplate < T > :: valid () const
{
    return this->pValue ? true : false;
}

template < class T >
inline void smartGDDPointerTemplate < T > :: swap ( smartGDDPointerTemplate < T > &  in )
{
    T * pTmp = this->pValue;
    this->pValue = in.pValue;
    in.pValue = pTmp;
}

template < class T >
smartGDDPointerTemplate < T > :: operator smartGDDPointerTemplate < const T > () const
{
    return smartGDDPointerTemplate < const T > ( this->pValue );
}

#endif // smartGDDPointer_h

