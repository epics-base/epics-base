/* devMbboCamac.c */
/* devMbboCamac.c - Device Support Routines for Camac Multi-bit binary output	*/
/*  32 bit binary output						 	*/
/*
 *      Original Author: Dave Barker
 *      Current Author:  Johnny Tang
 *      Date:            6th Aug 1993
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, SURA CEBAF.
 *
 *
 *
 * Modification Log:
 * -----------------
 *      4-28-94		jt
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<devCamac.h>
#include	<module_types.h>
#include	<mbboRecord.h>
#include        "camacLib.h"

/* Create the dset for devMbboCamac */
static long init();
static long init_record();
static long write_mbbo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboCamac={
	5,
	NULL,
	init,
	init_record,
	NULL,
	write_mbbo};

static long init(after)
int after;
{
#ifdef DEBUG_ON
    if ( CDEBUG)printf("devMbboCamac (init) called, pass=%d\n", after);
#endif
    return(0);
}

static long init_record(pmbbo)
struct mbboRecord	*pmbbo;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if ( CDEBUG)printf("devMbboCamac (init_record) called.\n");
#endif

    /* mbbo.out must be a CAMAC_IO */
    switch (pmbbo->out.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
             if ( CDEBUG)printf("devMbboCamac (init_record): malloc failed.\n");
#endif
             return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(pmbbo->out.value);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devMbboCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT);

        fsd = (int)atoi((char *)pcamacio->parm);



/*  set shft and mask fields to the bit position starting at 0 */
	pmbbo->shft = fsd;
	pmbbo->mask <<= fsd;


        pcio->f = pcamacio->f;

        pmbbo->dpvt = (long *)pcio;
        break;

    default :
	recGblRecordError(S_db_badField,(void *)pmbbo,
		"devMbboCamac (init_record) Illegal OUT field");
        return(S_db_badField);
    }
    return(CONVERT);
}

static long write_mbbo(pmbbo)
struct mbboRecord	*pmbbo;
{
register struct dinfo *pcio;
int   q;

        pcio = (struct dinfo *)pmbbo->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);

        q = 0;
        cfsa(pcio->f, pcio->ext, &(pmbbo->rval), &q);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devMbboCamac (write_mbbo):  f=%d ext=%ld mask=%ld value=%d\n",
                pcio->f, pcio->ext, pcio->mask, pmbbo->val);
#endif
        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
