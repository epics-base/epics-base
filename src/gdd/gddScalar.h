#ifndef GDD_SCALAR_H
#define GDD_SCALAR_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 * $Id$
 * $Log$
 *
 */

// ------------------------------------------------------------------------
// Add handling of the special case where the data is a scaler - the
// dimension is zero

class gddScalar : public gdd
{
public:
	gddScalar(void) { }
	gddScalar(gddScalar* ad) : gdd(ad) { }
	gddScalar(int app) : gdd(app) { }
	gddScalar(int app,aitEnum prim) : gdd(app,prim) { }

	void dump(void);
	void test(void);

	gddScalar& operator=(aitFloat64 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitFloat32 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitUint32 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitInt32 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitUint16 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitInt16 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitUint8 d) { *((gdd*)this)=d; return *this; }
	gddScalar& operator=(aitInt8 d) { *((gdd*)this)=d; return *this; }

protected:
	gddScalar(int app, aitEnum prim, int dimen, aitUint32* size_array):
		gdd(app,prim,dimen,size_array) { }
	~gddScalar(void) { }

private:
};

#endif
