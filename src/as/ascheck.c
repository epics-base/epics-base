/* share/src/as/ascheck.c */
/* share/src/as $Id$ */
/*	Author: Marty Kraimer	Date: 03-24-94	*/
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
 * .01	03-24-94	mrk	Initial Implementation
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "errlog.h"
#include "asLib.h"
int main(int argc,char **argv)
{
    int         i;
    int		strip;
    char	*sub = NULL;
    int		subLength = 0;
    char	**pstr;
    char	*psep;
    int		*len;
    long	status;
    static char *subSep = ",";

    /*Look for options*/
    while(argc>1 && (strncmp(argv[1],"-S",2)==0)) {
	pstr = &sub;
	psep = subSep;
	len = &subLength;
	if(strlen(argv[1])==2) {
	    dbCatString(pstr,len,argv[2],psep);
	    strip = 2;
	} else {
	    dbCatString(pstr,len,argv[1]+2,psep);
	    strip = 1;
	}
	argc -= strip;
	for(i=1; i<argc; i++) argv[i] = argv[i + strip];
    }
    if(argc!=1) {
	printf("usage: ascheck -Smacsub < file\n");
	exit(0);
    }
    status = asInitFP(stdin,sub);
    if(status) errMessage(status,"from asInitFP");
    return(0);
}
