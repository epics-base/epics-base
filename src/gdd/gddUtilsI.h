/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_UTILSI_H
#define GDD_UTILSI_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 * $Id$
 * $Log$
 * Revision 1.3  2002/02/13 14:34:11  lange
 * Fixed (mostly suppressed) HPUX aCC warnings
 *
 * Revision 1.2  1997/03/24 12:41:49  jbk
 * fixed bug in gddDestructor constructor.
 *
 * Revision 1.1  1997/03/21 01:56:12  jbk
 * *** empty log message ***
 *
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
inline gddDestructor::gddDestructor(void* usr_arg) { ref_cnt=0; arg=usr_arg; }
inline void gddDestructor::reference(void)      { ref_cnt++; } // X aCC 818
inline int gddDestructor::refCount(void) const  { return ref_cnt; }

#endif
