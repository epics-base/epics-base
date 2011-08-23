/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_SCALAR_H
#define GDD_SCALAR_H

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 */

#include "shareLib.h"

// ------------------------------------------------------------------------
// Add handling of the special case where the data is a scaler - the
// dimension is zero

class epicsShareClass gddScalar : public gdd
{
public:
	gddScalar(void);
	gddScalar(gddScalar* ad);
	gddScalar(int app);
	gddScalar(int app,aitEnum prim);

	void dump(void) const;
	void test(void);

	gddScalar& operator=(aitFloat64 d);
	gddScalar& operator=(aitFloat32 d);
	gddScalar& operator=(aitUint32 d);
	gddScalar& operator=(aitInt32 d);
	gddScalar& operator=(aitUint16 d);
	gddScalar& operator=(aitInt16 d);
	gddScalar& operator=(aitUint8 d);
	gddScalar& operator=(aitInt8 d);

protected:
	gddScalar(int app, aitEnum prim, int dimen, aitUint32* size_array);
	~gddScalar(void);

private:
};

#endif
