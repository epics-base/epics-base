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
 */

#include <stdio.h>
#include <limits.h>

#include "server.h"

#ifdef TEST
main ()
{
	casEventRegistry 	reg;
	casEventMask		bill1 (reg, "bill");
	casEventMask		bill2 (reg, "bill");
	casEventMask		bill3 (reg, "bill");
	casEventMask		art1 (reg, "art");
	casEventMask		art2 (reg, "art");
	casEventMask		jane (reg, "jane");
	casEventMask		artBill;
	casEventMask		tmp;

	bill1.show(10u);
	reg.show(10u);
	bill2.show(10u);
	reg.show(10u);
	bill3.show(10u);
	reg.show(10u);
	jane.show(10u);
	reg.show(10u);
	art1.show(10u);
	reg.show(10u);
	art2.show(10u);
	reg.show(10u);

	assert (bill1 == bill2);
	assert (bill1 == bill3);
	assert (jane != bill1);
	assert (jane != art1);
	assert (bill1 != art1);
	assert (art1 == art2);

	artBill = art1 | bill1;
	tmp = artBill & art1;
	assert (tmp.eventsSelected());
	tmp = artBill & bill1;
	assert (tmp.eventsSelected());
	tmp = artBill&jane;
	assert (tmp.noEventsSelected());
}
#endif

//
// casEventRegistry::maskAllocator()
//
casEventMask casEventRegistry::maskAllocator()
{
	casEventMask    evMask;
 
	if (this->maskBitAllocator<CHAR_BIT*sizeof(evMask.mask)) {
		evMask.mask = 1u<<(this->maskBitAllocator++);
	}
	return evMask;
}

//
// casEventRegistry::registerEvent()
//
casEventMask casEventRegistry::registerEvent(const char *pName)
{
	//
	// NOTE: pName outlives id here
	// (so the refString option is ok)
	//
	stringId                id (pName, stringId::refString);
	casEventMaskEntry       *pEntry;
	casEventMask            mask;

	pEntry = this->lookup (id);
	if (pEntry) {
		mask = *pEntry;
	}
	else {
		mask = this->maskAllocator();
		if (mask.mask == 0u) {
			errMessage (S_cas_tooManyEvents, NULL);
		}
		else {
			pEntry = new casEventMaskEntry(*this, mask, pName);
			if (pEntry) {
				mask = *pEntry;
			}
			else {
				mask.mask = 0u;
				errMessage(S_cas_noMemory, 
					"mask bit was lost during init");
                throw S_cas_noMemory;
			}
		}
	}
	return mask;
}

//
// casEventMask::show()
//
void casEventMask::show(unsigned level) const
{
	if (level>0u) {
		printf ("casEventMask = %x\n", this->mask);
	}
}

casEventMask::casEventMask (casEventRegistry &reg, const char *pName)
{
        *this = reg.registerEvent (pName);
}

//
// casEventRegistry::show()
//
void casEventRegistry::show(unsigned level) const
{
	if (level>1u) {
		printf ("casEventRegistry: bit allocator = %d\n", 
				this->maskBitAllocator);
	}
	this->resTable <casEventMaskEntry, stringId>::show(level);
}

//
// casEventMaskEntry::casEventMaskEntry()
//
casEventMaskEntry::casEventMaskEntry(
	casEventRegistry &regIn, casEventMask maskIn, const char *pName) :
	casEventMask (maskIn), stringId (pName), reg (regIn)
{
	int 	stat;

	assert (this->resourceName()!=NULL);
	stat = this->reg.add(*this);
	assert (stat==0);
}

//
// casEventMaskEntry::~casEventMaskEntry()
//
// empty destructor forces virtual
//
// (not inline so that we avoid duplication resulting 
// in the object code created by some compilers)
//
casEventMaskEntry::~casEventMaskEntry()
{
        this->reg.remove (*this);
}

//
// casEventMaskEntry::destroy()
//
void casEventMaskEntry::destroy()
{
	delete this;
}

//
// casEventMaskEntry::show()
//
void casEventMaskEntry::show (unsigned level) const 
{
	this->casEventMask::show(level);
	this->stringId::show(level);
}

