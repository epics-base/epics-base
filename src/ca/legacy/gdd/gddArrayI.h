/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef GDD_ARRAYI_H
#define GDD_ARRAYI_H

inline gddArray::gddArray (void) { }
inline gddArray::gddArray (gddArray* ad) : gdd(ad) { }
inline gddArray::gddArray (int app) : gdd(app) { }
inline gddArray::gddArray (int app, aitEnum prim) : gdd(app,prim) { }
inline gddArray::gddArray (int app, aitEnum prim, int dimen, aitUint32* size_array):
	gdd(app,prim,dimen,size_array) { }
inline gddArray& gddArray::operator=(aitFloat64* v) { *((gdd*)this)=v; return *this; }
inline gddArray& gddArray::operator=(aitFloat32* v) { *((gdd*)this)=v; return *this; }
inline gddArray& gddArray::operator=(aitUint32* v) { *((gdd*)this)=v; return *this; }
inline gddArray& gddArray::operator=(aitInt32* v) { *((gdd*)this)=v; return *this; }
inline gddArray& gddArray::operator=(aitUint16* v) { *((gdd*)this)=v; return *this; }
inline gddArray& gddArray::operator=(aitInt16* v) { *((gdd*)this)=v; return *this; }
inline gddArray& gddArray::operator=(aitUint8* v) { *((gdd*)this)=v; return *this; }
inline gddArray& gddArray::operator=(aitInt8* v) { *((gdd*)this)=v; return *this; }

inline gddArray::~gddArray(void) { }

#endif
