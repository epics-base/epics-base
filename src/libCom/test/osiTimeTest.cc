/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/


#include <osiTime.h>
#include <winsock.h>

void main ()
{
	unsigned i;
	osiTime begin = osiTime::getCurrent();
	const unsigned iter = 1000000u;

	for (i=0;i<iter;i++) {
		osiTime tmp = osiTime::getCurrent();
	}

	osiTime end = osiTime::getCurrent();

	printf("elapsed per call to osiTime::getCurrent() = %f\n", ((double) (end-begin))/iter);
}