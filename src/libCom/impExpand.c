/* impExpand.c */
/*	Author: Marty Kraimer	Date: 18SEP96 */
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
 * .01	18SEP96 mrk	Initial Implementation
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "impLib.h"

#define MAX_SIZE	256

int main(int argc,char **argv)
{
    IMP_HANDLE	handle;
    int		narg,i;
    char	*parm;
    char	buffer[MAX_SIZE];

    /*Look for options*/
    if(argc<2) {
	fprintf(stderr,
	    "usage: impExpand -Idir -Idir "
		"-S substitutions -S substitutions"
		" file1 file2 ...\n");
	exit(0);
    }
    if(impInit(&handle,MAX_SIZE)!=impSuccess) {
	fprintf(stderr,"impInit failed\n");
	exit(0);
    }
    while((strncmp(argv[1],"-I",2)==0)||(strncmp(argv[1],"-S",2)==0)) {
	if(strlen(argv[1])==2) {
	    parm = argv[2];
	    narg = 2;
	} else {
	    parm = argv[1]+2;
	    narg = 1;
	}
	if(strncmp(argv[1],"-I",2)==0) {
	    if(impAddPath(handle,parm)!=impSuccess) {
		fprintf(stderr,"impAddPath failed\n");
		exit(0);
	    }
	} else {
	    if(impMacAddSubstitutions(handle,parm)!=impSuccess) {
		fprintf(stderr,"impMacAddSubstitutions failed\n");
		exit(0);
	    }
	}
	argc -= narg;
	for(i=1; i<argc; i++) argv[i] = argv[i + narg];
    }
    if(argc<2 || (strncmp(argv[1],"-",1)==0)) {
	fprintf(stderr,
	    "usage: impExpand -Idir -Idir "
		"-S substitutions -S substitutions"
		" file1.dbd file2.dbd ...\n");
	exit(0);
    }
    for(i=1; i<argc; i++) {
	if(impOpenFile(handle,argv[i])!=impSuccess) {
	    fprintf(stderr,"impOpenFile failed for %s\n",argv[i]);
	    exit(0);
	}
	while(impGetLine(handle,buffer,MAX_SIZE)) printf("%s",buffer);
	if(impCloseFile(handle)!=impSuccess) {
	    fprintf(stderr,"impCloseFile failed for %s\n",argv[i]);
	}
    }
    impFree(handle);
    return(0);
}
