#ifndef GDD_ARRAY_H
#define GDD_ARRAY_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 * $Id$
 * $Log$
 *
 * ***********************************************************************
 * Adds ability to put array data into a DD, get it out, and adjust it
 * ***********************************************************************
 */

#define gddAtomic gddArray

class gddArray : public gdd
{
public:
	gddArray(void) { }
	gddArray(gddArray* ad) : gdd(ad) { }
	gddArray(int app) : gdd(app) { }
	gddArray(int app, aitEnum prim) : gdd(app,prim) { }
	gddArray(int app, aitEnum prim, int dimen, aitUint32* size_array):
		gdd(app,prim,dimen,size_array) { }
	gddArray(int app, aitEnum prim, int dimen, ...);

	// view dimension size info as a bounding box instead of first/count
	gddStatus getBoundingBoxSize(aitUint32* put_info_here);
	gddStatus setBoundingBoxSize(const aitUint32* const get_info_from_here);
	gddStatus getBoundingBoxOrigin(aitUint32* put_info_here);
	gddStatus setBoundingBoxOrigin(const aitUint32* const get_info_from_here);

	void dump(void);
	void test(void);

	gddArray& operator=(aitFloat64* v) { *((gdd*)this)=v; return *this; }
	gddArray& operator=(aitFloat32* v) { *((gdd*)this)=v; return *this; }
	gddArray& operator=(aitUint32* v) { *((gdd*)this)=v; return *this; }
	gddArray& operator=(aitInt32* v) { *((gdd*)this)=v; return *this; }
	gddArray& operator=(aitUint16* v) { *((gdd*)this)=v; return *this; }
	gddArray& operator=(aitInt16* v) { *((gdd*)this)=v; return *this; }
	gddArray& operator=(aitUint8* v) { *((gdd*)this)=v; return *this; }
	gddArray& operator=(aitInt8* v) { *((gdd*)this)=v; return *this; }

protected:
	~gddArray(void) { }
private:
};

#endif
