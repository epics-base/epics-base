/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casOpaqueAddrILh
#define casOpaqueAddrILh

//
// casOpaqueAddr::set()
//
inline void casOpaqueAddr::set (const caAddr &addr)
{
	caAddr *p = (caAddr *) this->opaqueAddr;	

	//
	// see class casOpaqueAddr::checkSize
	// for assert fail when
	// sizeof(casOpaqueAddr::opaqueAddr) < sizeof(caAddr)
	//
	*p = addr;
	this->init = true;
}

//
// casOpaqueAddr::set()
//
inline casOpaqueAddr::casOpaqueAddr (const caAddr &addr)
{
	this->set(addr);
}

//
// casOpaqueAddr::get()
//
inline caAddr casOpaqueAddr::get () const
{
	caAddr *p = (caAddr *) this->opaqueAddr;	

	assert ( this->init );
	//
	// see class casOpaqueAddr::checkSize
	// for assert fail when
	// sizeof(casOpaqueAddr::opaqueAddr) < sizeof(caAddr)
	//
	return *p;
}

#endif // casOpaqueAddrILh

