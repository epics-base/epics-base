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
 * Revision 1.10  1998/12/01 23:32:15  jhill
 * removed inline frm evt msk alloc
 *
 * Revision 1.9  1998/10/27 18:28:20  jhill
 * fixed warnings
 *
 * Revision 1.8  1998/05/05 16:26:31  jhill
 * fixed warnings
 *
 * Revision 1.7  1997/08/05 00:47:07  jhill
 * fixed warnings
 *
 * Revision 1.6  1997/04/10 19:34:07  jhill
 * API changes
 *
 * Revision 1.5  1996/12/11 01:01:56  jhill
 * casEventMaskEntry constr does res tbl add
 *
 * Revision 1.4  1996/12/06 22:32:11  jhill
 * force virtual destructor
 *
 * Revision 1.3  1996/11/02 00:54:10  jhill
 * many improvements
 *
 * Revision 1.2  1996/09/04 20:20:44  jhill
 * removed sizeof(casEventMask::mask) for MSVISC++
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
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
 
	this->mutex.osiLock();
	if (this->allocator<CHAR_BIT*sizeof(evMask.mask)) {
		evMask.mask = 1u<<(this->allocator++);
	}
	this->mutex.osiUnlock();
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
	stringId 		id (pName, stringId::refString);
	casEventMaskEntry	*pEntry;
	casEventMask		mask;

	this->mutex.osiLock();
	pEntry = this->lookup (id);
	if (pEntry) {
		mask = *pEntry;
	}
	else {
		mask = this->maskAllocator();
		if (mask.mask == 0u) {
			errMessage(S_cas_tooManyEvents, NULL);
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
			}
		}
	}
	this->mutex.osiUnlock();
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
	this->mutex.osiLock();
	if (level>1u) {
		printf ("casEventRegistry: bit allocator = %d\n", 
				this->allocator);
	}
	this->resTable <casEventMaskEntry, stringId>::show(level);
	this->mutex.osiUnlock();
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

