/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_CONTAINER_H
#define GDD_CONTAINER_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 * $Id$
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
	int total(void);

	void dump(void);
	void test(void);

	// The following are slow and inefficient
	gdd* getDD(aitIndex index);
	gdd* getDD(aitIndex index,gddScalar*&);
	gdd* getDD(aitIndex index,gddAtomic*&);
	gdd* getDD(aitIndex index,gddContainer*&);
	gdd* operator[](aitIndex index);

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
