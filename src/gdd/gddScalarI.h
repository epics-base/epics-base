
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