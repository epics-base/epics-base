/* dbls.c - structure listing program */
/* share/src/db $Id$ */

/*
 *      Author:          Bob Zieman
 *      Date:            11-7-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  11-26-91        jba     Initialized fp to null
 * .02  02-28-92        jba     ANSI C changes
 * .03  05-19-92	mrk	Mods for internal database structure changes
 */


#define SAME 0

#include <vxWorks.h>
#include <stdioLib.h>
#include <types.h>
#include <string.h>
#include <dbRecType.h>
#include <choice.h>
#include <cvtTable.h>
#include <devSup.h>
#include <drvSup.h>
#include <recSup.h>
#include <dbBase.h>
#include <dbRecDes.h>
#include <dbRecords.h>
#include <errMdef.h>
#include <error.h>
#include <sdrHeader.h>

extern struct dbBase *pdbBase;

/* forward references */
void            DbRecType();
void            DrvSup();
void            DevSup();
void            DbRecDes();
void            CvtTable();
void            ChoiceRec();
void            ChoiceDev();
void            ChoiceCvt();
void            ChoiceGbl();
void            DbErrDes();
void            DbRecords();
void            RecSup();
int             getSelection();

static char     buffer[512];
static char     ibuff[20];
static int      i;
static int      j;
static int      k;
static int      inum;
static int      begNumI;
static int      endNumI;
static int      endNumJ;
static int      begNumJ;

static struct PRTAB {
    char           *msg;	/* selection string */
    void            (*fp) ();	/* print routine */
}               comtab[] =
{
    {
	"dbRecType", DbRecType
    },
    {
	"drvSup", DrvSup
    },
    {
	"devSup", DevSup
    },
    {
	"dbRecDes", DbRecDes
    },
    {
	"cvtTable", CvtTable
    },
    {
	"choiceRec", ChoiceRec
    },
    {
	"choiceDev", ChoiceDev
    },
    {
	"choiceCvt", ChoiceCvt
    },
    {
	"choiceGbl", ChoiceGbl
    },
    {
	"dbErrDes", DbErrDes
    },
    {
	"recSup", RecSup
    },
    {
	"dbRecords", DbRecords
    },
    {
	"FFFF", NULL
    }
};
/*
 * LISTSTRUCTURES
 *
 * MAIN PROGRAM
 *
 */
dbls()
{
    struct PRTAB   *pcomtab = &comtab[0];
    void            (*fup) ();	/* SUBROUTINE POINTER */
    int             pos,
                    fflag;
    FILE           *fp=NULL,
                   *fopen();
    char            fname[80];
    long            status;
    struct recType *precType;

    if (!(precType=pdbBase->precType)) {
	status = S_sdr_notLoaded;
	errMessage(status, "dbls: Error - precType not loaded");
	return -1;
    }
    begNumI = 0;
    begNumJ = 0;

    pos = getSelection(pcomtab);
    printf("PUT OUTPUT LISTING IN FILE ??? (y|n) : ");
    scanf("%s", ibuff);
    if (ibuff[0] == 'y')
	fflag = 1;
    else
	fflag = 0;

    if (fflag) {
	printf("ENTER FULL_PATH_NAME OF OUTPUT LISTING FILE : ");
	scanf("%s", fname);
	if ((fp = fopen(fname, "a")) == NULL) {
	    printf("dbls: fopen error errno=%d\nfile=%s\n", MYERRNO, fname);
	    printf("TRY:  < cd \"/full_dir_path/.\" > to correct this failure\n");
	    return (-1);
	}
	printf("Warning Note: file %s opened for appending\n", fname);
    }
    fup = *comtab[pos].fp;	/* PREPARE INDIRECT CALL  */
    (*fup) (fp, fflag);		/* AND CALL SUBROUTINE    */
    if (fflag) {
	printf("YOUR OUTPUT IS LOCATED IN FILE %s\n", fname);
	fclose(fp);
    }
    return (0);
}				/* end of main */
getRecTypeSel()
{
    struct recType *precType;

    if (!(precType=pdbBase->precType)) return(0);
    endNumI = precType->number;

    printf("DO YOU WANT TO LIST ALL RECORD TYPES ??? (y|n) : ");
    scanf("%s", ibuff);

    if (ibuff[0] != 'y') {
	FOREVER {
	    printf("\nRECTYPE MENU:\n");
	    for (i = 0; i < precType->number; i++) {
		if (precType->papName[i]) {
		    printf("%d\t%s\n", i, precType->papName[i]);
		}
	    }
	    printf("ENTER SELECTION NUMBER  (X-%d) : ", i - 1);
	    scanf("%d", &inum);
	    if ((precType->papName[inum])
		    && (inum >= 0) && (inum < i)) {
		begNumI = inum;
		endNumI = begNumI + 1;
		break;
	    } else {
		printf("Not a selection, Please try again\n");
	    }
	}
    } else {
	for (i = 0; i < precType->number; i++) {
	    /* start at first record definition */
	    if (precType->papName[i]) {
		begNumI = i;
		break;
	    }
	}
    }
    return;
}				/* end of getRecTypeSel */
getSelection(pcomtab)
    struct PRTAB   *pcomtab;
{
    int             pos;
    FOREVER {
	printf("\nSTRUCTURE LISTING MENU:\n");
	pos = 0;		/* POS OF FIRST CMD IN TABLE */
	while (strncmp(pcomtab[pos].msg, "FFFF", 4)) {	/* DO UNTIL END OF TABLE    */
	    printf("\t%d\t%s\n", pos + 1, pcomtab[pos].msg);
	    pos++;
	}
	printf("ENTER SELECTION  NUMBER  (1-%d) : ", pos);
	scanf("%d", &inum);

	if (inum > 0 && inum <= pos)
	    return (inum - 1);
	else
	    printf("Not a selection, Please try again\n");

    }
}				/* end of getSelection */
bufOut(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    if (!fflag)
	printf("%s\n", buffer);
    else if ((fprintf(fp, "%s\n", buffer)) == EOF) {
	printf("Error writing file with fprintf\n");
	exit(1);
    }
}				/* end of bufOut */

