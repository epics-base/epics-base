#ifndef GDD_UTILSI_H
#define GDD_UTILSI_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 * $Id$
 * $Log$
 *
 */

// inline gddBounds::gddBounds(aitIndex c) { start=0; count=c; }
// inline gddBounds::gddBounds(aitIndex f, aitIndex c) { start=f; count=c; }

inline void gddBounds::setSize(aitIndex c)			{ count=c; }
inline void gddBounds::setFirst(aitIndex f)			{ start=f; }
inline void gddBounds::set(aitIndex f, aitIndex c)	{ start=f; count=c; }
inline void gddBounds::get(aitIndex& f, aitIndex& c){ f=start; c=count; }
inline aitIndex gddBounds::size(void) const			{ return count; }
inline aitIndex gddBounds::first(void) const		{ return start; }

inline gddDestructor::gddDestructor(void) { ref_cnt=0; arg=NULL; }
inline gddDestructor::gddDestructor(void* usr_arg) { ref_cnt=1; arg=usr_arg; }
inline void gddDestructor::reference(void)      { ref_cnt++; }
inline int gddDestructor::refCount(void) const  { return ref_cnt; }

#endif
