/* dbAsciiExpand.c */
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
    long	status;
    int		i;

    if(argc<2) {
	printf("usage: dbAsciiExpand file1.ascii fi;e2.ascii ...\n");
	exit(0);
    }
    for(i=1; i<argc; i++) {
	status = dbAsciiRead(&pdbbase,argv[i]);
	if(!status) continue;
	epicsPrintf("For input file %s",argv[i]);
	errMessage(status,"from dbAsciiInitialize");
    }
    dbWriteMenu(pdbbase,0);
    dbWriteRecDes(pdbbase,0);
    dbWriteDevice(pdbbase);
    dbWriteDriver(pdbbase);
    return(0);
}