static void
DbRecType(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct recType *precType;

    if(!(precType=pdbBase->precType)) return;
    sprintf(buffer, "\n\ndbls: listing the precType structure\n");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]    precType-> recType", &precType, precType);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t      number\t    [  %d  ]\t/* number of types */",
	    &precType->number, precType->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]  **papName\t\t /* pap to record type */",
	    &precType->papName, precType->papName);
    bufOut(fp, fflag);
    for (i = 0; i < precType->number; i++) {
	if (precType->papName[i]) {
	    sprintf(buffer, "%8x[%8x]    papName->[%d]->\t\"%s\"",
		    &precType->papName[i],
		    precType->papName[i],
		    i, precType->papName[i]);
	    bufOut(fp, fflag);
	}
    }
}				/* end of DbRecType */
static void
ChoiceGbl(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct arrChoiceSet *pchoiceGbl;

    if (!(pchoiceGbl=pdbBase->pchoiceGbl)) {
	printf("ChoiceGbl: Error - choiceGbl not loaded\n");
	return;
    }
    sprintf(buffer, "\n\ndbls: listing the choiceGbl structure\n");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]    pchoiceGbl -> arrChoiceSet", &pchoiceGbl, pchoiceGbl);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\t  number\t\t[%d]\t/* number of choice sets */",
	    &pchoiceGbl->number,
	    pchoiceGbl->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]\t**papChoiceSet -> choiceSet",
	    &pchoiceGbl->papChoiceSet,
	    pchoiceGbl->papChoiceSet);
    bufOut(fp, fflag);
    for (i = 0; i < pchoiceGbl->number; i++) {
	if (pchoiceGbl->papChoiceSet[i]) {
	    sprintf(buffer, "%8x[%8x]\t  papChoiceSet->[%d]\tINDEX[%d]",
		    &pchoiceGbl->papChoiceSet[i],
		    pchoiceGbl->papChoiceSet[i], i, i);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t      number [%d]\t\t/* number of choices */",
		    &pchoiceGbl->papChoiceSet[i]->number,
		    pchoiceGbl->papChoiceSet[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    **papChoice -> \t\"choice string\"",
		    &pchoiceGbl->papChoiceSet[i]->papChoice,
		    pchoiceGbl->papChoiceSet[i]->papChoice);
	    bufOut(fp, fflag);
	    for (j = 0; j < pchoiceGbl->papChoiceSet[i]->number; j++) {
		if (pchoiceGbl->papChoiceSet[i]->papChoice[j]) {
		    sprintf(buffer, "%8x[%8x]\t\t  papChoice->[%d]\t\"%s\"",
			    &pchoiceGbl->papChoiceSet[i]->papChoice[j],
			    pchoiceGbl->papChoiceSet[i]->papChoice[j],
			    j,
			    pchoiceGbl->papChoiceSet[i]->papChoice[j]);
		    bufOut(fp, fflag);
		}
	    }
	}
    }
}				/* end of ChoiceGbl */
static void
ChoiceDev(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct devChoiceRec *pchoiceDev;
    struct recType	*precType;

    if (!(pchoiceDev=pdbBase->pchoiceDev)) {
	printf("ChoiceDev: Error - choiceDev not loaded\n");
	return;
    }
    if(!(precType=pdbBase->precType)) return;
    getRecTypeSel();
    sprintf(buffer, "\n\ndbls: listing the  choiceDev structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]pchoiceDev-> devChoiceRec ", &pchoiceDev, pchoiceDev);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t      number\t[%d]\t\t\t/* number of devChoiceSet */",
	    &pchoiceDev->number, pchoiceDev->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]    **papDevChoiceSet -> devChoiceSet ",
	    &pchoiceDev->papDevChoiceSet,
	    pchoiceDev->papDevChoiceSet);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (pchoiceDev->papDevChoiceSet[i]) {
	    sprintf(buffer, "\n%8x[%8x]\tpapDevChoiceSet->[%d]\t\tRECTYPE \"%s\"",
		    &pchoiceDev->papDevChoiceSet[i],
		    pchoiceDev->papDevChoiceSet[i], i,
		    precType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t   number\t[%d]\t\t/*number of choices */ ",
		    &pchoiceDev->papDevChoiceSet[i]->number,
		    pchoiceDev->papDevChoiceSet[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t   **papDevChoice -> devChoice",
		    &pchoiceDev->papDevChoiceSet[i]->papDevChoice,
		    pchoiceDev->papDevChoiceSet[i]->papDevChoice);
	    bufOut(fp, fflag);
	    for (j = 0; j < pchoiceDev->papDevChoiceSet[i]->number; j++) {
		if (pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]) {
		    sprintf(buffer, "%8x[%8x]\t   papDevChoice[%d]",
			    &pchoiceDev->papDevChoiceSet[i]->papDevChoice[j],
			 pchoiceDev->papDevChoiceSet[i]->papDevChoice[j], j);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t       link_type [%d]",
			    &pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]->link_type,
			    pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]->link_type);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x[%8x]\t\t*pchoice\t\t\"%s\"",
		    &pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice,
		    pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice,
		    pchoiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice);
		    bufOut(fp, fflag);
		}
	    }
	}
    }
}				/* end	ChoiceDev */
static void
ChoiceCvt(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct choiceSet *pchoiceCvt;

    if (!(pchoiceCvt=pdbBase->pchoiceCvt)) {
	printf("ChoiceCvt: Error - choiceCvt not loaded\n");
	return;
    }
    sprintf(buffer, "\n\ndbls: listing the  choiceCvt structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x] choiceCvt -> choiceCvt", &pchoiceCvt, pchoiceCvt);

    bufOut(fp, fflag);
    sprintf(buffer, "%8x               number\t    [  %d  ]\t/* number of choices */",
	    &pchoiceCvt->number, pchoiceCvt->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   **papChoice -> \"choice string\"",
	    &pchoiceCvt->papChoice, pchoiceCvt->papChoice);
    bufOut(fp, fflag);
    for (i = 0; i < pchoiceCvt->number; i++)
	if (pchoiceCvt->papChoice[i]) {
	    sprintf(buffer, "%8x[%8x]     papChoice[%d]\t\"%s\" ",
		    &pchoiceCvt->papChoice[i], pchoiceCvt->papChoice[i], i,
		    pchoiceCvt->papChoice[i]);
	    bufOut(fp, fflag);
	}
    return;
}				/* end of ChoiceCvt */
static void
CvtTable(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct arrBrkTable *pcvtTable;

    if (!(pcvtTable=pdbBase->pcvtTable)) {
	printf("CvtTable: Error - cvtTable not loaded\n");
	return;
    }
    begNumI = 0;
    endNumI = pcvtTable->number;

    printf("DO YOU WANT TO LIST ALL CVT TABLES ??? (y|n) : ");
    scanf("%s", ibuff);

    if (ibuff[0] != 'y') {
	FOREVER {
	    printf("\nBRKPOINT MENU:\n");
	    for (i = begNumI; i < endNumI; i++) {
		if (pcvtTable->papBrkTable[i]) {
		    sprintf(buffer, "%d\t%s", i, pcvtTable->papBrkTable[i]->name);
		    bufOut(fp, 0);
		}
	    }
	    printf("ENTER SELECTION NUMBER  (N-%d) : ", i - 1);
	    scanf("%d", &inum);
	    if ((pcvtTable->papBrkTable[inum]) && inum < endNumI) {
		begNumI = inum;
		endNumI = begNumI + 1;
		break;
	    } else {
		printf("Not a selection, Please try again\n");
	    }
	}
    } else {
	for (i = 0; i < pcvtTable->number; i++) {
	    if (pcvtTable->papBrkTable[i]) {
		begNumI = i;
		break;
	    }
	}
    }
    sprintf(buffer, "\n\ndbls: listing the  pcvtTable structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x] pcvtTable -> arrBrkTable", &pcvtTable, pcvtTable);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x               number\t    [  %d  ]\t/* number of break tables */",
	    &pcvtTable->number, pcvtTable->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   **papBrkTable -> brkTable",
	    &pcvtTable->papBrkTable, pcvtTable->papBrkTable);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (pcvtTable->papBrkTable[i]) {
	    sprintf(buffer, "%8x[%8x]     papBrkTable[%d] ",
		    &pcvtTable->papBrkTable[i], pcvtTable->papBrkTable[i], i);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t *name\t\t\t\"%s\"",
		    &pcvtTable->papBrkTable[i]->name,
		    pcvtTable->papBrkTable[i]->name,
		    pcvtTable->papBrkTable[i]->name);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t  number    [  %d  ]\t/* number of brkInt in this table */ ",
		    &pcvtTable->papBrkTable[i]->number,
		    pcvtTable->papBrkTable[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t  rawLow\t\t\t%-d",
		    &pcvtTable->papBrkTable[i]->rawLow,
		    pcvtTable->papBrkTable[i]->rawLow);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t  rawHigh\t\t\t%-d",
		    &pcvtTable->papBrkTable[i]->rawHigh,
		    pcvtTable->papBrkTable[i]->rawHigh);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t**papBrkInt",
		    &pcvtTable->papBrkTable[i]->papBrkInt,
		    pcvtTable->papBrkTable[i]->papBrkInt);
	    bufOut(fp, fflag);
	    for (j = 0; j < pcvtTable->papBrkTable[i]->number; j++) {
		if (pcvtTable->papBrkTable[i]->papBrkInt[j]) {
		    sprintf(buffer, "%8x[%8x]\t  papBrkInt[%d] -> brkInt",
			    &pcvtTable->papBrkTable[i]->papBrkInt[j],
			    pcvtTable->papBrkTable[i]->papBrkInt[j], j);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t      raw\t\t\t%-d",
			    &pcvtTable->papBrkTable[i]->papBrkInt[j]->raw,
			    pcvtTable->papBrkTable[i]->papBrkInt[j]->raw);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t      slope\t\t\t%-.5f",
			    &pcvtTable->papBrkTable[i]->papBrkInt[j]->slope,
			    pcvtTable->papBrkTable[i]->papBrkInt[j]->slope);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t      eng\t\t\t%-.5f",
			    &pcvtTable->papBrkTable[i]->papBrkInt[j]->eng,
			    pcvtTable->papBrkTable[i]->papBrkInt[j]->eng);
		    bufOut(fp, fflag);
		}
	    }
	}
    }
}				/* end of CvtTable */
static void
DevSup(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct recType	*precType;
    struct recDevSup	*precDevSup;

    if (!(precDevSup=pdbBase->precDevSup)) {
	printf("DevSup: Error - devSup not loaded\n");
	return;
    }
    if(!(precType=pdbBase->precType)) return;
    getRecTypeSel();
    sprintf(buffer, "\n\ndbls: listing the devSup structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x] precDevSup -> recDevSup ", &precDevSup, precDevSup);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\tnumber\t   [  %d  ]\t/* number of record types */",
	    &precDevSup->number, precDevSup->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   **papDevSup -> precDevSup ",
	    &precDevSup->papDevSup, precDevSup->papDevSup);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (precDevSup->papDevSup[i]) {
	    sprintf(buffer, "%8x[%8x]     papDevSup->[%d]\t\"%s\"",
		    &precDevSup->papDevSup[i],
		    precDevSup->papDevSup[i], i,
		    precType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t   number\t[%d]\t/* number of dset */",
		    &precDevSup->papDevSup[i]->number,
		    precDevSup->papDevSup[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]       **papDsetName",
		    &precDevSup->papDevSup[i]->papDsetName,
		    precDevSup->papDevSup[i]->papDsetName);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]       **papDset",
		    &precDevSup->papDevSup[i]->papDset,
		    precDevSup->papDevSup[i]->papDset);
	    bufOut(fp, fflag);
	    for (j = 0; j < precDevSup->papDevSup[i]->number; j++) {
		if (precDevSup->papDevSup[i]->papDsetName[j]) {
		    sprintf(buffer, "%8x[%8x]\t\tpapDsetName[%d]\t%s",
			    &precDevSup->papDevSup[i]->papDsetName[j],
			    precDevSup->papDevSup[i]->papDsetName[j],
			    j,
			    precDevSup->papDevSup[i]->papDsetName[j]);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x[%8x]\t\tpapDset[%d]",
			    &precDevSup->papDevSup[i]->papDset[j],
			    precDevSup->papDevSup[i]->papDset[j], j);
		    bufOut(fp, fflag);
		}
	    }
	}
    }
}				/* end of DevSup */
static void
DrvSup(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct drvSup  *pdrvSup;

    if (!(pdrvSup=pdbBase->pdrvSup)) {
	printf("DrvSup: Error - drvSup not loaded\n");
    }
    sprintf(buffer, "\n\ndbls: listing the drvSup structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]  pdrvSup -> pdrvSup ", &pdrvSup, pdrvSup);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\t number\t[%d]\t/* number of dset */", &pdrvSup->number, pdrvSup->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]     **papDrvName ->  \t/* pArr of ptr to papDrvName */ ",
	    &pdrvSup->papDrvName, pdrvSup->papDrvName);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]     **papDrvet-> drvet \t/* pArr ptr to drvet */",
	    &pdrvSup->papDrvet, pdrvSup->papDrvet);
    bufOut(fp, fflag);
    for (i = 0; i < pdrvSup->number; i++) {
	if (pdrvSup->papDrvName[i])
	    sprintf(buffer, "%8x[%8x]\t   papDrvName->[%d]\t\"%s\"",
		    &pdrvSup->papDrvName[i],
		    pdrvSup->papDrvName[i],
		    i, pdrvSup->papDrvName[i]);
	bufOut(fp, fflag);
	sprintf(buffer, "%8x[%8x]\t   papDrvet->[%d] /* reserved ptr to drvet */",
		&pdrvSup->papDrvet[i],
		pdrvSup->papDrvet[i], i);
	bufOut(fp, fflag);
    }
}				/* end of DrvSup */
static void
DbRecDes(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    char           *ptemp;
    long            buff[6];
    int             allJ = 0;
    struct recDes  *precDes;
    struct recType	*precType;

    if(!(precType=pdbBase->precType)) return;
    if (!(precDes=pdbBase->precDes)) {
	printf("DbRecDes: Error - precDes not loaded\n");
	return;
    }
    getRecTypeSel();
    begNumJ = 0;
    endNumJ = precDes->papRecTypDes[begNumI]->no_fields;
    if ((endNumI - begNumI) < 2) {
	printf("DO YOU WANT TO LIST ALL FIELDS ??? (y|n) : ");
	scanf("%s", ibuff);
	if (ibuff[0] != 'y')
	    FOREVER {
	    printf("\nFIELDNAME MENU FOR RECTYPE %s:\n", precType->papName[begNumI]);
	    for (j = 0; j < endNumJ; j++) {
		printf("%3d %4.4s",
		       precDes->papRecTypDes[begNumI]->sortFldInd[j],
		       &precDes->papRecTypDes[begNumI]->sortFldName[j]);
		if (!((j + 1) % 8))
		    printf("\n");
	    }
	    printf("\n");
	    printf("ENTER SELECTION NUMBER  (0-%d) : ", j - 1);
	    scanf("%d", &inum);
	    if ((inum >= 0 && inum < j)) {
		begNumJ = inum;
		endNumJ = begNumJ + 1;
		break;
	    } else {
		printf("Not a selection, Please try again\n");
	    }
	    }
    } else
	allJ = 1;

    sprintf(buffer, "\n\ndbls: listing the precDes structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]  precDes-> recDes", &precDes, precDes);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\tnumber [%d] \t/* number of recTypDes */",
	    &precDes->number, precDes->number);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {

	if (precDes->papRecTypDes[i]) {
	    sprintf(buffer, "\n%8x[%8x]    **papRecTypDes-> recTypDes\t\"%s\"",
		    &precDes->papRecTypDes,
		    precDes->papRecTypDes,
		    precType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    rec_size [%d]",
		    &precDes->papRecTypDes[i]->rec_size,
		    precDes->papRecTypDes[i]->rec_size);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    no_fields [%d]",
		    &precDes->papRecTypDes[i]->no_fields,
		    precDes->papRecTypDes[i]->no_fields);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    no_prompt [%d]",
		    &precDes->papRecTypDes[i]->no_prompt,
		    precDes->papRecTypDes[i]->no_prompt);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    no_links [%d]",
		    &precDes->papRecTypDes[i]->no_links,
		    precDes->papRecTypDes[i]->no_links);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    *link_ind",
		    &precDes->papRecTypDes[i]->link_ind,
		    precDes->papRecTypDes[i]->link_ind);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    *sortFldName",
		    &precDes->papRecTypDes[i]->sortFldName,
		    precDes->papRecTypDes[i]->sortFldName);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    *sortFldInd",
		    &precDes->papRecTypDes[i]->sortFldInd,
		    precDes->papRecTypDes[i]->sortFldInd);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t   **papFldDes-> fldDes",
		    &precDes->papRecTypDes[i]->papFldDes,
		    precDes->papRecTypDes[i]->papFldDes);
	    bufOut(fp, fflag);

	    if (allJ) {
		endNumJ = precDes->papRecTypDes[i]->no_fields;
		/* expand *link_ind */
		for (j = 0; j < precDes->papRecTypDes[i]->no_links; j++) {
		    sprintf(buffer,"%8x\tlink_ind[%d] offset=[%d]",
			   &precDes->papRecTypDes[i]->link_ind[j],
			   j,
			   precDes->papRecTypDes[i]->link_ind[j]);
    		    bufOut(fp, fflag);
		}
		/* expand *sortFldName and *sortFldInd */
		ptemp = (char *) precDes->papRecTypDes[i]->sortFldName;
		for (j = 0; j < precDes->papRecTypDes[i]->no_fields; j++) {
		    sprintf(buffer,"[%8x] sortFldName[%2d]=%4.4s [%8x] sortFldInd=%2d",
			   ptemp,
			   j,
			   ptemp,
			   &precDes->papRecTypDes[i]->sortFldInd[j],
			   precDes->papRecTypDes[i]->sortFldInd[j]
			);
    		    bufOut(fp, fflag);
		    ptemp += 4;
		}
	    }
	    for (j = begNumJ; j < endNumJ; j++) {
		sprintf(buffer, "\n");
		bufOut(fp, fflag);
		sprintf(buffer, "%8x[%8x]\t   **papFldDes->fldDes[%d]",
			&precDes->papRecTypDes[i]->papFldDes[j],
			precDes->papRecTypDes[i]->papFldDes[j], j);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tprompt\t\t\"%s\"",
			precDes->papRecTypDes[i]->papFldDes[j]->prompt,
			precDes->papRecTypDes[i]->papFldDes[j]->prompt);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tfldname\t\t\"%4.4s\"",
			precDes->papRecTypDes[i]->papFldDes[j]->fldname,
			precDes->papRecTypDes[i]->papFldDes[j]->fldname);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\toffset [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->offset,
			precDes->papRecTypDes[i]->papFldDes[j]->offset);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tsize [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->size,
			precDes->papRecTypDes[i]->papFldDes[j]->size);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tspecial [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->special,
			precDes->papRecTypDes[i]->papFldDes[j]->special);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tfield_type [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->field_type,
			precDes->papRecTypDes[i]->papFldDes[j]->field_type);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tdbr_field_type [%d]",
		   &precDes->papRecTypDes[i]->papFldDes[j]->dbr_field_type,
		   precDes->papRecTypDes[i]->papFldDes[j]->dbr_field_type);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tprocess_passive [%d]",
		   &precDes->papRecTypDes[i]->papFldDes[j]->process_passive,
		   precDes->papRecTypDes[i]->papFldDes[j]->process_passive);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tchoice_set [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->choice_set,
			precDes->papRecTypDes[i]->papFldDes[j]->choice_set);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tcvt_type [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->cvt_type,
			precDes->papRecTypDes[i]->papFldDes[j]->cvt_type);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tpromptflag [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->promptflag,
			precDes->papRecTypDes[i]->papFldDes[j]->promptflag);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tlowfl [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->lowfl,
			precDes->papRecTypDes[i]->papFldDes[j]->lowfl);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\thighfl [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->highfl,
			precDes->papRecTypDes[i]->papFldDes[j]->highfl);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tinterest [%d]",
			&precDes->papRecTypDes[i]->papFldDes[j]->interest,
			precDes->papRecTypDes[i]->papFldDes[j]->interest);
		bufOut(fp, fflag);
		memcpy((void *) buff,
			(void *) & precDes->papRecTypDes[i]->papFldDes[j]->initial, 8);
		sprintf(buffer, "%8x[%8x][%8x]\tinitial",
			&precDes->papRecTypDes[i]->papFldDes[j]->initial,
			buff[0], buff[1]);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\trange1.fldnum [%d]",
		    &precDes->papRecTypDes[i]->papFldDes[j]->range1.fldnum,
		    precDes->papRecTypDes[i]->papFldDes[j]->range1.fldnum);
		bufOut(fp, fflag);
		memcpy((void *) buff,
			(void *) & precDes->papRecTypDes[i]->papFldDes[j]->range1.value, 8);
		sprintf(buffer, "%8x[%8x][%8x]\trange1.value",
		     &precDes->papRecTypDes[i]->papFldDes[j]->range1.value,
			buff[0], buff[1]);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\trange2.fldnum [%d]",
		    &precDes->papRecTypDes[i]->papFldDes[j]->range2.fldnum,
		    precDes->papRecTypDes[i]->papFldDes[j]->range2.fldnum);
		bufOut(fp, fflag);
		memcpy((void *) buff,
			(void *) & precDes->papRecTypDes[i]->papFldDes[j]->range2.value, 8);
		sprintf(buffer, "%8x[%8x][%8x]\trange2.value",
		     &precDes->papRecTypDes[i]->papFldDes[j]->range2.value,
			buff[0], buff[1]);
		bufOut(fp, fflag);
	    }
	}
    }
}				/* end of DbRecDes */
static void
ChoiceRec(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct choiceRec *pchoiceRec;
    struct recType	*precType;

    if(!(precType=pdbBase->precType)) return;
    if (!(pchoiceRec=pdbBase->pchoiceRec)) {
	printf("ChoiceRec: Error - choiceRec not loaded\n");
	return;
    }
    endNumI = precType->number;

    printf("DO YOU WANT TO LIST ALL RECORD TYPES ??? (y|n) : ");
    scanf("%s", ibuff);

    if (ibuff[0] != 'y') {
	FOREVER {
	    printf("\nRECTYPE MENU:\n");
	    for (i = 0; i < precType->number; i++) {
		if (precType->papName[i]
			&& pchoiceRec->papArrChoiceSet[i]) {
		    printf("%d\t%s\n", i, precType->papName[i]);
		}
	    }
	    printf("ENTER SELECTION NUMBER  (X-%d) : ", i - 1);
	    scanf("%d", &inum);
	    if ((pchoiceRec->papArrChoiceSet[inum])
		    && (inum >= 0) && (inum < i)) {
		begNumI = inum;
		endNumI = begNumI + 1;
		break;
	    } else {
		printf("Not a selection, Please try again\n");
	    }
	}
    } else {
	for (i = 0; i < precType->number; i++) {
	    /* start at first record definition */
	    if (pchoiceRec->papArrChoiceSet[i]) {
		begNumI = i;
		break;
	    }
	}
    }
    sprintf(buffer, "\n\ndbls: listing the  choiceRec structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   pchoiceRec -> pchoiceRec", &pchoiceRec, pchoiceRec);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\tnumber\t\t[%d]", &pchoiceRec->number, pchoiceRec->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]\t**papArrChoiceSet -> arrChoiceSet",
	    &pchoiceRec->papArrChoiceSet,
	    pchoiceRec->papArrChoiceSet);
    bufOut(fp, fflag);

    for (i = begNumI; i < endNumI; i++) {
	if (pchoiceRec->papArrChoiceSet[i]) {
	    sprintf(buffer, "%8x[%8x]\tpapArrChoiceSet[%d] -> arrChoiceSet\t\tRECTYPE \"%s\"",
		    &pchoiceRec->papArrChoiceSet[i],
		    pchoiceRec->papArrChoiceSet[i], i,
		    precType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    number\t\t[%d]\t/* number of choice sets */",
		    &pchoiceRec->papArrChoiceSet[i]->number,
		    pchoiceRec->papArrChoiceSet[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    **papChoiceSet",
		    &pchoiceRec->papArrChoiceSet[i]->papChoiceSet,
		    pchoiceRec->papArrChoiceSet[i]->papChoiceSet);
	    bufOut(fp, fflag);
	    for (j = 0; j < pchoiceRec->papArrChoiceSet[i]->number; j++) {
		if (pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]) {
		    sprintf(buffer, "%8x[%8x]\t    papChoiceSet[%d] -> choiceSet \t\tINDEX[%d]",

			    &pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j],
		      pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j], j, j);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t\tnumber\t\t[%d]\t/* number of choices */ ",
		    &pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->number,
		    pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->number);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x[%8x]\t\t**papChoice -> \"string\"",
			    &pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice,
			    pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice);
		    bufOut(fp, fflag);
		    for (k = 0; k < pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->number; k++) {
			if (pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k]) {
			    sprintf(buffer, "%8x[%8x]\t\tpapChoice[%d]\t\"%s\"",
				    &pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k],
				    pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k], k,
				    pchoiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k]);
			    bufOut(fp, fflag);
			}
		    }

		}
	    }

	}
    }
}				/* end of ChoiceRec */
static void
DbErrDes(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    if (!dbErrDes) {
	printf("DbErrDes Error - dbErrDes not loaded\n");
	return;
    }
    sprintf(buffer, "\n\ndbls: listing the dbErrDes structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x] dbErrDes -> errDes ", &dbErrDes, dbErrDes);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\tnumber\t   [  %d  ]\t /* dim of err modules */",
	    &dbErrDes->number, dbErrDes->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   **papErrSet -> errSet ",
	    &dbErrDes->papErrSet, dbErrDes->papErrSet);
    bufOut(fp, fflag);
    for (i = 0; i < dbErrDes->number; i++) {
	if (dbErrDes->papErrSet[i]) {
	    sprintf(buffer, "%8x[%8x]     papErrSet->[%d]\tmodule[%d]",
		    &dbErrDes->papErrSet[i],
		    dbErrDes->papErrSet[i], i,
		    i + 501);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t   number\t[%d]\t/* dim of errSet */",
		    &dbErrDes->papErrSet[i]->number,
		    dbErrDes->papErrSet[i]->number);
	    bufOut(fp, fflag);
	    for (j = 0; j < dbErrDes->papErrSet[i]->number; j++) {
		if (dbErrDes->papErrSet[i]->papName[j]) {
		    sprintf(buffer, "%8x[%8x]\t\tpapName[%d]\t\"%s\"",
			    &dbErrDes->papErrSet[i]->papName[j],
			    dbErrDes->papErrSet[i]->papName[j],
			    j,
			    dbErrDes->papErrSet[i]->papName[j]);
		    bufOut(fp, fflag);
		}
	    }
	}
    }
}				/* end of DbErrDes */
static void
DbRecords(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct recType	*precType;
    struct recHeader	*precHeader;

    if(!(precType=pdbBase->precType)) return;
    if (!(precHeader=pdbBase->precHeader)) {
	printf("DbRecords: Error - precHeader not loaded\n");
	return;
    }
    getRecTypeSel();
    sprintf(buffer, "\n\ndbls: listing the precHeader structure\n");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]    precHeader -> recHeader", &precHeader, precHeader);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\t  number\t\t[%d]\t/* number of record types */",
	    &precHeader->number,
	    precHeader->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]\t**papRecLoc -> recLoc",
	    &precHeader->papRecLoc,
	    precHeader->papRecLoc);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (precHeader->papRecLoc[i]) {
	    sprintf(buffer, "%8x[%8x]\t  papRecLoc->[%d]\tRECTYPE[\"%s\"]",
		    &precHeader->papRecLoc[i],
		    precHeader->papRecLoc[i],
			 i,
		    precType->papName[i]);
	    bufOut(fp, fflag);

	    sprintf(buffer, "%8x\t\t      rec_size [%d]\t\t/* rec_size in bytes */",
		    &precHeader->papRecLoc[i]->rec_size,
		    precHeader->papRecLoc[i]->rec_size);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t      record_type [%d]\t\t/* record_type in bytes */",
		    &precHeader->papRecLoc[i]->record_type,
		    precHeader->papRecLoc[i]->record_type);
	    bufOut(fp, fflag);
	}
    }
}				/* end of DbRecords */

