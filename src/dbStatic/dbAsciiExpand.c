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
    FILE	*fp;

    if(argc!=3) {
	printf("usage: dbAsciiExpand infile outfile\n");
	exit(0);
    }
    status = dbAsciiRead(&pdbbase,argv[1]);
    fp = fopen(argv[2],"w");
    if(!fp) {
	errMessage(0,argv[2]);
	return(-1);
    }
    dbWriteMenu(pdbbase,fp,0);
    dbWriteRecDes(pdbbase,fp,0);
    dbWriteDevice(pdbbase,fp);
    dbWriteDriver(pdbbase,fp);
    status = fclose(fp);
    if(!fp) {
	errMessage(0,argv[2]);
	return(-1);
    }
    return(0);
}
