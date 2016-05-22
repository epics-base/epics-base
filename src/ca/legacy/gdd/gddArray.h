/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_ARRAY_H
#define GDD_ARRAY_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
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
