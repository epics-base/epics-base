/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	Author: Marty Kraimer
 *	Date:	9/28/95
 *	Replacement for old bldCvtTable
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "dbDefs.h"
#include "ellLib.h"
#include "cvtTable.h"

#define MAX_LINE_SIZE 160
#define MAX_BREAKS 100
struct brkCreateInfo {
    double           engLow;	/* Lowest value desired: engineering units */
    double           engHigh;	/* Highest value desired: engineering units */
    double            rawLow;	/* Raw value for EngLow			 */
    double            rawHigh;	/* Raw value for EngHigh		 */
    double           accuracy;	/* accuracy desired in engineering units */
    double           tblEngFirst;/* First table value: engineering units */
    double           tblEngLast;	/* Last table value: engineering units	 */
    double           tblEngDelta;/* Change per table entry: eng units	 */
    long            nTable;	/* number of table entries 		 */
    /* (last-first)/delta + 1		 */
    double          *pTable;	/* addr of data table			 */
} brkCreateInfo;

typedef struct brkInt {	/* breakpoint interval */
    double	raw;	/* raw value for beginning of interval */
    double	slope;	/* slope for interval */
    double	eng;	/* converted value for beginning of interval */
} brkInt;

brkInt brkint[MAX_BREAKS];

static int create_break(struct brkCreateInfo *pbci, brkInt *pabrkInt,
	int max_breaks, int *n_breaks);
static char inbuf[MAX_LINE_SIZE];
static int linenum=0;

typedef struct dataList{
	struct dataList *next;
	double		value;
}dataList;

static int getNumber(char **pbeg, double *value)
{
    int	 nchars=0;

    while(isspace((int)**pbeg) && **pbeg!= '\0') (*pbeg)++;
    if(**pbeg == '!' || **pbeg == '\0') return(-1);
    if(sscanf(*pbeg,"%lf%n",value,&nchars)!=1) return(-1);
    *pbeg += nchars;
    return(0);
}
	
static void errExit(char *pmessage)
{
    fprintf(stderr, "%s\n", pmessage);
    fflush(stderr);
    exit(-1);
}

int main(int argc, char **argv)
{
    char	*pbeg;
    char	*pend;
    double	value;
    char	*pname = NULL;
    dataList	*phead;
    dataList	*pdataList;
    dataList	*pnext;
    double	*pdata;
    long	ndata;
    int		nBreak,n;
    size_t  len;
    char	*outFilename;
    char	*pext;
    FILE	*outFile;
    FILE	*inFile;
    char	*plastSlash;
    

    if(argc<2) {
	fprintf(stderr,"usage: makeBpt file.data [outfile]\n");
	exit(-1);
    }
    if (argc==2) {
	plastSlash = strrchr(argv[1],'/');
	plastSlash = (plastSlash ? plastSlash+1 : argv[1]);
	outFilename = calloc(1,strlen(plastSlash)+2);
	if(!outFilename) {
	    fprintf(stderr,"calloc failed\n");
	    exit(-1);
	}
	strcpy(outFilename,plastSlash);
	pext = strstr(outFilename,".data");
	if(!pext) {
	    fprintf(stderr,"Input file MUST have .data  extension\n");
	    exit(-1);
	}
	strcpy(pext,".dbd");
    } else {
	outFilename = calloc(1,strlen(argv[2])+1);
	if(!outFilename) {
	    fprintf(stderr,"calloc failed\n");
	    exit(-1);
	}
	strcpy(outFilename,argv[2]);
    }
    inFile = fopen(argv[1],"r");
    if(!inFile) {
	fprintf(stderr,"Error opening %s\n",argv[1]);
	exit(-1);
    }
    outFile = fopen(outFilename,"w");
    if(!outFile) {
	fprintf(stderr,"Error opening %s\n",outFilename);
	exit(-1);
    }
    while(fgets(inbuf,MAX_LINE_SIZE,inFile)) {
	linenum++;
	pbeg = inbuf;
	while(isspace((int)*pbeg) && *pbeg!= '\0') pbeg++;
	if(*pbeg == '!' || *pbeg == '\0') continue;
	while(*pbeg!='"' && *pbeg!= '\0') pbeg++;
	if(*pbeg!='"' ) errExit("Illegal Header");
	pbeg++; pend = pbeg;
	while(*pend!='"' && *pend!= '\0') pend++;
	if(*pend!='"') errExit("Illegal Header");
	len = pend - pbeg;
	if(len<=1) errExit("Illegal Header");
	pname = calloc(len+1,sizeof(char));
	if(!pname) {
	    fprintf(stderr,"calloc failed while processing line %d\n",linenum);
	    exit(-1);
	}
	strncpy(pname,pbeg,len);
	pname[len]='\0';
	pbeg = pend + 1;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.engLow = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.rawLow = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.engHigh = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.rawHigh = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.accuracy = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.tblEngFirst = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.tblEngLast = value;
	if(getNumber(&pbeg,&value)) errExit("Illegal Header");
	brkCreateInfo.tblEngDelta = value;
	goto got_header;
    }
    errExit("Illegal Header");
got_header:
    phead = pnext = 0;
    ndata = 0;
    errno = 0;
    while(fgets(inbuf,MAX_LINE_SIZE,inFile)) {
	double	value;

	linenum++;
	pbeg = inbuf;
	while(!getNumber(&pbeg,&value)) {
	    ndata++;
	    pdataList = (dataList *)calloc(1,sizeof(dataList));
	    if(!pdataList) {
		fprintf(stderr,"calloc failed (after header)"
			" while processing line %d\n",linenum);
		exit(-1);
	    }
	    if(!phead) 
		phead = pdataList;
	    else
		pnext->next = pdataList;
	    pdataList->value = value;
	    pnext = pdataList;
	}
    }
    if(!pname) {
	errExit("create_break failed: no name specified\n");
    }
    brkCreateInfo.nTable = ndata;
    pdata = (double *)calloc(brkCreateInfo.nTable,sizeof(double));
    if(!pdata) {
	fprintf(stderr,"calloc failed for table length %ld\n",brkCreateInfo.nTable);
	exit(-1);
    }
    pnext = phead;
    for(n=0; n<brkCreateInfo.nTable; n++) {
	pdata[n] = pnext->value;
	pdataList = pnext;
	pnext = pnext->next;
	free((void *)pdataList);
    }
    brkCreateInfo.pTable = pdata;
    if(create_break(&brkCreateInfo,&brkint[0],MAX_BREAKS,&nBreak)) 
	errExit("create_break failed\n");
    fprintf(outFile,"breaktable(%s) {\n",pname);
    for(n=0; n<nBreak; n++) {
	fprintf(outFile,"\t%f %f\n",brkint[n].raw,brkint[n].eng);
    }
    fprintf(outFile,"}\n");
    fclose(inFile);
    fclose(outFile);
    return(0);
}

