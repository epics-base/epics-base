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
#include <epicsPrint.h>
#include <errMdef.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <dbBase.h>
#include <gpHash.h>

DBBASE *pdbbase = NULL;

#define MAX_PATH_LENGTH	256

static void addPath(char *path,char *newdir)
{ 
    if((strlen(path)+strlen(newdir)+2) > (size_t)MAX_PATH_LENGTH) {
	fprintf(stderr,"path > 256 characters\n");
	exit(-1);
    }
    if(strlen(path) > (size_t)0) strcat(path,":");
    strcat(path,newdir);
}

int main(int argc,char **argv)
{
    long	status;
    dbMenu	*pdbMenu;
    char	*outFilename;
    char	*pext;
    FILE	*outFile;
    char	*plastSlash;

    int		strip;
    char	path[MAX_PATH_LENGTH];
    int		i;

    /*Look for path, i.e. -I dir or -Idir*/
    path[0] = 0;
    while(strncmp(argv[1],"-I",2)==0) {
	if(strlen(argv[1])==2) {
	    addPath(path,argv[2]);
	    strip = 2;
	} else {
	    addPath(path,argv[1]+2);
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
    status = dbReadDatabase(&pdbbase,argv[1],path);
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
	    fprintf(outFile,"\t%s,\n",pdbMenu->papChoiceName[i]);
	}
	fprintf(outFile,"}%s;\n",pdbMenu->name);
	fprintf(outFile,"#endif /*INC%sH*/\n",pdbMenu->name);
	pdbMenu = (dbMenu *)ellNext(&pdbMenu->node);
    }
    fclose(outFile);
    free((void *)outFilename);
    return(0);
}
