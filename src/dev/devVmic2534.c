/* devVmic2534.c */
/* share/src/dev @(#)devVmic2534.c	1.2     6/3/93 */

#ifndef lint
static char rcsid[] =
    "@(#) /babar/CVSROOT/epics/base/src/dev/devVmic2534.c,v 1.1 1996/01/09 19:54:24 lewis Exp(LBL)";
#endif


/*
 *      device suppoer routines for V.M.I.C. VMIVME-2534
 *      digital i/o module
 *
 *      Author:      Bill Brown
 *      Date:        03/22/94
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1994, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (DE-AC03-76SF00) at Lawrence Berkeley Laboratory.
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              Control System Group
 *              Advanced Light Source
 *              Lawrence Berkeley Laboratory
 *
 *      Co-developed with
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      NOTES:
 *          This code is/was cloned from "devXy240.c" of EPICS R3.11
 *
 *
 *          This code is initially designed to work with boards equipped
 *          the "I/O true data" option.  It _may_ be possible to determine
 *          the presence or absence of this option at initialization time
 *          and provide appropriate bit-flipping at run-time.
 *
 *          Bits 0-15 are "hard-wired" as outputs.
 *          Bits 16-31 are "hard-wired" as inputs.
 *
 *
 * Modification Log:
 * -----------------
 *
    @(#) devVmic2534.c,v
    @(#) Revision 1.1  1996/01/09 19:54:24  lewis
    @(#) Add PAL record.  Remove APS eg/er records.
    @(#) Add 4 STAR drivers, LBL 2534, and preliminaries for LBL/SLAC 9210/9325 drivers.
    @(#)
 * Revision 1.1.1.1  1995/07/25  21:49:33  lewis
 * Baseline import R3.12 w/LBL drivers and mods.
 *
 * Revision 1.3  1994/08/19  15:17:54  wbrown
 * added debug #ifdef, printf statements
 *
 * Revision 1.2  94/07/20  09:27:01  wbrown
 * modified to dynamically assign data direction at
 *   record init time.
 * 
 * Revision 1.1  94/07/12  10:30:14  wbrown
 * Initial revision
 * 
 * 
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<dbScan.h>
#include	<link.h>
#include	<module_types.h>
#include	<biRecord.h>
#include	<boRecord.h>
#include	<mbbiRecord.h>
#include	<mbboRecord.h>


static long init_bi();
static long init_bo();
static long init_mbbi();
static long init_mbbo();
static long bi_ioinfo();
static long mbbi_ioinfo();
static long read_bi();
static long write_bo();
static long read_mbbi();
static long write_mbbo();


typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	} BINARYDSET;


BINARYDSET devBiVmic2534=  {6,NULL,NULL,init_bi,  bi_ioinfo,  read_bi};
BINARYDSET devBoVmic2534=  {6,NULL,NULL,init_bo,       NULL,  write_bo};
BINARYDSET devMbbiVmic2534={6,NULL,NULL,init_mbbi,mbbi_ioinfo,read_mbbi};
BINARYDSET devMbboVmic2534={6,NULL,NULL,init_mbbo,       NULL,write_mbbo};

static long init_bi( struct biRecord	*pbi)
{
    struct vmeio *pvmeio;


    /* bi.inp must be an VME_IO */
    switch (pbi->inp.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbi->inp.value);
	pbi->mask=1;
	pbi->mask <<= pvmeio->signal;
#ifdef DEBUG_2534
	printf("Input: signal = %x, mask = %08x\n", pvmeio->signal, pbi->mask);
