/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//  epicsMemoryH.h
//	Author:	Jeff Hill
//	Date:	March 2001

#ifndef epicsMemoryH
#define epicsMemoryH

enum epics_auto_ptr_type {
    eapt_scalar, eapt_array };

template < class T, epics_auto_ptr_type PT = eapt_scalar >
class epics_auto_ptr {
public:	
    typedef T element_type;
    explicit epics_auto_ptr ( T * p = 0 ) throw ();
    epics_auto_ptr ( epics_auto_ptr<T,PT> & rhs ) throw ();
	~epics_auto_ptr() throw ();
    epics_auto_ptr<T,PT> & operator = ( epics_auto_ptr<T,PT> & rhs ) throw ();					
	T & operator * () const throw ();
    T * operator -> () const throw ();
    T & operator [] ( unsigned index ) const throw ();
	T * get () const throw ();                 
	T * release () throw (); 
    void reset ( T * p = 0 ) throw ();
private:
    T * p;
    void destroyTarget () throw ();
};

template < class T, epics_auto_ptr_type PT >
inline epics_auto_ptr<T,PT>::epics_auto_ptr ( T *pIn ) throw () : 
p ( pIn ) {}

template < class T, epics_auto_ptr_type PT >
inline epics_auto_ptr<T,PT>::
    epics_auto_ptr ( epics_auto_ptr<T,PT> & ap ) throw () : 
    p ( ap.release() ) {}

template < class T, epics_auto_ptr_type PT >
inline void epics_auto_ptr<T,PT>::destroyTarget () throw ()
{
    if ( PT == eapt_scalar ) {
        delete this->p;
    }
    else { 
        delete [] this->p;
    }
}

template < class T, epics_auto_ptr_type PT >
inline epics_auto_ptr<T,PT>::~epics_auto_ptr () throw ()
{
    this->destroyTarget ();
}

template < class T, epics_auto_ptr_type PT >	
inline epics_auto_ptr<T,PT> & epics_auto_ptr<T,PT>::operator = 
    ( epics_auto_ptr<T,PT> & rhs ) throw ()
{
    if ( &rhs != this) {
        this->destroyTarget ();
        this->p = rhs.release ();
    }
    return *this;
}

template < class T, epics_auto_ptr_type PT >	
inline T & epics_auto_ptr<T,PT>::operator * () const throw()
{ 
    return * this->p; 
}

template < class T, epics_auto_ptr_type PT >	
inline T * epics_auto_ptr<T,PT>::operator -> () const throw ()
{ 
    return this->p; 
}

template < class T, epics_auto_ptr_type PT >	
T & epics_auto_ptr<T,PT>::operator [] ( unsigned index ) const throw ()
{
    return this->p [ index ];
}

template < class T, epics_auto_ptr_type PT >	
inline T * epics_auto_ptr<T,PT>::get () const throw ()
{ 
    return this->p; 
}

template < class T, epics_auto_ptr_type PT >	
inline T * epics_auto_ptr<T,PT>::release () throw ()
{ 
    T *pTmp = this->p;
    this->p = 0;
    return pTmp; 
}

template < class T, epics_auto_ptr_type PT >	
inline void epics_auto_ptr<T,PT>::reset ( T * pIn ) throw ()
{
    this->destroyTarget ();
    this->p = pIn;
}

#endif // ifndef epicsMemoryH
