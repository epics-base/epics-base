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
// 
// $Id$
// 
// $Log$
// Revision 1.2  1997/04/23 17:13:07  jhill
// fixed export of symbols from WIN32 DLL
//
// Revision 1.1  1996/06/25 19:11:51  jbk
// new in EPICS base
//
//

// *Revision 1.2  1996/06/24 03:15:40  jbk
// *name changes and fixes for aitString and fixed string functions
// *Revision 1.1  1996/05/31 13:15:37  jbk
// *add new stuff

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

