/* dbToMenu.c */
/*	Author: Marty Kraimer	Date: 11Sep95	*/
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
 * .01	11Sep95	mrk	Initial Implementation
*/
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "errMdef.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "dbBase.h"
#include "gpHash.h"
#include "osiFileName.h"

DBBASE *pdbbase = NULL;

int main(int argc,char **argv)
{
    dbMenu	*pdbMenu;
    char	*outFilename;
    char	*pext;
    FILE	*outFile;
    char	*plastSlash;
    int		i;
    int		strip;
    char	*path = NULL;
    char	*sub = NULL;
    int		pathLength = 0;
    int		subLength = 0;
    char	**pstr;
    char	*psep;
    int		*len;
    long	status;
    static char *pathSep = OSI_PATH_LIST_SEPARATOR;
    static char *subSep = ",";

    /*Look for options*/
    if(argc<2) {
	fprintf(stderr,"usage: dbToMenu -Idir -Idir file.dbd\n");
	exit(0);
    }
    while((strncmp(argv[1],"-I",2)==0)||(strncmp(argv[1],"-S",2)==0)) {
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
    if(argc!=2 || (strncmp(argv[1],"-",1)==0)) {
	fprintf(stderr,"usage: dbToMenu -Idir -Idir file.dbd\n");
	exit(0);
    }
    /*remove path so that outFile is created where program is executed*/
    plastSlash = strrchr(argv[1],'/');
    plastSlash = (plastSlash ? plastSlash+1 : argv[1]);
    outFilename = dbCalloc(1,strlen(plastSlash)+1);
    strcpy(outFilename,plastSlash);
    pext = strstr(outFilename,".dbd");
    if(!pext) {
	fprintf(stderr,"Input file MUST have .dbd extension\n");
	exit(-1);
    }
    strcpy(pext,".h");
    outFile = fopen(outFilename,"w");
    if(!outFile) {
	errPrintf(0,__FILE__,__LINE__,"Error opening %s\n",outFilename);
	exit(-1);
    }
    pdbbase = dbAllocBase();
    pdbbase->ignoreMissingMenus = TRUE;
    status = dbReadDatabase(&pdbbase,argv[1],path,sub);
    if(status)  {
	fprintf(stderr,"Terminal error For input file %s\n",argv[1]);
	exit(-1);
    }
    pdbMenu = (dbMenu *)ellFirst(&pdbbase->menuList);
    while(pdbMenu) {
	fprintf(outFile,"#ifndef INC%sH\n",pdbMenu->name);
	fprintf(outFile,"#define INC%sH\n",pdbMenu->name);
	fprintf(outFile,"typedef enum {\n");
	for(i=0; i<pdbMenu->nChoice; i++) {
	    fprintf(outFile,"\t%s",pdbMenu->papChoiceName[i]);
	    if(i < (pdbMenu->nChoice - 1)) fprintf(outFile,",");
	    fprintf(outFile,"\n");
	}
	fprintf(outFile,"}%s;\n",pdbMenu->name);
	fprintf(outFile,"#endif /*INC%sH*/\n",pdbMenu->name);
	pdbMenu = (dbMenu *)ellNext(&pdbMenu->node);
    }
    fclose(outFile);
    free((void *)outFilename);
    return(0);
}
