#ifndef GDD_CONTAINER_H
#define GDD_CONTAINER_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 * $Id$
 * $Log$
 * Revision 1.4  1999/05/10 23:42:25  jhill
 * fixed many const releated problems
 *
 * Revision 1.3  1999/04/30 15:24:53  jhill
 * fixed improper container index bug
 *
 * Revision 1.2  1997/04/23 17:13:04  jhill
 * fixed export of symbols from WIN32 DLL
 *
 * Revision 1.1  1997/03/21 01:56:06  jbk
 * *** empty log message ***
 *
 *
 */

#include "shareLib.h"

class constGddCursor;
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
	gddCursor getCursor(void);
	constGddCursor getCursor(void) const;

	const gdd* cData(void) const;
	gdd* cData(void);

protected:
	gddContainer(int,int,int,int*);
	~gddContainer(void);

	void cInit(int num_things_within);
	gddStatus changeType(int,aitEnum);
	gddStatus setBound(int,aitIndex,aitIndex);
	gddStatus getBound(int,aitIndex&,aitIndex&) const;
	gddStatus setBound(aitIndex,aitIndex);

private:
	friend class constGddCursor;
};

class epicsShareClass constGddCursor {
public:
	constGddCursor(void);
	constGddCursor(const gddContainer* ec);

	const gdd* first(void);
	const gdd* first(const gddScalar*&);
	const gdd* first(const gddAtomic*&);
	const gdd* first(const gddContainer*&);

	const gdd* next(void);
	const gdd* next(const gddScalar*&);
	const gdd* next(const gddAtomic*&);
	const gdd* next(const gddContainer*&);

	const gdd* current(void) const;
	const gdd* current(const gddScalar*&) const;
	const gdd* current(const gddAtomic*&) const;
	const gdd* current(const gddContainer*&) const;

	const gdd* operator[](int index);

private:
	const gddContainer* list;
	const gdd* curr;
	int curr_index;
};

class epicsShareClass gddCursor : private constGddCursor {
public:
	gddCursor(void);
	gddCursor(gddContainer* ec);

	gdd* first(void);
	gdd* first(gddScalar*&);
	gdd* first(gddAtomic*&);
	gdd* first(gddContainer*&);

	gdd* next(void);
	gdd* next(gddScalar*&);
	gdd* next(gddAtomic*&);
	gdd* next(gddContainer*&);

	gdd* current(void) const;
	gdd* current(gddScalar*&) const;
	gdd* current(gddAtomic*&) const;
	gdd* current(gddContainer*&) const;

	gdd* operator[](int index);
};

#endif
