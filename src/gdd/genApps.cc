// Author: Jim Kowalkowski
// Date: 2/96
// 
// $Id$
// 
// $Log$
//

// *Revision 1.2  1996/06/24 03:15:40  jbk
// *name changes and fixes for aitString and fixed string functions
// *Revision 1.1  1996/05/31 13:15:37  jbk
// *add new stuff

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

