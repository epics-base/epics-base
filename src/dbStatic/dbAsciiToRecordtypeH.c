/* dbAsciiToRecordtypeH.c */
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

int main(int argc,char **argv)
{
    long	status;
    char	*outFilename;
    char	*pext;
    FILE	*outFile;
    int		i;
    dbMenu	*pdbMenu;
    dbRecDes	*pdbRecDes;
    dbFldDes	*pdbFldDes;
    int		isdbCommonRecord = FALSE;
    char	*plastSlash;

    if(argc!=2) {
	fprintf(stderr,"usage: dbAsciiToMenuH file.ascii\n");
	exit(-1);
    }
    plastSlash = strrchr(argv[1],'/');
    plastSlash = (plastSlash ? plastSlash+1 : argv[1]);
    outFilename = dbCalloc(1,strlen(plastSlash)+1);
    strcpy(outFilename,plastSlash);
    pext = strstr(outFilename,".ascii");
    if(!pext) {
	fprintf(stderr,"Input file MUST have .ascii extension\n");
	exit(-1);
    }
    strcpy(pext,".h");
    if(strcmp(outFilename,"dbCommonRecord.h")==0) {
	strcpy(outFilename,"dbCommon.h");
	isdbCommonRecord = TRUE;
    }
    outFile = fopen(outFilename,"w");
    if(!outFile) {
	errPrintf(0,__FILE__,__LINE__,"Error opening %s\n",outFilename);
	exit(-1);
    }
    pdbbase = dbAllocBase();
    pdbbase->ignoreMissingMenus = TRUE;
    status = dbAsciiRead(&pdbbase,argv[1]);
    if(status)  {
	epicsPrintf("Terminal error For input file %s\n",argv[1]);
	exit(-1);
    }
    fprintf(outFile,"#include <vxWorks.h>\n");
    fprintf(outFile,"#include <semLib.h>\n");
    fprintf(outFile,"#include \"ellLib.h\"\n");
    fprintf(outFile,"#include \"fast_lock.h\"\n");
    fprintf(outFile,"#include \"link.h\"\n");
    fprintf(outFile,"#include \"tsDefs.h\"\n");
    pdbMenu = (dbMenu *)ellFirst(&pdbbase->menuList);
    while(pdbMenu) {
	fprintf(outFile,"\n#ifndef INC%sH\n",pdbMenu->name);
	fprintf(outFile,"#define INC%sH\n",pdbMenu->name);
	fprintf(outFile,"typedef enum {\n");
	for(i=0; i<pdbMenu->nChoice; i++) {
	    fprintf(outFile,"\t%s,\n",pdbMenu->papChoiceName[i]);
	}
	fprintf(outFile,"}%s;\n",pdbMenu->name);
	fprintf(outFile,"#endif /*INC%sH*/\n",pdbMenu->name);
	pdbMenu = (dbMenu *)ellNext(&pdbMenu->node);
    }
    pdbRecDes = (dbRecDes *)ellFirst(&pdbbase->recDesList);
    while(pdbRecDes) {
	fprintf(outFile,"#ifndef INC%sH\n",pdbRecDes->name);
	fprintf(outFile,"#define INC%sH\n",pdbRecDes->name);
	fprintf(outFile,"typedef struct %s",pdbRecDes->name);
	if(!isdbCommonRecord) fprintf(outFile,"Record");
	fprintf(outFile," {\n");
	for(i=0; i<pdbRecDes->no_fields; i++) {
	    char	name[256];
	    int		j;

	    pdbFldDes = pdbRecDes->papFldDes[i];
	    for(j=0; j< (int)strlen(pdbFldDes->name); j++)
		name[j] = tolower(pdbFldDes->name[j]);
	    name[strlen(pdbFldDes->name)] = 0;
	    switch(pdbFldDes->field_type) {
		case DBF_STRING :
		    fprintf(outFile,"\tchar\t\t%s[%d]; /*%s*/\n",
			name,pdbFldDes->size,pdbFldDes->prompt);
		    break;
		case DBF_CHAR :
		    fprintf(outFile,"\tchar\t\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_UCHAR :
		    fprintf(outFile,"\tunsigned char\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_SHORT :
		    fprintf(outFile,"\tshort\t\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_USHORT :
		    fprintf(outFile,"\tunsigned short\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_LONG :
		    fprintf(outFile,"\tlong\t\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_ULONG :
		    fprintf(outFile,"\tunsigned long\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_FLOAT :
		    fprintf(outFile,"\tfloat\t\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_DOUBLE :
		    fprintf(outFile,"\tdouble\t\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_ENUM :
		case DBF_MENU :
		case DBF_DEVICE :
		    fprintf(outFile,"\tunsigned short\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_INLINK :
		case DBF_OUTLINK :
		case DBF_FWDLINK :
		    fprintf(outFile,"\tDBLINK\t\t%s;\t/*%s*/\n",
			name,pdbFldDes->prompt);
		    break;
		case DBF_NOACCESS:
		    fprintf(outFile,"\t%s;\t/*%s*/\n",
			pdbFldDes->extra,pdbFldDes->prompt);
		    break;
		default:
		    fprintf(outFile,"ILLEGAL FIELD TYPE\n");
	    }
	}
	fprintf(outFile,"} %s",pdbRecDes->name);
	if(!isdbCommonRecord) fprintf(outFile,"Record");
	fprintf(outFile,";\n");
	if(!isdbCommonRecord) {
	    for(i=0; i<pdbRecDes->no_fields; i++) {
		pdbFldDes = pdbRecDes->papFldDes[i];
		fprintf(outFile,"#define %sRecord%s\t%d\n",
		    pdbRecDes->name,pdbFldDes->name,pdbFldDes->indRecDes);
	    }
	}
	fprintf(outFile,"#endif /*INC%sH*/\n",pdbRecDes->name);
	pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node);
	if(pdbRecDes) fprintf(outFile,"\n");
    }
    if(!isdbCommonRecord) {
	fprintf(outFile,"#ifdef GEN_SIZE_OFFSET\n");
	pdbRecDes = (dbRecDes *)ellFirst(&pdbbase->recDesList);
	while(pdbRecDes) {
	    fprintf(outFile,"int %sRecordSizeOffset(dbRecDes *pdbRecDes)\n{\n",
		pdbRecDes->name);
	    fprintf(outFile,"    %sRecord *prec = 0;\n",pdbRecDes->name);
	    for(i=0; i<pdbRecDes->no_fields; i++) {
		char	name[256];
		int		j;

		pdbFldDes = pdbRecDes->papFldDes[i];
		for(j=0; j< (int)strlen(pdbFldDes->name); j++)
		    name[j] = tolower(pdbFldDes->name[j]);
		name[strlen(pdbFldDes->name)] = 0;
		fprintf(outFile,
		"  pdbRecDes->papFldDes[%d]->size=sizeof(prec->%s);\n",
		    i,name);
		fprintf(outFile,"  pdbRecDes->papFldDes[%d]->offset=",i);
		fprintf(outFile,
		    "(short)((char *)&prec->%s - (char *)prec);\n",name);
	    }
	    fprintf(outFile,"    pdbRecDes->rec_size = sizeof(*prec);\n");
	    fprintf(outFile,"    return(0);\n");
	    fprintf(outFile,"}\n");
	    pdbRecDes = (dbRecDes *)ellNext(&pdbRecDes->node);
	}
	fprintf(outFile,"#endif /*GEN_SIZE_OFFSET*/\n");
    }
    fclose(outFile);
    free((void *)outFilename);
    return(0);
}
