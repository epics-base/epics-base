/* dbls.c - structure listing program */
/* share/src/db $Id$ */

/*
 *      Author:          Bob Zeiman
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
 * .01  02-28-92        jba     ANSI C changes
 */


#define SAME 0

#include <vxWorks.h>
#include <stdioLib.h>
#include <types.h>
#include <dbRecType.h>
#include <choice.h>
#include <cvtTable.h>
#include <devSup.h>
#include <drvSup.h>
#include <recSup.h>
#include <dbRecDes.h>
#include <dbRecords.h>
#include <errMdef.h>
#include <error.h>
#include <sdrHeader.h>

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


    if (!dbRecType) {
	status = S_sdr_notLoaded;
	errMessage(status, "dbls: Error - dbRecType not loaded");
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
    endNumI = dbRecType->number;

    printf("DO YOU WANT TO LIST ALL RECORD TYPES ??? (y|n) : ");
    scanf("%s", ibuff);

    if (ibuff[0] != 'y') {
	FOREVER {
	    printf("\nRECTYPE MENU:\n");
	    for (i = 0; i < dbRecType->number; i++) {
		if (dbRecType->papName[i]) {
		    printf("%d\t%s\n", i, dbRecType->papName[i]);
		}
	    }
	    printf("ENTER SELECTION NUMBER  (X-%d) : ", i - 1);
	    scanf("%d", &inum);
	    if ((dbRecType->papName[inum])
		    && (inum >= 0) && (inum < i)) {
		begNumI = inum;
		endNumI = begNumI + 1;
		break;
	    } else {
		printf("Not a selection, Please try again\n");
	    }
	}
    } else {
	for (i = 0; i < dbRecType->number; i++) {
	    /* start at first record definition */
	    if (dbRecType->papName[i]) {
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

    sprintf(buffer, "\n\ndbls: listing the dbRecType structure\n");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]    dbRecType-> recType", &dbRecType, dbRecType);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t      number\t    [  %d  ]\t/* number of types */",
	    &dbRecType->number, dbRecType->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]  **papName\t\t /* pap to record type */",
	    &dbRecType->papName, dbRecType->papName);
    bufOut(fp, fflag);
    for (i = 0; i < dbRecType->number; i++) {
	if (dbRecType->papName[i]) {
	    sprintf(buffer, "%8x[%8x]    papName->[%d]->\t\"%s\"",
		    &dbRecType->papName[i],
		    dbRecType->papName[i],
		    i, dbRecType->papName[i]);
	    bufOut(fp, fflag);
	}
    }
}				/* end of DbRecType */
static void
ChoiceGbl(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    if (!choiceGbl) {
	printf("ChoiceGbl: Error - choiceGbl not loaded\n");
	return;
    }
    sprintf(buffer, "\n\ndbls: listing the choiceGbl structure\n");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]    choiceGbl -> arrChoiceSet", &choiceGbl, choiceGbl);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\t  number\t\t[%d]\t/* number of choice sets */",
	    &choiceGbl->number,
	    choiceGbl->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]\t**papChoiceSet -> choiceSet",
	    &choiceGbl->papChoiceSet,
	    choiceGbl->papChoiceSet);
    bufOut(fp, fflag);
    for (i = 0; i < choiceGbl->number; i++) {
	if (choiceGbl->papChoiceSet[i]) {
	    sprintf(buffer, "%8x[%8x]\t  papChoiceSet->[%d]\tINDEX[%d]",
		    &choiceGbl->papChoiceSet[i],
		    choiceGbl->papChoiceSet[i], i, i);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t      number [%d]\t\t/* number of choices */",
		    &choiceGbl->papChoiceSet[i]->number,
		    choiceGbl->papChoiceSet[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    **papChoice -> \t\"choice string\"",
		    &choiceGbl->papChoiceSet[i]->papChoice,
		    choiceGbl->papChoiceSet[i]->papChoice);
	    bufOut(fp, fflag);
	    for (j = 0; j < choiceGbl->papChoiceSet[i]->number; j++) {
		if (choiceGbl->papChoiceSet[i]->papChoice[j]) {
		    sprintf(buffer, "%8x[%8x]\t\t  papChoice->[%d]\t\"%s\"",
			    &choiceGbl->papChoiceSet[i]->papChoice[j],
			    choiceGbl->papChoiceSet[i]->papChoice[j],
			    j,
			    choiceGbl->papChoiceSet[i]->papChoice[j]);
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
    if (!choiceDev) {
	printf("ChoiceDev: Error - choiceDev not loaded\n");
	return;
    }
    getRecTypeSel();
    sprintf(buffer, "\n\ndbls: listing the  choiceDev structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]choiceDev-> devChoiceRec ", &choiceDev, choiceDev);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t      number\t[%d]\t\t\t/* number of devChoiceSet */",
	    &choiceDev->number, choiceDev->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]    **papDevChoiceSet -> devChoiceSet ",
	    &choiceDev->papDevChoiceSet,
	    choiceDev->papDevChoiceSet);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (choiceDev->papDevChoiceSet[i]) {
	    sprintf(buffer, "\n%8x[%8x]\tpapDevChoiceSet->[%d]\t\tRECTYPE \"%s\"",
		    &choiceDev->papDevChoiceSet[i],
		    choiceDev->papDevChoiceSet[i], i,
		    dbRecType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t   number\t[%d]\t\t/*number of choices */ ",
		    &choiceDev->papDevChoiceSet[i]->number,
		    choiceDev->papDevChoiceSet[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t   **papDevChoice -> devChoice",
		    &choiceDev->papDevChoiceSet[i]->papDevChoice,
		    choiceDev->papDevChoiceSet[i]->papDevChoice);
	    bufOut(fp, fflag);
	    for (j = 0; j < choiceDev->papDevChoiceSet[i]->number; j++) {
		if (choiceDev->papDevChoiceSet[i]->papDevChoice[j]) {
		    sprintf(buffer, "%8x[%8x]\t   papDevChoice[%d]",
			    &choiceDev->papDevChoiceSet[i]->papDevChoice[j],
			 choiceDev->papDevChoiceSet[i]->papDevChoice[j], j);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t       link_type [%d]",
			    &choiceDev->papDevChoiceSet[i]->papDevChoice[j]->link_type,
			    choiceDev->papDevChoiceSet[i]->papDevChoice[j]->link_type);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x[%8x]\t\t*pchoice\t\t\"%s\"",
		    &choiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice,
		    choiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice,
		    choiceDev->papDevChoiceSet[i]->papDevChoice[j]->pchoice);
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

    if (!choiceCvt) {
	printf("ChoiceCvt: Error - choiceCvt not loaded\n");
	return;
    }
    sprintf(buffer, "\n\ndbls: listing the  choiceCvt structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x] choiceCvt -> choiceCvt", &choiceCvt, choiceCvt);

    bufOut(fp, fflag);
    sprintf(buffer, "%8x               number\t    [  %d  ]\t/* number of choices */",
	    &choiceCvt->number, choiceCvt->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   **papChoice -> \"choice string\"",
	    &choiceCvt->papChoice, choiceCvt->papChoice);
    bufOut(fp, fflag);
    for (i = 0; i < choiceCvt->number; i++)
	if (choiceCvt->papChoice[i]) {
	    sprintf(buffer, "%8x[%8x]     papChoice[%d]\t\"%s\" ",
		    &choiceCvt->papChoice[i], choiceCvt->papChoice[i], i,
		    choiceCvt->papChoice[i]);
	    bufOut(fp, fflag);
	}
    return;
}				/* end of ChoiceCvt */
static void
CvtTable(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    if (!cvtTable) {
	printf("CvtTable: Error - cvtTable not loaded\n");
	return;
    }
    begNumI = 0;
    endNumI = cvtTable->number;

    printf("DO YOU WANT TO LIST ALL CVT TABLES ??? (y|n) : ");
    scanf("%s", ibuff);

    if (ibuff[0] != 'y') {
	FOREVER {
	    printf("\nBRKPOINT MENU:\n");
	    for (i = begNumI; i < endNumI; i++) {
		if (cvtTable->papBrkTable[i]) {
		    sprintf(buffer, "%d\t%s", i, cvtTable->papBrkTable[i]->name);
		    bufOut(fp, 0);
		}
	    }
	    printf("ENTER SELECTION NUMBER  (N-%d) : ", i - 1);
	    scanf("%d", &inum);
	    if ((cvtTable->papBrkTable[inum]) && inum < endNumI) {
		begNumI = inum;
		endNumI = begNumI + 1;
		break;
	    } else {
		printf("Not a selection, Please try again\n");
	    }
	}
    } else {
	for (i = 0; i < cvtTable->number; i++) {
	    if (cvtTable->papBrkTable[i]) {
		begNumI = i;
		break;
	    }
	}
    }
    sprintf(buffer, "\n\ndbls: listing the  cvtTable structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x] cvtTable -> arrBrkTable", &cvtTable, cvtTable);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x               number\t    [  %d  ]\t/* number of break tables */",
	    &cvtTable->number, cvtTable->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   **papBrkTable -> brkTable",
	    &cvtTable->papBrkTable, cvtTable->papBrkTable);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (cvtTable->papBrkTable[i]) {
	    sprintf(buffer, "%8x[%8x]     papBrkTable[%d] ",
		    &cvtTable->papBrkTable[i], cvtTable->papBrkTable[i], i);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t *name\t\t\t\"%s\"",
		    &cvtTable->papBrkTable[i]->name,
		    cvtTable->papBrkTable[i]->name,
		    cvtTable->papBrkTable[i]->name);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t  number    [  %d  ]\t/* number of brkInt in this table */ ",
		    &cvtTable->papBrkTable[i]->number,
		    cvtTable->papBrkTable[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t  rawLow\t\t\t%-d",
		    &cvtTable->papBrkTable[i]->rawLow,
		    cvtTable->papBrkTable[i]->rawLow);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t  rawHigh\t\t\t%-d",
		    &cvtTable->papBrkTable[i]->rawHigh,
		    cvtTable->papBrkTable[i]->rawHigh);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t**papBrkInt",
		    &cvtTable->papBrkTable[i]->papBrkInt,
		    cvtTable->papBrkTable[i]->papBrkInt);
	    bufOut(fp, fflag);
	    for (j = 0; j < cvtTable->papBrkTable[i]->number; j++) {
		if (cvtTable->papBrkTable[i]->papBrkInt[j]) {
		    sprintf(buffer, "%8x[%8x]\t  papBrkInt[%d] -> brkInt",
			    &cvtTable->papBrkTable[i]->papBrkInt[j],
			    cvtTable->papBrkTable[i]->papBrkInt[j], j);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t      raw\t\t\t%-d",
			    &cvtTable->papBrkTable[i]->papBrkInt[j]->raw,
			    cvtTable->papBrkTable[i]->papBrkInt[j]->raw);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t      slope\t\t\t%-.5f",
			    &cvtTable->papBrkTable[i]->papBrkInt[j]->slope,
			    cvtTable->papBrkTable[i]->papBrkInt[j]->slope);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t      eng\t\t\t%-.5f",
			    &cvtTable->papBrkTable[i]->papBrkInt[j]->eng,
			    cvtTable->papBrkTable[i]->papBrkInt[j]->eng);
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

    if (!devSup) {
	printf("DevSup: Error - devSup not loaded\n");
	return;
    }
    getRecTypeSel();
    sprintf(buffer, "\n\ndbls: listing the devSup structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x] devSup -> recDevSup ", &devSup, devSup);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\tnumber\t   [  %d  ]\t/* number of record types */",
	    &devSup->number, devSup->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   **papDevSup -> devSup ",
	    &devSup->papDevSup, devSup->papDevSup);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (devSup->papDevSup[i]) {
	    sprintf(buffer, "%8x[%8x]     papDevSup->[%d]\t\"%s\"",
		    &devSup->papDevSup[i],
		    devSup->papDevSup[i], i,
		    dbRecType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t   number\t[%d]\t/* number of dset */",
		    &devSup->papDevSup[i]->number,
		    devSup->papDevSup[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]       **dsetName",
		    &devSup->papDevSup[i]->dsetName,
		    devSup->papDevSup[i]->dsetName);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]       **papDset",
		    &devSup->papDevSup[i]->papDset,
		    devSup->papDevSup[i]->papDset);
	    bufOut(fp, fflag);
	    for (j = 0; j < devSup->papDevSup[i]->number; j++) {
		if (devSup->papDevSup[i]->dsetName[j]) {
		    sprintf(buffer, "%8x[%8x]\t\tdsetName[%d]\t%s",
			    &devSup->papDevSup[i]->dsetName[j],
			    devSup->papDevSup[i]->dsetName[j],
			    j,
			    devSup->papDevSup[i]->dsetName[j]);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x[%8x]\t\tpapDset[%d]",
			    &devSup->papDevSup[i]->papDset[j],
			    devSup->papDevSup[i]->papDset[j], j);
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
    if (!drvSup) {
	printf("DrvSup: Error - drvSup not loaded\n");
    }
    sprintf(buffer, "\n\ndbls: listing the drvSup structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]  drvSup -> drvSup ", &drvSup, drvSup);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\t number\t[%d]\t/* number of dset */", &drvSup->number, drvSup->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]     **drvetName ->  \t/* pArr of ptr to drvetName */ ",
	    &drvSup->drvetName, drvSup->drvetName);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]     **papDrvet-> drvet \t/* pArr ptr to drvet */",
	    &drvSup->papDrvet, drvSup->papDrvet);
    bufOut(fp, fflag);
    for (i = 0; i < drvSup->number; i++) {
	if (drvSup->drvetName[i])
	    sprintf(buffer, "%8x[%8x]\t   drvetName->[%d]\t\"%s\"",
		    &drvSup->drvetName[i],
		    drvSup->drvetName[i],
		    i, drvSup->drvetName[i]);
	bufOut(fp, fflag);
	sprintf(buffer, "%8x[%8x]\t   papDrvet->[%d] /* reserved ptr to drvet */",
		&drvSup->papDrvet[i],
		drvSup->papDrvet[i], i);
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
    if (!dbRecDes) {
	printf("DbRecDes: Error - dbRecDes not loaded\n");
	return;
    }
    getRecTypeSel();
    begNumJ = 0;
    endNumJ = dbRecDes->papRecTypDes[begNumI]->no_fields;
    if ((endNumI - begNumI) < 2) {
	printf("DO YOU WANT TO LIST ALL FIELDS ??? (y|n) : ");
	scanf("%s", ibuff);
	if (ibuff[0] != 'y')
	    FOREVER {
	    printf("\nFIELDNAME MENU FOR RECTYPE %s:\n", dbRecType->papName[begNumI]);
	    for (j = 0; j < endNumJ; j++) {
		printf("%3d %4.4s",
		       dbRecDes->papRecTypDes[begNumI]->sortFldInd[j],
		       &dbRecDes->papRecTypDes[begNumI]->sortFldName[j]);
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

    sprintf(buffer, "\n\ndbls: listing the dbRecDes structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]  dbRecDes-> recDes", &dbRecDes, dbRecDes);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\tnumber [%d] \t/* number of recTypDes */",
	    &dbRecDes->number, dbRecDes->number);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {

	if (dbRecDes->papRecTypDes[i]) {
	    sprintf(buffer, "\n%8x[%8x]    **papRecTypDes-> recTypDes\t\"%s\"",
		    &dbRecDes->papRecTypDes,
		    dbRecDes->papRecTypDes,
		    dbRecType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    rec_size [%d]",
		    &dbRecDes->papRecTypDes[i]->rec_size,
		    dbRecDes->papRecTypDes[i]->rec_size);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    no_fields [%d]",
		    &dbRecDes->papRecTypDes[i]->no_fields,
		    dbRecDes->papRecTypDes[i]->no_fields);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    no_prompt [%d]",
		    &dbRecDes->papRecTypDes[i]->no_prompt,
		    dbRecDes->papRecTypDes[i]->no_prompt);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    no_links [%d]",
		    &dbRecDes->papRecTypDes[i]->no_links,
		    dbRecDes->papRecTypDes[i]->no_links);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    *link_ind",
		    &dbRecDes->papRecTypDes[i]->link_ind,
		    dbRecDes->papRecTypDes[i]->link_ind);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    *sortFldName",
		    &dbRecDes->papRecTypDes[i]->sortFldName,
		    dbRecDes->papRecTypDes[i]->sortFldName);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    *sortFldInd",
		    &dbRecDes->papRecTypDes[i]->sortFldInd,
		    dbRecDes->papRecTypDes[i]->sortFldInd);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t   **papFldDes-> fldDes",
		    &dbRecDes->papRecTypDes[i]->papFldDes,
		    dbRecDes->papRecTypDes[i]->papFldDes);
	    bufOut(fp, fflag);

	    if (allJ) {
		endNumJ = dbRecDes->papRecTypDes[i]->no_fields;
		/* expand *link_ind */
		for (j = 0; j < dbRecDes->papRecTypDes[i]->no_links; j++) {
		    sprintf(buffer,"%8x\tlink_ind[%d] offset=[%d]",
			   &dbRecDes->papRecTypDes[i]->link_ind[j],
			   j,
			   dbRecDes->papRecTypDes[i]->link_ind[j]);
    		    bufOut(fp, fflag);
		}
		/* expand *sortFldName and *sortFldInd */
		ptemp = (char *) dbRecDes->papRecTypDes[i]->sortFldName;
		for (j = 0; j < dbRecDes->papRecTypDes[i]->no_fields; j++) {
		    sprintf(buffer,"[%8x] sortFldName[%2d]=%4.4s [%8x] sortFldInd=%2d",
			   ptemp,
			   j,
			   ptemp,
			   &dbRecDes->papRecTypDes[i]->sortFldInd[j],
			   dbRecDes->papRecTypDes[i]->sortFldInd[j]
			);
    		    bufOut(fp, fflag);
		    ptemp += 4;
		}
	    }
	    for (j = begNumJ; j < endNumJ; j++) {
		sprintf(buffer, "\n");
		bufOut(fp, fflag);
		sprintf(buffer, "%8x[%8x]\t   **papFldDes->fldDes[%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j],
			dbRecDes->papRecTypDes[i]->papFldDes[j], j);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tprompt\t\t\"%s\"",
			dbRecDes->papRecTypDes[i]->papFldDes[j]->prompt,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->prompt);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tfldname\t\t\"%4.4s\"",
			dbRecDes->papRecTypDes[i]->papFldDes[j]->fldname,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->fldname);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\toffset [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->offset,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->offset);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tsize [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->size,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->size);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tspecial [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->special,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->special);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tfield_type [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->field_type,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->field_type);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tdbr_field_type [%d]",
		   &dbRecDes->papRecTypDes[i]->papFldDes[j]->dbr_field_type,
		   dbRecDes->papRecTypDes[i]->papFldDes[j]->dbr_field_type);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tprocess_passive [%d]",
		   &dbRecDes->papRecTypDes[i]->papFldDes[j]->process_passive,
		   dbRecDes->papRecTypDes[i]->papFldDes[j]->process_passive);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tchoice_set [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->choice_set,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->choice_set);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tcvt_type [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->cvt_type,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->cvt_type);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tpromptflag [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->promptflag,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->promptflag);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tlowfl [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->lowfl,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->lowfl);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\thighfl [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->highfl,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->highfl);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\tinterest [%d]",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->interest,
			dbRecDes->papRecTypDes[i]->papFldDes[j]->interest);
		bufOut(fp, fflag);
		memcpy((void *) & dbRecDes->papRecTypDes[i]->papFldDes[j]->initial,
		      (void *) buff, 8);
		sprintf(buffer, "%8x[%8x][%8x]\tinitial",
			&dbRecDes->papRecTypDes[i]->papFldDes[j]->initial,
			buff[0], buff[1]);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\trange1.fldnum [%d]",
		    &dbRecDes->papRecTypDes[i]->papFldDes[j]->range1.fldnum,
		    dbRecDes->papRecTypDes[i]->papFldDes[j]->range1.fldnum);
		bufOut(fp, fflag);
		memcpy((void *) & dbRecDes->papRecTypDes[i]->papFldDes[j]->range1.value,
		      (void *) buff, 8);
		sprintf(buffer, "%8x[%8x][%8x]\trange1.value",
		     &dbRecDes->papRecTypDes[i]->papFldDes[j]->range1.value,
			buff[0], buff[1]);
		bufOut(fp, fflag);
		sprintf(buffer, "%8x\t\t\trange2.fldnum [%d]",
		    &dbRecDes->papRecTypDes[i]->papFldDes[j]->range2.fldnum,
		    dbRecDes->papRecTypDes[i]->papFldDes[j]->range2.fldnum);
		bufOut(fp, fflag);
		memcpy((void *) & dbRecDes->papRecTypDes[i]->papFldDes[j]->range2.value,
		      (void *) buff, 8);
		sprintf(buffer, "%8x[%8x][%8x]\trange2.value",
		     &dbRecDes->papRecTypDes[i]->papFldDes[j]->range2.value,
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
    if (!choiceRec) {
	printf("ChoiceRec: Error - choiceRec not loaded\n");
	return;
    }
    endNumI = dbRecType->number;

    printf("DO YOU WANT TO LIST ALL RECORD TYPES ??? (y|n) : ");
    scanf("%s", ibuff);

    if (ibuff[0] != 'y') {
	FOREVER {
	    printf("\nRECTYPE MENU:\n");
	    for (i = 0; i < dbRecType->number; i++) {
		if (dbRecType->papName[i]
			&& choiceRec->papArrChoiceSet[i]) {
		    printf("%d\t%s\n", i, dbRecType->papName[i]);
		}
	    }
	    printf("ENTER SELECTION NUMBER  (X-%d) : ", i - 1);
	    scanf("%d", &inum);
	    if ((choiceRec->papArrChoiceSet[inum])
		    && (inum >= 0) && (inum < i)) {
		begNumI = inum;
		endNumI = begNumI + 1;
		break;
	    } else {
		printf("Not a selection, Please try again\n");
	    }
	}
    } else {
	for (i = 0; i < dbRecType->number; i++) {
	    /* start at first record definition */
	    if (choiceRec->papArrChoiceSet[i]) {
		begNumI = i;
		break;
	    }
	}
    }
    sprintf(buffer, "\n\ndbls: listing the  choiceRec structure");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   choiceRec -> choiceRec", &choiceRec, choiceRec);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\tnumber\t\t[%d]", &choiceRec->number, choiceRec->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]\t**papArrChoiceSet -> arrChoiceSet",
	    &choiceRec->papArrChoiceSet,
	    choiceRec->papArrChoiceSet);
    bufOut(fp, fflag);

    for (i = begNumI; i < endNumI; i++) {
	if (choiceRec->papArrChoiceSet[i]) {
	    sprintf(buffer, "%8x[%8x]\tpapArrChoiceSet[%d] -> arrChoiceSet\t\tRECTYPE \"%s\"",
		    &choiceRec->papArrChoiceSet[i],
		    choiceRec->papArrChoiceSet[i], i,
		    dbRecType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t    number\t\t[%d]\t/* number of choice sets */",
		    &choiceRec->papArrChoiceSet[i]->number,
		    choiceRec->papArrChoiceSet[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t    **papChoiceSet",
		    &choiceRec->papArrChoiceSet[i]->papChoiceSet,
		    choiceRec->papArrChoiceSet[i]->papChoiceSet);
	    bufOut(fp, fflag);
	    for (j = 0; j < choiceRec->papArrChoiceSet[i]->number; j++) {
		if (choiceRec->papArrChoiceSet[i]->papChoiceSet[j]) {
		    sprintf(buffer, "%8x[%8x]\t    papChoiceSet[%d] -> choiceSet \t\tINDEX[%d]",

			    &choiceRec->papArrChoiceSet[i]->papChoiceSet[j],
		      choiceRec->papArrChoiceSet[i]->papChoiceSet[j], j, j);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x\t\t\tnumber\t\t[%d]\t/* number of choices */ ",
		    &choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->number,
		    choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->number);
		    bufOut(fp, fflag);
		    sprintf(buffer, "%8x[%8x]\t\t**papChoice -> \"string\"",
			    &choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice,
			    choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice);
		    bufOut(fp, fflag);
		    for (k = 0; k < choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->number; k++) {
			if (choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k]) {
			    sprintf(buffer, "%8x[%8x]\t\tpapChoice[%d]\t\"%s\"",
				    &choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k],
				    choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k], k,
				    choiceRec->papArrChoiceSet[i]->papChoiceSet[j]->papChoice[k]);
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
    if (!dbRecords) {
	printf("DbRecords: Error - dbRecords not loaded\n");
	return;
    }
    getRecTypeSel();
    sprintf(buffer, "\n\ndbls: listing the dbRecords structure\n");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]    dbRecords -> recHeader", &dbRecords, dbRecords);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\t  number\t\t[%d]\t/* number of record types */",
	    &dbRecords->number,
	    dbRecords->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]\t**papRecLoc -> recLoc",
	    &dbRecords->papRecLoc,
	    dbRecords->papRecLoc);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (dbRecords->papRecLoc[i]) {
	    sprintf(buffer, "%8x[%8x]\t  papRecLoc->[%d]\tRECTYPE[\"%s\"]",
		    &dbRecords->papRecLoc[i],
		    dbRecords->papRecLoc[i],
			 i,
		    dbRecType->papName[i]);
	    bufOut(fp, fflag);

	    sprintf(buffer, "%8x\t\t      no_records [%d]\t\t/* no_records of this type */",
		    &dbRecords->papRecLoc[i]->no_records,
		    dbRecords->papRecLoc[i]->no_records);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t      no_used [%d]\t\t/* no_used of this type */",
		    &dbRecords->papRecLoc[i]->no_used,
		    dbRecords->papRecLoc[i]->no_used);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x\t\t      rec_size [%d]\t\t/* rec_size in bytes */",
		    &dbRecords->papRecLoc[i]->rec_size,
		    dbRecords->papRecLoc[i]->rec_size);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     *pFirst\t\t\t/* ptr to first record */",
		    &dbRecords->papRecLoc[i]->pFirst,
		    dbRecords->papRecLoc[i]->pFirst);
	    bufOut(fp, fflag);
	}
    }
}				/* end of DbRecords */

