/* devMbbiCamac.c */
/* devMbbiCamac.c - Device Support Routines for Camac Multi-bit binary input */
/* Camac 32 bit Multibit binary input					     */
/*
 *      Original Author: Dave Barker
 *      Current Author:  Johnny Tang
 *      Date:            6th August 1993
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, SURA CEBAF.
 *
 *
 * Modification Log:
 * -----------------
 *      4-28-94		jt
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
#include	<module_types.h>
#include	<mbbiRecord.h>
#include        "camacLib.h"

/* Create the dset for devAiMbbiXVme210 */
static long init();
static long init_record();
static long read_mbbi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
}devMbbiCamac={
	5,
	NULL,
	init,
	init_record,
	NULL,
	read_mbbi};

static long init(after)
int after;
{
#ifdef DEBUG_ON
    if ( CDEBUG)printf("devMbbiCamac (init) Called. pass = %d\n", after);
#endif
    return(0);
}

static long init_record(pmbbi)
struct mbbiRecord	*pmbbi;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if ( CDEBUG)printf("devMbbiCamac (init_record) Called.\n");
#endif

    /* mbbi.inp must be a CAMAC_IO */
    switch (pmbbi->inp.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
            if ( CDEBUG)printf("devMbbiCamac (init_record): malloc failed.\n");
#endif
            return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(pmbbi->inp.value);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devMbbiCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT); /* cdreg failed if ext is zero */

        fsd = atoi((char *)pcamacio->parm);

/*  set shft and mask fields to the bit position 
	(where the least significatn bit is 0) */
	pmbbi->shft = fsd;
	pmbbi->mask <<= fsd;
	pcio->f = pcamacio->f;
        pmbbi->dpvt = (long *)pcio;

	break;

    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
		"devMbbiCamac (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(CONVERT);
}

static long read_mbbi(pmbbi)
struct mbbiRecord	*pmbbi;
{
register struct dinfo *pcio;
int q;
unsigned long val;

        pcio = (struct dinfo *)pmbbi->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devMbbiCamac (read_mbbi):  f=%d ext=%ld mask=%ld\n",
                pcio->f, pcio->ext, pmbbi->mask);
#endif
        q=0;
        cfsa(pcio->f, pcio->ext, &val, &q);
	if(q)
	{
		pmbbi->rval = val&pmbbi->mask; 
		return(CONVERT);
        }
        else return(DO_NOT_CONVERT);
}
