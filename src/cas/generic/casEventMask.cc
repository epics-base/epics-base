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


#include <epicsAssert.h>
#include <stdio.h>
#include <limits.h>

#include <casdef.h>
#include <casEventMask.h>

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
inline casEventMask casEventRegistry::maskAllocator()
{
        casEventMask    evMask;
 
        if (this->allocator>=CHAR_BIT*sizeof(casEventMask::mask)) {
                return evMask;
        }
        evMask.mask = 1u<<(this->allocator++);
        return evMask;
}

//
// casEventMask::registerEvent()
//
casEventMask casEventRegistry::registerEvent(const char *pName)
{
	casEventMaskEntry	*pEntry;
	stringId 		id (pName);
	int			stat;
	casEventMask		mask;

	if (!this->init) {
		errMessage(S_cas_noMemory, "no memory during init?");
		return mask;
	}

	pEntry = this->lookup (id);
	if (pEntry) {
		return *pEntry;
	}
	mask = this->maskAllocator();
	if (mask.mask == 0u) {
		errMessage(S_cas_tooManyEvents, NULL);
		return mask;
	}
	pEntry = new casEventMaskEntry(mask, pName);
	if (!pEntry) {
		mask.mask = 0u;
		errMessage(S_cas_noMemory, "mask bit was lost during init");
		return mask;
	}
	stat = this->add(*pEntry);
	assert(stat==0);
	return *pEntry;
}

//
// casEventMask::show()
//
void casEventMask::show(unsigned level)
{
	if (level>0u) {
		printf ("casEventMask = %x\n", this->mask);
	}
}

//
// casEventRegistry::show()
//
void casEventRegistry::show(unsigned level)
{
	if (level>1u) {
		printf ("init = %d bit allocator = %d\n", 
				this->init, this->allocator);
	}
	this->resTable <casEventMaskEntry, stringId>::show(level);
}