#endif DEBUG_2534
	if ( vmic2534_setIOBitDirection( pvmeio->card, pbi->mask, 0 ) != 0 )
	    {
	    pbi->mask = 0;
	    recGblRecordError(S_db_badField,(void *)pbi,
		"devBiVmic2534 (init_record) Data Direction Conflict");
	    return(S_db_badField);
	    }
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiVmic2534 (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long bi_ioinfo(
    int               cmd,
    struct biRecord     *pbi,
    IOSCANPVT		*ppvt)
{
    vmic2534_getioscanpvt(pbi->inp.value.vmeio.card,ppvt);
    return(0);
}

static long read_bi(struct biRecord	*pbi)
{
	struct vmeio *pvmeio;
	int	    status;
	long	    value;

	
	pvmeio = (struct vmeio *)&(pbi->inp.value);
	status = vmic2534_bi_driver(pvmeio->card,pbi->mask,&value);
	if(status==0) {
		pbi->rval = value;
		return(0);
	} else {
                recGblSetSevr(pbi,READ_ALARM,INVALID_ALARM);
		return(2);
	}
}

static long init_bo(struct boRecord	*pbo)
{
    unsigned int value;
    int status=0;
    struct vmeio *pvmeio;

    /* bo.out must be an VME_IO */
    switch (pbo->out.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbo->out.value);
	pbo->mask = 1;
	pbo->mask <<= pvmeio->signal;
#ifdef DEBUG_2534
	printf("Output: signal = 0x%x, mask = %08x\n", pvmeio->signal, pbo->mask);
#endif DEBUG_2534
	if ( vmic2534_setIOBitDirection( pvmeio->card, pbo->mask, 1 ) != 0 )
	    {
	    pbo->mask = 0;
	    status = S_db_badField;
	    recGblRecordError(status,(void *)pbo,
		"devBoVmic2534 (init_record) Data Direction Conflict");
	    }
	else
	    {
	    status = vmic2534_bo_read(pvmeio->card,pbo->mask,&value);
	    if(status == 0) pbo->rbv = pbo->rval = value;
	    else status = 2;
	    }
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pbo,
	    "devBoVmic2534 (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_bo(struct boRecord	*pbo)
{
	struct vmeio *pvmeio;
	int	    status;

	
	pvmeio = (struct vmeio *)&(pbo->out.value);
	status = vmic2534_bo_driver(pvmeio->card,pbo->rval,pbo->mask);
	if(status!=0) {
                recGblSetSevr(pbo,WRITE_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long init_mbbi(struct mbbiRecord	*pmbbi)
{
    struct vmeio *pvmeio;

    /* mbbi.inp must be an VME_IO */
    switch (pmbbi->inp.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pmbbi->inp.value);
	pmbbi->shft = pmbbi->inp.value.vmeio.signal;
	pmbbi->mask <<= pmbbi->shft;
	if ( vmic2534_setIOBitDirection( pvmeio->card, pmbbi->mask, 0 ) != 0 )
	    {
	    pmbbi->mask = 0;
	    recGblRecordError(S_db_badField,(void *)pmbbi,
		    "devMbbiVmic2534 (init_record) Data Direction Conflict");
	    return(S_db_badField);
	    }
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
		"devMbbiVmic2534 (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long mbbi_ioinfo(
    int               cmd,
    struct mbbiRecord     *pmbbi,
    IOSCANPVT		*ppvt)
{
    vmic2534_getioscanpvt(pmbbi->inp.value.vmeio.card,ppvt);
    return(0);
}

static long read_mbbi(struct mbbiRecord	*pmbbi)
{
	struct vmeio	*pvmeio;
	int		status;
	unsigned long	value;

	
	pvmeio = (struct vmeio *)&(pmbbi->inp.value);
	status = vmic2534_bi_driver(pvmeio->card,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
	} else {
                recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long init_mbbo(struct mbboRecord	*pmbbo)
{
    unsigned long value;
    struct vmeio *pvmeio;
    int		status = 0;

    /* mbbo.out must be an VME_IO */
    switch (pmbbo->out.type) {
    case (VME_IO) :
	pvmeio = &(pmbbo->out.value.vmeio);
	pmbbo->shft = pvmeio->signal;
	pmbbo->mask <<= pmbbo->shft;
	if ( vmic2534_setIOBitDirection( pvmeio->card, pmbbo->mask, 1 ) != 0 )
	    {
	    pmbbo->mask = 0;
	    recGblRecordError(S_db_badField,(void *)pmbbo,
		    "devMbbiVmic2534 (init_record) Data Direction Conflict");
	    return(S_db_badField);
	    }
	status = vmic2534_bo_read(pvmeio->card,pmbbo->mask,&value);
	if(status==0) pmbbo->rbv = pmbbo->rval = value;
	else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pmbbo,
		"devMbboVmic2534 (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_mbbo(struct mbboRecord	*pmbbo)
{
	struct vmeio *pvmeio;
	int	    status;
	unsigned long value;

	
	pvmeio = &(pmbbo->out.value.vmeio);
	status = vmic2534_bo_driver(pvmeio->card,pmbbo->rval,pmbbo->mask);
	if(status==0) {
		status = vmic2534_bo_read(pvmeio->card,pmbbo->mask,&value);
		if(status==0) pmbbo->rbv = value;
                else recGblSetSevr(pmbbo,READ_ALARM,INVALID_ALARM);
	} else {
                recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM);
	}
	return(status);
}