static int create_break( struct brkCreateInfo *pbci, brkInt *pabrkInt,
    int max_breaks, int *n_breaks)
{
    brkInt  *pbrkInt;
    double          *table = pbci->pTable;
    long            ntable = pbci->nTable;
    double          ilow,
                    ihigh,
                    tbllow,
                    tblhigh,
                    slope,
                    offset;
    int             ibeg,
                    iend,
                    i,
                    inc,
                    imax,
                    n;
    double          rawBeg,
                    engBeg,
                    rawEnd,
                    engEnd,
                    engCalc,
                    engActual,
                    error;
    int             valid,
                    all_ok,
                    expanding;
    /* make checks to ensure that brkCreateInfo makes sense */
    if (pbci->engLow >= pbci->engHigh) {
	errExit("create_break: engLow >= engHigh");
	return (-1);
    }
    if ((pbci->engLow < pbci->tblEngFirst)
	    || (pbci->engHigh > pbci->tblEngLast)) {
	errExit("create_break: engLow > engHigh");
	return (-1);
    }
    if (pbci->tblEngDelta <= 0.0) {
	errExit("create_break: tblEngDelta <= 0.0");
	return (-1);
    }
    if (ntable < 3) {
	errExit("raw data must have at least 3 elements");
	return (-1);
    }
/***************************************************************************
			 Convert Table to raw values
 *
 * raw and table values are assumed to be related by an equation of the form:
 *
 * raw = slope*table + offset
 *
 * The following algorithm converts each table value to raw units
 *
 * 1) Finds the locations in Table corresponding to engLow and engHigh
 *    Note that these locations need not be exact integers
 * 2) Interpolates to obtain table values corresponding to engLow and enghigh
 *    we now have the equations:
 *    rawLow = slope*tblLow + offset
 *    rawHigh = slope*tblHigh + offset
 * 4) Solving these equations for slope and offset gives:
 *    slope=(rawHigh-rawLow)/(tblHigh-tblLow)
 *    offset=rawHigh-slope*tblHigh
 * 5) for each table value set table[i]=table[i]*slope+offset
 *************************************************************************/
    /* Find engLow in Table  and then compute tblLow */
    ilow = (pbci->engLow - pbci->tblEngFirst) / (pbci->tblEngDelta);
    i = (int) ilow;
    if (i >= ntable - 1)
	i = ntable - 2;
    tbllow = table[i] + (table[i + 1] - table[i]) * (ilow - (double) i);
    /* Find engHigh in Table and then compute tblHigh */
    ihigh = (pbci->engHigh - pbci->tblEngFirst) / (pbci->tblEngDelta);
    i = (int) ihigh;
    if (i >= ntable - 1)
	i = ntable - 2;
    tblhigh = table[i] + (table[i + 1] - table[i]) * (ihigh - (double) i);
    /* compute slope and offset */
    slope = (pbci->rawHigh - pbci->rawLow) / (tblhigh - tbllow);
    offset = pbci->rawHigh - slope * tblhigh;
    /* convert table to raw units */
    for (i = 0; i < ntable; i++)
	table[i] = table[i] * slope + offset;

/*****************************************************************************
 *		Now create break point table
 *
 * The algorithm does the following:
 *
 * It finds one breakpoint interval at a time. For each it does the following:
 *
 * 1) Use a relatively large portion of the remaining table as an interval
 * 2) It attempts to use the entire interval as a breakpoint interval
 *	Success is determined by the following algorithm:
 *	  a) compute the slope using the entire interval
 *        b) for each table entry in the interval determine the eng value
 *	     using the slope just determined.
 *	  c) compare the computed value with eng value associated with table
 *        d) if all table entries are within the accuracy desired then success.
 * 3) If successful then attempt to expand the interval and try again.
 *    Note that it is expanded by up to 1/10 of the table size.
 * 4) If not successful reduce the interval by 1 and try again.
 *    Once the interval is being decreased it will never be increased again.
 * 5) The algorithm will ultimately fail or will have determined the optimum
 *    breakpoint interval
 *************************************************************************/

    /* Must start with table entry corresponding to engLow; */
    i = (int) ilow;
    if (i >= ntable - 1)
	i = ntable - 2;
    rawBeg = table[i] + (table[i + 1] - table[i]) * (ilow - (double) i);
    engBeg = pbci->engLow;
    ibeg = (int) (ilow);       /* Make sure that ibeg > ilow */
    if( ibeg < ilow ) ibeg = ibeg + 1;
    /* start first breakpoint interval */
    n = 1;
    pbrkInt = pabrkInt;
    pbrkInt->raw = rawBeg;
    pbrkInt->eng = engBeg;
    /* determine next breakpoint interval */
    while ((engBeg <= pbci->engHigh) && (ibeg < ntable - 1)) {
	/* determine next interval to try. Up to 1/10 full range */
	rawEnd = rawBeg;
	engEnd = engBeg;
	iend = ibeg;
	inc = (int) ((ihigh - ilow) / 10.0);
	if (inc < 1)
	    inc = 1;
	valid = TRUE;
	/* keep trying intervals until cant do better */
	expanding = TRUE;	/* originally we are trying larger and larger
				 * intervals */
	while (valid) {
	    imax = iend + inc;
	    if (imax >= ntable) {
		/* don't go past end of table */
		imax = ntable - 1;
		inc = ntable - iend - 1;
		expanding = FALSE;
	    }
	    if (imax > (int) (ihigh + 1.0)) {	/* Don't go to far past
						 * engHigh */
		imax = (int) (ihigh + 1.0);
		inc = (int) (ihigh + 1.0) - iend;
		expanding = FALSE;
	    }
	    if (imax <= ibeg)
		break;		/* failure */
	    rawEnd = table[imax];
	    engEnd = pbci->tblEngFirst + (double) imax *(pbci->tblEngDelta);
	    slope = (engEnd - engBeg) / (rawEnd - rawBeg);
	    all_ok = TRUE;
	    for (i = ibeg + 1; i <= imax; i++) {
		engCalc = engBeg + slope * (table[i] - rawBeg);
		engActual = pbci->tblEngFirst + ((double) i) * (pbci->tblEngDelta);
		error = engCalc - engActual;
		if (error < 0.0)
		    error = -error;
		if (error >= pbci->accuracy) {
		    /* we will be trying smaller intervals */
		    expanding = FALSE;
		    /* just decrease inc and let while(valid) try again */
		    inc--;
		    all_ok = FALSE;
		    break;
		}
	    }			/* end for */
	    if (all_ok) {
		iend = imax;
		/* if not expanding we found interval */
		if (!expanding)
		    break;
		/* will automatically try larger interval */
	    }
	}			/* end while(valid) */
	/* either we failed or optimal interval has been found */
	if ((iend <= ibeg) && (iend < (int) ihigh)) {
	    errExit("Could not meet accuracy criteria");
	    return (-1);
	}
	pbrkInt->slope = slope;
	/* get ready for next breakpoint interval */
	if (n++ >= max_breaks) {
	    errExit("Break point table too large");
	    return (-1);
	}
	ibeg = iend;
	pbrkInt++;
	rawBeg = rawEnd;
	engBeg = engEnd;
	pbrkInt->raw = rawBeg;
	pbrkInt->eng = engBeg + (pbrkInt->raw - rawBeg) * slope;
    }
    pbrkInt->slope = 0.0;
    *n_breaks = n;
    return (0);
}
