/* makeDbdDepends.c    */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "errMdef.h"
#include "errlog.h"

#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "osiFileName.h"

epicsShareExtern char *makeDbdDepends;

DBBASE *pdbbase = NULL;

int main(int argc,char **argv)
{
    int         i;
    int        strip;
    char    *path = NULL;
    char    *sub = NULL;
    int        pathLength = 0;
    int        subLength = 0;
    char    **pstr;
    char    *psep;
    int        *len;
    long    status;
    static char *pathSep = OSI_PATH_LIST_SEPARATOR;
    static char *subSep = ",";

    /*Look for options*/
    if(argc<2) {
        printf("usage: makeDbdDepends -Idir -Smacsub target.dbd input.dbd \n");
        exit(0);
    }
    while((strncmp(argv[1],"-I",2)==0)||(strncmp(argv[1],"-S",2)==0)||
        (strncmp(argv[1],"-g",2)==0)) {
        if(strncmp(argv[1],"-I",2)==0) {
            pstr = &path;
            psep = pathSep;
            len = &pathLength;
        } else {
            pstr = &sub;
            psep = subSep;
            len = &subLength;
        }
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
    if(argc<2 || (strncmp(argv[1],"-",1)==0)) {
        printf("usage: makeDbdDepends -Idir -Idir -Smacsub target.dbd input.dbd \n");
        exit(0);
    }
    makeDbdDepends = argv[1];
    for(i=2; i<argc; i++) {
        status = dbReadDatabase(&pdbbase,argv[i],path,sub);
        if(!status) continue;
        fprintf(stderr,"For input file %s",argv[i]);
        /*errMessage(status,"from makeDbdDepends");*/
        errPrintf(-1,__FILE__, __LINE__,"dbReadDatabase Error\n");
    }
    return(0);
}
