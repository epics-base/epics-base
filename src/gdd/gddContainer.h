#ifndef GDD_CONTAINER_H
#define GDD_CONTAINER_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 * $Id$
 * $Log$
 * Revision 1.2  1997/04/23 17:13:04  jhill
 * fixed export of symbols from WIN32 DLL
 *
 * Revision 1.1  1997/03/21 01:56:06  jbk
 * *** empty log message ***
 *
 *
 */

#include "shareLib.h"

class gddCursor;

/* this class needs to be able to register a destructor for the container */

class epicsShareClass gddContainer : public gdd
{
public:
	gddContainer(void);
	gddContainer(int app);
	gddContainer(gddContainer* ac);
	gddContainer(int app,int number_of_things_in_it);

	gddStatus insert(gdd*);
	gddStatus remove(aitIndex index);
	int total(void) const;

	void dump(void) const;
	void test(void);

	// preferred method for looking into a container
	gddCursor getCursor(void) const;
	gdd* cData(void) const;

protected:
	gddContainer(int,int,int,int*) { }
	~gddContainer(void) { }

	void cInit(int num_things_within);
	gddStatus changeType(int,aitEnum) {
		gddAutoPrint("gddContainer::changeType()",gddErrorNotAllowed);
		return gddErrorNotAllowed; }
	gddStatus setBound(int,aitIndex,aitIndex) {
		gddAutoPrint("setBound()",gddErrorNotAllowed);
		return gddErrorNotAllowed; }
	gddStatus getBound(int,aitIndex&,aitIndex&) {
		gddAutoPrint("getBound()",gddErrorNotAllowed);
		return gddErrorNotAllowed; }
	gddStatus setBound(aitIndex,aitIndex);

private:
	friend class gddCursor;
};

class epicsShareClass gddCursor
{
public:
	gddCursor(void);
	gddCursor(const gddContainer* ec);

	gdd* first(void);
	gdd* first(gddScalar*&);
	gdd* first(gddAtomic*&);
	gdd* first(gddContainer*&);

	gdd* next(void);
	gdd* next(gddScalar*&);
	gdd* next(gddAtomic*&);
	gdd* next(gddContainer*&);

	gdd* current(void);
	gdd* current(gddScalar*&);
	gdd* current(gddAtomic*&);
	gdd* current(gddContainer*&);

	gdd* operator[](int index);

private:
	const gddContainer* list;
	gdd* curr;
	int curr_index;
};

#include "gddContainerI.h"

#endif
