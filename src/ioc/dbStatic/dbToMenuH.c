/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbToMenu.c */
/*	Author: Marty Kraimer	Date: 11Sep95	*/
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
	fprintf(stderr,"usage: dbToMenu -Idir -Idir file.dbd [outfile]\n");
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
    if(argc<2 || (strncmp(argv[1],"-",1)==0)) {
	fprintf(stderr,"usage: dbToMenu -Idir -Idir file.dbd [outfile]\n");
	exit(0);
    }
    if (argc==2) {
        /*remove path so that outFile is created where program is executed*/
        plastSlash = strrchr(argv[1],'/');
        if(!plastSlash)  plastSlash = strrchr(argv[1],'\\');
        plastSlash = (plastSlash ? plastSlash+1 : argv[1]);
        outFilename = dbCalloc(1,strlen(plastSlash)+1);
        strcpy(outFilename,plastSlash);
        pext = strstr(outFilename,".dbd");
        if (!pext) {
            fprintf(stderr,"Input file MUST have .dbd extension\n");
            exit(-1);
        }
        strcpy(pext,".h");
    } else {
        outFilename = dbCalloc(1,strlen(argv[2])+1);
        strcpy(outFilename,argv[2]);
    }
    pdbbase = dbAllocBase();
    pdbbase->ignoreMissingMenus = TRUE;
    status = dbReadDatabase(&pdbbase,argv[1],path,sub);
    if (status) {
        errlogFlush();
        fprintf(stderr, "dbToMenuH: Input errors, no output generated\n");
        exit(1);
    }
    outFile = fopen(outFilename, "w");
    if (!outFile) {
        epicsPrintf("Error creating output file \"%s\"\n", outFilename);
        exit(1);
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
