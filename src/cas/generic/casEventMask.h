/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 *
 */


#ifndef casEventMaskH
#define casEventMaskH

#include <resourceLib.h>
 

class casEventMaskEntry;
class casEventRegistry;


class casEventMask {
        friend inline casEventMask operator| (const casEventMask &lhs, 
					const casEventMask &rhs);
        friend inline casEventMask operator& (const casEventMask &lhs, 
					const casEventMask &rhs);
        friend inline int operator== (const casEventMask &lhs, 
					const casEventMask &rhs);
        friend inline int operator!= (const casEventMask &lhs, 
					const casEventMask &rhs);
	friend class casEventRegistry;
public:
	void clear ()
	{
		this->mask = 0u;
	}

        casEventMask (casEventRegistry &reg, const char *pName);

        casEventMask () 
	{
		this->clear();
	}
	
	void show (unsigned level);
 
	int eventsSelected()
	{
		return this->mask!=0u;
	}
	int noEventsSelected()
	{
		return this->mask==0u;
	}

        inline void operator|= (const casEventMask &rhs);
        inline void operator&= (const casEventMask &rhs);
 
private:
        unsigned mask;
};

class casEventMaskEntry : public tsSLNode<casEventMaskEntry>, 
	public casEventMask, public stringId {
public:
	casEventMaskEntry (casEventMask maskIn, const char *pName) : 
		casEventMask (maskIn), stringId (pName)	{}
	void show (unsigned level) 
	{
		this->casEventMask::show(level);
		this->stringId::show(level);
	}
private:
};
 
class casEventRegistry : resTable <casEventMaskEntry, stringId> {
public:
	casEventRegistry() : allocator(0) 
	{
		int status;
		status = this->resTable <casEventMaskEntry, stringId>::init(1u<<8u);
		if (status) {
			this->init = 0u;
			return;
		}
		this->init = 1u;
	}
	~casEventRegistry()
	{
		this->destroyAllEntries();
	}
        casEventMask registerEvent (const char *pName);

	void show (unsigned level);

private:
	unsigned allocator;
	unsigned char init;

        casEventMask maskAllocator();
};

inline casEventMask::casEventMask (casEventRegistry &reg, const char *pName)
{
	*this = reg.registerEvent (pName);
}

inline casEventMask operator| (const casEventMask &lhs, const casEventMask &rhs)
{
        casEventMask result;
 
        result.mask = lhs.mask | rhs.mask;
        return result;
}
inline casEventMask operator& (const casEventMask &lhs, const casEventMask &rhs)
{
        casEventMask result;
 
        result.mask = lhs.mask & rhs.mask;
        return result;
}

inline int operator== (const casEventMask &lhs, const casEventMask &rhs)
{
        return lhs.mask == rhs.mask;
}

inline int operator!= (const casEventMask &lhs, const casEventMask &rhs)
{
        return lhs.mask != rhs.mask;
}

inline void casEventMask::operator|= (const casEventMask &rhs) {
	*this = *this | rhs;		
}

inline void casEventMask::operator&= (const casEventMask &rhs) {
	*this = *this & rhs;		
}

#endif // casEventMaskH

