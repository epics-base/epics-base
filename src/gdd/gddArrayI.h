
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