static void
RecSup(fp, fflag)
    FILE           *fp;
    int             fflag;
{
    if (!recSup) {
	printf("RecSup: Error - recSup not loaded\n");
	return;
    }
    getRecTypeSel();
    sprintf(buffer, "\n\ndbls: listing the recSup structure\n");
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]   recSup -> recSup", &recSup, recSup);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x\t\t number\t\t[%d]\t/* number of record types */",
	    &recSup->number,
	    recSup->number);
    bufOut(fp, fflag);
    sprintf(buffer, "%8x[%8x]     **papRset -> rset",
	    &recSup->papRset,
	    recSup->papRset);
    bufOut(fp, fflag);
    for (i = begNumI; i < endNumI; i++) {
	if (recSup->papRset[i]) {
	    sprintf(buffer, "%8x[%8x]\t papRset->[%d]\t\tRECTYPE[\"%s\"]",
		    &recSup->papRset[i],
		    recSup->papRset[i], i,
		    dbRecType->papName[i]);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     number\t\t/*number of support routines */",
		    &recSup->papRset[i]->number,
		    recSup->papRset[i]->number);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     report\t\t/*print report */",
		    &recSup->papRset[i]->report,
		    recSup->papRset[i]->report);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     init\t\t/*init support */",
		    &recSup->papRset[i]->init,
		    recSup->papRset[i]->init);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     init_record\t/*init record */",
		    &recSup->papRset[i]->init_record,
		    recSup->papRset[i]->init_record);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     process\t\t/*process record */",
		    &recSup->papRset[i]->process,
		    recSup->papRset[i]->process);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     special\t\t/*special processing */",
		    &recSup->papRset[i]->special,
		    recSup->papRset[i]->special);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_precision\t/* get_precision of this type */",
		    &recSup->papRset[i]->get_precision,
		    recSup->papRset[i]->get_precision);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_value\t\t/*get value field */",
		    &recSup->papRset[i]->get_value,
		    recSup->papRset[i]->get_value);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     cvt_dbaddr\t\t/*cvt dbAddr */",
		    &recSup->papRset[i]->cvt_dbaddr,
		    recSup->papRset[i]->cvt_dbaddr);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_array_info\t/* get_array_info of this type */",
		    &recSup->papRset[i]->get_array_info,
		    recSup->papRset[i]->get_array_info);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     put_array_info\t/* put_array_info of this type */",
		    &recSup->papRset[i]->put_array_info,
		    recSup->papRset[i]->put_array_info);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_enum_str\t/*get string from enum item*/",
		    &recSup->papRset[i]->get_enum_str,
		    recSup->papRset[i]->get_enum_str);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_units \t\t/* get_units of this type */",
		    &recSup->papRset[i]->get_units,
		    recSup->papRset[i]->get_units);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_graphic_double /* get_graphic_double of this type */",
		    &recSup->papRset[i]->get_graphic_double,
		    recSup->papRset[i]->get_graphic_double);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_control_double /* get_control_double of this type */",
		    &recSup->papRset[i]->get_control_double,
		    recSup->papRset[i]->get_control_double);
	    bufOut(fp, fflag);
	    sprintf(buffer, "%8x[%8x]\t     get_enum_strs\t/*get all enum strings */",
		    &recSup->papRset[i]->get_enum_strs,
		    recSup->papRset[i]->get_enum_strs);
	    bufOut(fp, fflag);
	}
    }
}				/* end of recSup */
