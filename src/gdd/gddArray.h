#ifndef GDD_ARRAY_H
#define GDD_ARRAY_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 * $Id$
 * $Log$
 * Revision 1.3  1999/04/30 15:24:53  jhill
 * fixed improper container index bug
 *
 * Revision 1.2  1997/04/23 17:13:03  jhill
 * fixed export of symbols from WIN32 DLL
 *
 * Revision 1.1  1997/03/21 01:56:05  jbk
 * *** empty log message ***
 *
 *
 * ***********************************************************************
 * Adds ability to put array data into a DD, get it out, and adjust it
 * ***********************************************************************
 */

#include "shareLib.h"

#define gddAtomic gddArray

class epicsShareClass gddArray : public gdd
{
public:
	gddArray(void);
	gddArray(gddArray* ad);
	gddArray(int app);
	gddArray(int app, aitEnum prim);
	gddArray(int app, aitEnum prim, int dimen, aitUint32* size_array);
	gddArray(int app, aitEnum prim, int dimen, ...);

	// view dimension size info as a bounding box instead of first/count
	gddStatus getBoundingBoxSize(aitUint32* put_info_here) const;
	gddStatus setBoundingBoxSize(const aitUint32* const get_info_from_here);
	gddStatus getBoundingBoxOrigin(aitUint32* put_info_here) const;
	gddStatus setBoundingBoxOrigin(const aitUint32* const get_info_from_here);

	void dump(void) const;
	void test(void);

	gddArray& operator=(aitFloat64* v);
	gddArray& operator=(aitFloat32* v);
	gddArray& operator=(aitUint32* v);
	gddArray& operator=(aitInt32* v);
	gddArray& operator=(aitUint16* v);
	gddArray& operator=(aitInt16* v);
	gddArray& operator=(aitUint8* v);
	gddArray& operator=(aitInt8* v);

protected:
	~gddArray(void);
private:
};

#endif
