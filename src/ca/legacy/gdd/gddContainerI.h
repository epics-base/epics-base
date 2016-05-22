/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_CONTAINERI_H
#define GDD_CONTAINERI_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 */

inline gddContainer::gddContainer(int,int,int,int*) { }
inline gddContainer::~gddContainer(void) { }

inline gddStatus gddContainer::changeType(int,aitEnum) {
	gddAutoPrint("gddContainer::changeType()",gddErrorNotAllowed);
	return gddErrorNotAllowed; }
inline gddStatus gddContainer::setBound(int,aitIndex,aitIndex) {
	gddAutoPrint("setBound()",gddErrorNotAllowed);
	return gddErrorNotAllowed; }
inline gddStatus gddContainer::getBound(int,aitIndex&,aitIndex&) const {
	gddAutoPrint("getBound()",gddErrorNotAllowed);
	return gddErrorNotAllowed; }

inline gdd* gddContainer::cData(void)
	{ return (gdd*)dataPointer(); }

inline const gdd* gddContainer::cData(void) const
	{ return (const gdd*)dataPointer(); }

inline int gddContainer::total(void) const
	{ return bounds->size(); }

inline gddStatus gddContainer::setBound(aitIndex f, aitIndex c)
	{ bounds->set(f,c); return 0; }

inline constGddCursor::constGddCursor(void):list(NULL)
	{ curr=NULL; }
inline constGddCursor::constGddCursor(const gddContainer* ec):list(ec)
	{ curr=ec->cData(); curr_index=0; }

inline const gdd* constGddCursor::first(void)
	{ curr=list->cData(); curr_index=0; return curr; }
inline const gdd* constGddCursor::first(const gddScalar*& dd)
	{ return (const gdd*)(dd=(gddScalar*)first()); }
inline const gdd* constGddCursor::first(const gddAtomic*& dd)
	{ return (const gdd*)(dd=(gddAtomic*)first()); }
inline const gdd* constGddCursor::first(const gddContainer*& dd)
	{ return (const gdd*)(dd=(gddContainer*)first()); }

inline const gdd* constGddCursor::next(void)
	{ if(curr) { curr_index++;curr=curr->next(); } return curr; }
inline const gdd* constGddCursor::next(const gddScalar*& dd)
	{ return (const gdd*)(dd=(gddScalar*)next()); }
inline const gdd* constGddCursor::next(const gddAtomic*& dd)
	{ return (const gdd*)(dd=(gddAtomic*)next()); }
inline const gdd* constGddCursor::next(const gddContainer*& dd)
	{ return (const gdd*)(dd=(gddContainer*)next()); }

inline const gdd* constGddCursor::current(void) const 
	{ return curr; }
inline const gdd* constGddCursor::current(const gddScalar*& dd) const
	{ return (const gdd*)(dd=(gddScalar*)current()); }
inline const gdd* constGddCursor::current(const gddAtomic*& dd) const
	{ return (const gdd*)(dd=(gddAtomic*)current()); }
inline const gdd* constGddCursor::current(const gddContainer*& dd) const
	{ return (const gdd*)(dd=(gddContainer*)current()); }

inline gddCursor::gddCursor(void){}
inline gddCursor::gddCursor(gddContainer* ec) :
	constGddCursor(ec) {}

inline gdd* gddCursor::first(void)
	{ return (gdd *) constGddCursor::first(); }
inline gdd* gddCursor::first(gddScalar*& dd)
	{ return (gdd *) constGddCursor::first((const gddScalar*&)dd); }
inline gdd* gddCursor::first(gddAtomic*& dd)
	{ return (gdd *) constGddCursor::first((const gddAtomic*&)dd); }
inline gdd* gddCursor::first(gddContainer*& dd)
	{ return (gdd *) constGddCursor::first((const gddContainer*&)dd); }

inline gdd* gddCursor::next(void)
	{ return (gdd *) constGddCursor::next(); }
inline gdd* gddCursor::next(gddScalar*& dd)
	{ return (gdd*)constGddCursor::next((const gddScalar*&)dd); }
inline gdd* gddCursor::next(gddAtomic*& dd)
	{ return (gdd*)constGddCursor::next((const gddAtomic*&)dd); }
inline gdd* gddCursor::next(gddContainer*& dd)
	{ return (gdd*)constGddCursor::next((const gddContainer*&)dd); }

inline gdd* gddCursor::current(void) const 
	{ return (gdd*)constGddCursor::current(); }
inline gdd* gddCursor::current(gddScalar*& dd) const
	{ return (gdd*)constGddCursor::current((const gddScalar*&)dd); }
inline gdd* gddCursor::current(gddAtomic*& dd) const
	{ return (gdd*)constGddCursor::current((const gddAtomic*&)dd); }
inline gdd* gddCursor::current(gddContainer*& dd) const
	{ return (gdd*)constGddCursor::current((const gddContainer*&)dd); }

inline gdd* gddCursor::operator[](int index)
{ return (gdd *) constGddCursor::operator [](index); }

#endif
