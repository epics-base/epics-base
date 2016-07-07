/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// Author: Jim Kowalkowski
// Date: 2/96

#define epicsExportSharedSymbols
#include "gddAppTable.h"

#if 0
#ifndef GDD_NO_EPICS
#include "dbrMapper.h"
#endif
#endif

int main(int argc, char* argv[])
{
	FILE* fd;

	gddApplicationTypeTable& tt = gddApplicationTypeTable::AppTable();

	if(argc<2)
	{
		fprintf(stderr,"You must enter a file name on command line\n");
		return -1;
	}

	if((fd=fopen(argv[1],"w"))==NULL)
	{
		fprintf(stderr,"Cannot open file %s\n",argv[1]);
		return -1;
	}

#if 0
#ifndef GDD_NO_EPICS
	gddMakeMapDBR(tt);
#endif
#endif

	tt.describe(fd);
	fclose(fd);

	return 0;
}

