/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_UTILS_H
#define GDD_UTILS_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 */

#ifdef vxWorks
#include <vxWorks.h>
#include <vme.h>
#include <sysLib.h>
#include <semLib.h>
#endif

#include "aitTypes.h"
#include "gddErrorCodes.h"
#include "gddNewDel.h"
#include "shareLib.h"

// ---------------------------------------------------------------------
// Describe the bounds of a single dimension in terms of the first
// element and the number of elements in the dimension.

class epicsShareClass gddBounds
{
public:
	// I found a weird memory management quirk with SunOS when constructors
	// are present in a class that is used as an array in another class
	// as it is in gddBounds2D.  The memory manager reallocates the entire
	// gddBounds2D in some sort of buffer pool which stays allocated when the
	// program completes

	// gddBounds(void);
	// gddBounds(aitIndex c);
	// gddBounds(aitIndex f, aitIndex c);

	void setSize(aitIndex c);
	void setFirst(aitIndex c);
	void set(aitIndex f, aitIndex c);
	void get(aitIndex& f, aitIndex& c);
	aitIndex size(void) const;
	aitIndex first(void) const;

private:
	aitIndex start;
	aitIndex count;
};


// ---------------------------------------------------------------------
// This is a class for managing the destruction of user data held within
// a DD.  It allows multiple DDs to reference the same chunk of user
// data.  The data would remain alive until the reference count goes to
// zero, at this time the user destructor function will be called with
// a pointer to the piece of data to deal with.  The intention here is
// that user will create a subclass of this and supply the virtual
// function run(void*).  The default behavior will be to treat the void*
// data as an array of aitInt8 and call remove.

// **** WARNING!
// The reference count is initialized to zero.  This means that if you
// create one and want to keep it, you must reference it twice - the
// initial time and a time for your personal use.  This is because the
// gdd library automatically increments the reference count on 
// destructors when data is referenced into a gdd with a destructor.
// Remember the rule: if you pass a destructor to someone, then that
// someone now owns the destructor.  A reference count of 1 indicates that
// the thing will go away when unreferenced - which is what you will
// have if you only reference the destructor once after it is created.

class epicsShareClass gddDestructor
{
public:
	gddDestructor(void);
	gddDestructor(void* user_arg);

	void reference(void);
	int refCount(void) const;

	gddStatus destroy(void* thing_to_remove);
	virtual void run(void* thing_to_remove);

	gdd_NEWDEL_FUNC(arg) // for using generic new and remove
protected:
	aitUint16 ref_cnt;
	void* arg;
	virtual ~gddDestructor () {}
private:
	gdd_NEWDEL_DATA
};

#include "gddUtilsI.h"

// ---------------------------------------------------------------------
// Special managment for 1D-2D-3D bounds - others use normal new/remove.
// Since bounds are maintained as arrays, where the length of the array is
// the dimension of the data, we will manage the normal cases using 
// free lists.

class epicsShareClass gddBounds1D
{
public:
	gddBounds1D(void) { }
	gddBounds* boundArray(void);
	gdd_NEWDEL_FUNC(b[0]) // required for using generic new and remove
private:
	gddBounds b[1];
	gdd_NEWDEL_DATA // required for using generic new/remove
};
inline gddBounds* gddBounds1D::boundArray(void) { return b; }

class epicsShareClass gddBounds2D
{
public:
	gddBounds2D(void) { }
	gddBounds* boundArray(void);
	gdd_NEWDEL_FUNC(b[0]) // required for using generic new and remove
private:
	gddBounds b[2];
	gdd_NEWDEL_DATA // required for using generic new/remove
};
inline gddBounds* gddBounds2D::boundArray(void) { return b; }

class epicsShareClass gddBounds3D
{
public:
	gddBounds3D(void) { }
	gddBounds* boundArray(void);
	gdd_NEWDEL_FUNC(b[0]) // for using generic new and remove
private:
	gddBounds b[3];
	gdd_NEWDEL_DATA // required for using generic new/remove
};
inline gddBounds* gddBounds3D::boundArray(void) { return b; }

#endif

