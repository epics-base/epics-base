/* dbExpandInclude.c */
/*	Author: Marty Kraimer	Date: 30NOV95	*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/* Modification Log:
 * -----------------
 * .01	30NOV95	mrk	Initial Implementation
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <epicsPrint.h>
#include <errMdef.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <dbBase.h>
#include <gpHash.h>

DBBASE *pdbbase = NULL;

int main(int argc,char **argv)
{
    int		arg,strip;
    char	*path=0;
    long	status;
    int		i;

        /*Look for path, i.e. -I path or -Ipath*/
    for(arg=1; arg<argc; arg++) {
	if(strncmp(argv[arg],"-I",2)!=0) continue;
	if(strlen(argv[arg])==2) {
	    path = argv[arg+1];
	    strip = 2;
	} else {
	    path = argv[arg] + 2;
	    strip = 1;
	}
	argc -= strip;
	for(i=arg; i<argc; i++) argv[i] = argv[i + strip];
	break;
    }
    if(argc<2) {
	printf("usage: dbExpandInclude -Ipath file1.db file2.db ...\n");
	exit(0);
    }
    for(i=1; i<argc; i++) {
	status = dbReadDatabase(&pdbbase,argv[i],path);
	if(!status) continue;
	fprintf(stderr,"For input file %s",argv[i]);
	errMessage(status,"from dbReadDatabase");
    }
    dbWriteMenuFP(pdbbase,stdout,0);
    dbWriteRecDesFP(pdbbase,stdout,0);
    dbWriteDeviceFP(pdbbase,stdout);
    dbWriteDriverFP(pdbbase,stdout);
    dbWriteBreaktableFP(pdbbase,stdout);
    dbWriteRecordFP(pdbbase,stdout,0,0);
    return(0);
}