static void
RecSup(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    struct recType	*precType;
    struct recSup	*precSup;

    if(!(precType=pdbBase->precType)) return;
    if (!(precSup=pdbBase->precSup)) {
	printf("RecSup: Error - recSup not loaded\n");
	return;
    }
    getRecTypeSel();
    sprintf(buffer, "\n\ndbls: listing the recSup structure\n");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   precSup -> precSup", &precSup, precSup);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\t number\t\t[%d]\t/* number of record types */",
	    &precSup->number,
	    precSup->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]     **papRset -> rset",
	    &precSup->papRset,
	    precSup->papRset);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (precSup->papRset[i]) {
	    sprintf(buffer, "%8x[%8x]\t papRset->[%d]\t\tRECTYPE[\"%s\"]",
		    &precSup->papRset[i],
		    precSup->papRset[i], i,
		    precType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     number\t\t/*number of support routines */",
		    &precSup->papRset[i]->number,
		    precSup->papRset[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     report\t\t/*print report */",
		    &precSup->papRset[i]->report,
		    precSup->papRset[i]->report);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     init\t\t/*init support */",
		    &precSup->papRset[i]->init,
		    precSup->papRset[i]->init);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     init_record\t/*init record */",
		    &precSup->papRset[i]->init_record,
		    precSup->papRset[i]->init_record);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     process\t\t/*process record */",
		    &precSup->papRset[i]->process,
		    precSup->papRset[i]->process);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     special\t\t/*special processing */",
		    &precSup->papRset[i]->special,
		    precSup->papRset[i]->special);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_precision\t/* get_precision of this type */",
		    &precSup->papRset[i]->get_precision,
		    precSup->papRset[i]->get_precision);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_value\t\t/*get value field */",
		    &precSup->papRset[i]->get_value,
		    precSup->papRset[i]->get_value);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     cvt_dbaddr\t\t/*cvt dbAddr */",
		    &precSup->papRset[i]->cvt_dbaddr,
		    precSup->papRset[i]->cvt_dbaddr);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_array_info\t/* get_array_info of this type */",
		    &precSup->papRset[i]->get_array_info,
		    precSup->papRset[i]->get_array_info);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     put_array_info\t/* put_array_info of this type */",
		    &precSup->papRset[i]->put_array_info,
		    precSup->papRset[i]->put_array_info);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_enum_str\t/*get string from enum item*/",
		    &precSup->papRset[i]->get_enum_str,
		    precSup->papRset[i]->get_enum_str);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_units \t\t/* get_units of this type */",
		    &precSup->papRset[i]->get_units,
		    precSup->papRset[i]->get_units);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_graphic_double /* get_graphic_double of this type */",
		    &precSup->papRset[i]->get_graphic_double,
		    precSup->papRset[i]->get_graphic_double);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_control_double /* get_control_double of this type */",
		    &precSup->papRset[i]->get_control_double,
		    precSup->papRset[i]->get_control_double);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_enum_strs\t/*get all enum strings */",
		    &precSup->papRset[i]->get_enum_strs,
		    precSup->papRset[i]->get_enum_strs);
	    bufOut(fp, fflag);
	}
    }
}				/* end of recSup */
