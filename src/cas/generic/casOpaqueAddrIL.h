
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
	this->init = TRUE;
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

	assert(this->init);
	//
	// see class casOpaqueAddr::checkSize
	// for assert fail when
	// sizeof(casOpaqueAddr::opaqueAddr) < sizeof(caAddr)
	//
	return *p;
}

#endif // casOpaqueAddrILh

