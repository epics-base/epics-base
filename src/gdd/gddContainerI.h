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
 *
 * $Id$
 * $Log$
 * Revision 1.1  1997/03/21 01:56:07  jbk
 * *** empty log message ***
 *
 *
 */

inline gdd* gddContainer::cData(void) const
	{ return (gdd*)dataPointer(); }
inline int gddContainer::total(void)
	{ return bounds->size(); }
inline gdd* gddContainer::operator[](aitIndex index)
	{ return getDD(index); }
inline gdd* gddContainer::getDD(aitIndex index,gddScalar*& dd)
	{ return (gdd*)(dd=(gddScalar*)getDD(index)); }
inline gdd* gddContainer::getDD(aitIndex index,gddAtomic*& dd)
	{ return (gdd*)(dd=(gddAtomic*)getDD(index)); }
inline gdd* gddContainer::getDD(aitIndex index,gddContainer*& dd)
	{ return (gdd*)(dd=(gddContainer*)getDD(index)); }
inline gddStatus gddContainer::setBound(aitIndex f, aitIndex c)
	{ bounds->set(f,c); return 0; }

inline gddCursor::gddCursor(void):list(NULL)
	{ curr=NULL; }
inline gddCursor::gddCursor(const gddContainer* ec):list(ec)
	{ curr=ec->cData(); curr_index=0; }

inline gdd* gddCursor::first(void)
	{ curr=list->cData(); curr_index=0; return curr; }
inline gdd* gddCursor::first(gddScalar*& dd)
	{ return (gdd*)(dd=(gddScalar*)first()); }
inline gdd* gddCursor::first(gddAtomic*& dd)
	{ return (gdd*)(dd=(gddAtomic*)first()); }
inline gdd* gddCursor::first(gddContainer*& dd)
	{ return (gdd*)(dd=(gddContainer*)first()); }

inline gdd* gddCursor::next(void)
	{ if(curr) { curr_index++;curr=curr->next(); } return curr; }
inline gdd* gddCursor::next(gddScalar*& dd)
	{ return (gdd*)(dd=(gddScalar*)next()); }
inline gdd* gddCursor::next(gddAtomic*& dd)
	{ return (gdd*)(dd=(gddAtomic*)next()); }
inline gdd* gddCursor::next(gddContainer*& dd)
	{ return (gdd*)(dd=(gddContainer*)next()); }

inline gdd* gddCursor::current(void)
	{ return curr; }
inline gdd* gddCursor::current(gddScalar*& dd)
	{ return (gdd*)(dd=(gddScalar*)current()); }
inline gdd* gddCursor::current(gddAtomic*& dd)
	{ return (gdd*)(dd=(gddAtomic*)current()); }
inline gdd* gddCursor::current(gddContainer*& dd)
	{ return (gdd*)(dd=(gddContainer*)current()); }

#endif
