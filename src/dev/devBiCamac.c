/* devBiCamac.c */
/* devBiCamac.c - Device Support Routines for Generic Camac Binary input */
/*
 *      Original Author: Dave Barker
 *      Current Author:  Johnny Tang
 *      Date:            25th July 1993.
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1993, SURA CEBAF.
 *
 *
 * Modification Log:
 * -----------------
 *      April 22, 1994 	jt	
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<devCamac.h>
#include	<link.h>
#include	<module_types.h>
#include	<biRecord.h>
#include "camacLib.h"


/* Create the dset for devBiCamac */
static long init();
static long init_record();
static long read_bi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
}devBiCamac={
	5,
	NULL,
	init,
	init_record,
	NULL,
	read_bi};

static long init(after)
    int after;
{
#ifdef DEBUG_ON
    if ( CDEBUG)printf("devBiCamac (init) Called. pass = %d\n", after);
#endif
    return(0);
}

static long init_record(pbi)
struct biRecord	*pbi;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if ( CDEBUG)printf("devBiCamac (init_record) Called.\n");
#endif

    /* bi.inp must be a CAMAC_IO */
    switch (pbi->inp.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
            if ( CDEBUG)printf("devBiCamac (init_record): malloc failed.\n");
#endif
            return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(pbi->inp.value);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devBiCamac (init_record): B=%d C=%d N=%d A=%d F=%d PARM = %s\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f, pcamacio->parm);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT); /* cdreg failed if ext is zero */

	fsd = atoi((char *)pcamacio->parm);
/*  for a binary, the parameter should indicate which bit starting with 0 is to be turned on */
	pbi->mask = 1;
        pbi->mask <<= fsd;
        pcio->f = pcamacio->f;

        pbi->dpvt = (long *)pcio;

	break;

    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiCamac (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_bi(pbi)
struct biRecord	*pbi;
{
register struct dinfo *pcio;
int         q;

        pcio = (struct dinfo *)pbi->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devBiCamac (read_bi):  f=%d ext=%ld mask=%ld\n",
                pcio->f, pcio->ext, pbi->mask);
#endif
        q=0;
        cfsa(pcio->f, pcio->ext, &(pbi->rval), &q);
        pbi->rval &= pbi->mask;

        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
