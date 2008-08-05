/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbToRecordtypeH.c */
/*	Author: Marty Kraimer	Date: 11Sep95	*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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
    int		i;
    char	*outFilename;
    char	*pext;
    FILE	*outFile;
    dbMenu	*pdbMenu;
    dbRecordType	*pdbRecordType;
    dbFldDes	*pdbFldDes;
    dbText	*pdbCdef;
    int		isdbCommonRecord = FALSE;
    char	*plastSlash;
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
	fprintf(stderr,"usage: dbToRecordtypeH -Idir -Idir file.dbd [outfile]\n");
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
	fprintf(stderr,"usage: dbToRecordtypeH -Idir -Idir file.dbd [outfile]\n");
	exit(0);
    }
    if(argc==2){
    /*remove path so that outFile is created where program is executed*/
    plastSlash = strrchr(argv[1],'/');
    if(!plastSlash)  plastSlash = strrchr(argv[1],'\\');
    plastSlash = (plastSlash ? plastSlash+1 : argv[1]);
    outFilename = dbCalloc(1,strlen(plastSlash)+1);
    strcpy(outFilename,plastSlash);
    pext = strstr(outFilename,".dbd");
    if(!pext) {
	fprintf(stderr,"Input file MUST have .dbd extension\n");
	exit(-1);
    }
    strcpy(pext,".h");
    if(strcmp(outFilename,"dbCommonRecord.h")==0) {
	strcpy(outFilename,"dbCommon.h");
	isdbCommonRecord = TRUE;
    }
    }else {
    outFilename = dbCalloc(1,strlen(argv[2])+1);
    strcpy(outFilename,argv[2]);
    if(strstr(outFilename,"dbCommon.h")!=0) {
	isdbCommonRecord = TRUE;
    }
    }
    pdbbase = dbAllocBase();
    pdbbase->ignoreMissingMenus = TRUE;
    pdbbase->loadCdefs = TRUE;
    status = dbReadDatabase(&pdbbase,argv[1],path,sub);
    if(status)  {
        errlogFlush();
        fprintf(stderr, "dbToMenuH: Input errors, no output generated\n");
        exit(1);
    }
    outFile = fopen(outFilename,"w");
    if(!outFile) {
        epicsPrintf("Error creating output file \"%s\"\n", outFilename);
        exit(1);
    }

    pdbMenu = (dbMenu *)ellFirst(&pdbbase->menuList);
    while(pdbMenu) {
	fprintf(outFile,"\n#ifndef INC%sH\n",pdbMenu->name);
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
    pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    while(pdbRecordType) {
        fprintf(outFile,"#ifndef INC%sH\n",pdbRecordType->name);
        fprintf(outFile,"#define INC%sH\n",pdbRecordType->name);
	pdbCdef = (dbText *)ellFirst(&pdbRecordType->cdefList);
	while (pdbCdef) {
	    fprintf(outFile,"%s\n",pdbCdef->text);
	    pdbCdef = (dbText *)ellNext(&pdbCdef->node);
	}
	fprintf(outFile,"typedef struct %s",pdbRecordType->name);
	if(!isdbCommonRecord) fprintf(outFile,"Record");
	fprintf(outFile," {\n");
	for(i=0; i<pdbRecordType->no_fields; i++) {
	    char	name[256];
	    int		j;

	    pdbFldDes = pdbRecordType->papFldDes[i];
	    for(j=0; j< (int)strlen(pdbFldDes->name); j++)
		name[j] = tolower(pdbFldDes->name[j]);
	    name[strlen(pdbFldDes->name)] = 0;
	    switch(pdbFldDes->field_type) {
		case DBF_STRING :
		    fprintf(outFile, "\tchar\t\t%s[%d];\t/* %s */\n",
			name, pdbFldDes->size, pdbFldDes->prompt);
		    break;
		case DBF_CHAR :
		    fprintf(outFile, "\tepicsInt8\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_UCHAR :
		    fprintf(outFile, "\tepicsUInt8\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_SHORT :
		    fprintf(outFile, "\tepicsInt16\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_USHORT :
		    fprintf(outFile, "\tepicsUInt16\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_LONG :
		    fprintf(outFile, "\tepicsInt32\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_ULONG :
		    fprintf(outFile, "\tepicsUInt32\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_FLOAT :
		    fprintf(outFile, "\tepicsFloat32\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_DOUBLE :
		    fprintf(outFile, "\tepicsFloat64\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_ENUM :
		case DBF_MENU :
		case DBF_DEVICE :
		    fprintf(outFile, "\tepicsEnum16\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_INLINK :
		case DBF_OUTLINK :
		case DBF_FWDLINK :
		    fprintf(outFile, "\tDBLINK\t\t%s;\t/* %s */\n",
			name, pdbFldDes->prompt);
		    break;
		case DBF_NOACCESS:
		    fprintf(outFile, "\t%s;\t/* %s */\n",
			pdbFldDes->extra, pdbFldDes->prompt);
		    break;
		default:
		    fprintf(outFile,"ILLEGAL FIELD TYPE\n");
	    }
	}
	fprintf(outFile,"} %s",pdbRecordType->name);
	if(!isdbCommonRecord) fprintf(outFile,"Record");
	fprintf(outFile,";\n");
	if(!isdbCommonRecord) {
	    for(i=0; i<pdbRecordType->no_fields; i++) {
		pdbFldDes = pdbRecordType->papFldDes[i];
		fprintf(outFile,"#define %sRecord%s\t%d\n",
		    pdbRecordType->name,pdbFldDes->name,pdbFldDes->indRecordType);
	    }
	}
	fprintf(outFile,"#endif /*INC%sH*/\n",pdbRecordType->name);
	pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node);
	if(pdbRecordType) fprintf(outFile,"\n");
    }
    if(!isdbCommonRecord) {
	fprintf(outFile,"#ifdef GEN_SIZE_OFFSET\n");
	fprintf(outFile,"#ifdef __cplusplus\n");
	fprintf(outFile,"extern \"C\" {\n");
	fprintf(outFile,"#endif\n");
        fprintf(outFile,"#include <epicsExport.h>\n");
	pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
	while(pdbRecordType) {
		fprintf(outFile,"static int %sRecordSizeOffset(dbRecordType *pdbRecordType)\n{\n",
		pdbRecordType->name);
	    fprintf(outFile,"    %sRecord *prec = 0;\n",pdbRecordType->name);
	    for(i=0; i<pdbRecordType->no_fields; i++) {
		char	name[256];
		int		j;

		pdbFldDes = pdbRecordType->papFldDes[i];
		for(j=0; j< (int)strlen(pdbFldDes->name); j++)
		    name[j] = tolower(pdbFldDes->name[j]);
		name[strlen(pdbFldDes->name)] = 0;
		fprintf(outFile,
		"  pdbRecordType->papFldDes[%d]->size=sizeof(prec->%s);\n",
		    i,name);
		fprintf(outFile,"  pdbRecordType->papFldDes[%d]->offset=",i);
		fprintf(outFile,
		    "(short)((char *)&prec->%s - (char *)prec);\n",name);
	    }
	    fprintf(outFile,"    pdbRecordType->rec_size = sizeof(*prec);\n");
	    fprintf(outFile,"    return(0);\n");
	    fprintf(outFile,"}\n");
	    fprintf(outFile,"epicsExportRegistrar(%sRecordSizeOffset);\n",
		pdbRecordType->name);
	    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node);
	}
	fprintf(outFile,"#ifdef __cplusplus\n");
	fprintf(outFile,"}\n");
	fprintf(outFile,"#endif\n");
	fprintf(outFile,"#endif /*GEN_SIZE_OFFSET*/\n");
    }
    fclose(outFile);
    free((void *)outFilename);
    return(0);
}
