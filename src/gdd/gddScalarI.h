/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef GDD_SCALARI_H
#define GDD_SCALARI_H

inline gddScalar::gddScalar(void) { }
inline gddScalar::gddScalar(gddScalar* ad) : gdd(ad) { }
inline gddScalar::gddScalar(int app) : gdd(app) { }
inline gddScalar::gddScalar(int app,aitEnum prim) : gdd(app,prim) { }

inline gddScalar& gddScalar::operator=(aitFloat64 d) { *((gdd*)this)=d; return *this; }
inline gddScalar& gddScalar::operator=(aitFloat32 d) { *((gdd*)this)=d; return *this; }
inline gddScalar& gddScalar::operator=(aitUint32 d) { *((gdd*)this)=d; return *this; }
inline gddScalar& gddScalar::operator=(aitInt32 d) { *((gdd*)this)=d; return *this; }
inline gddScalar& gddScalar::operator=(aitUint16 d) { *((gdd*)this)=d; return *this; }
inline gddScalar& gddScalar::operator=(aitInt16 d) { *((gdd*)this)=d; return *this; }
inline gddScalar& gddScalar::operator=(aitUint8 d) { *((gdd*)this)=d; return *this; }
inline gddScalar& gddScalar::operator=(aitInt8 d) { *((gdd*)this)=d; return *this; }

inline gddScalar::gddScalar(int app, aitEnum prim, int dimen, aitUint32* size_array):
	gdd(app,prim,dimen,size_array) { }

inline gddScalar::~gddScalar(void) { }

#endif
