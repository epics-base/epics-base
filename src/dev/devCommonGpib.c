/* devXxCommonGpib.c */
/* share/src/devOpt $Id$ */
/*
 *      Author: 		John Winans
 *      Origional Author:	Ned D. Arnold
 *      Date:   		11-20-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1988, 1989, the Regents of the University of California,
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
 * All rights reserved. No part of this publication may be reproduced, 
 * stored in a retrieval system, transmitted, in any form or by any
 * means,  electronic, mechanical, photocopying, recording, or otherwise
 * without prior written permission of Los Alamos National Laboratory
 * and Argonne National Laboratory.
 *
 * Modification Log:
 * -----------------
 * .01  05-30-91        nda     Initial Release
 * .02  06-18-91	nda	init_rec_mbbo must return(2) if no init value
 * .03  07-02-91	nda	renamed String/StringOut DSET's to Si/So
 * .04  07-11-91	jrw	added the callbackRequest to process()
 * .05  07-15-91	jrw	redesigned init processing... more generic
 * .06  11-01-91	jrw	major rework to fit into new GPIB driver
 * .07  11-11-91	jrw	added new version of SRQ support
 * .08  11-17-91	jrw	changed to support SR620
 * .09  11-20-91	jrw	redesigned as a library
 * .10  11-22-91	jrw	removed output formatting for all but GPIBWRITEs
 * .11	01-10-92	jrw	changed return from GPIBSOFT (propagated, was 0)
 * .12	02-05-92	jba	Changed process parameter from precord->pdba to precord
 * .13	02-18-92	jrw	Changed return from the AO init function to 2
 * .14	02-26-92	jrw	added return codes to the output work functions
 * .15	02-27-92	jrw	added the setting of PACT to 1 when init fails
 * .16	04-08-92	jrw	reordered initXx to clean up SRQ init code
 * .17  04-30-92	jrw	added waveform record support
 * .18  07-10-92	jrw	initXx endless loop looking for hwpvt
 *
 * WISH LIST:
 *  It would be nice to read and write directly to/from the val field
 *  when operating on SI and SO records.  The strncpy looks like a waste.
 */

/* This MUST be deleted when the new dbScanner stuff is put into EPICS! */
/* any existing databases will also have to be changed from passive, to */
/* I/O event scanned for magic SRQ processing */
#define SCAN_IO_EVENT 0		/* This should be in an epicsH header file */

#include	<vxWorks.h>
#include	<taskLib.h>
#include	<rngLib.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<recSup.h>
#include	<callback.h>
#include	<drvSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<dbCommon.h>
#include	<aiRecord.h>
#include	<aoRecord.h>
#include	<biRecord.h>
#include	<boRecord.h>
#include	<mbbiRecord.h>
#include	<mbboRecord.h>
#include	<stringinRecord.h>
#include	<stringoutRecord.h>
#include	<longinRecord.h>
#include	<longoutRecord.h>
#include        <waveformRecord.h>

#include	<drvGpibInterface.h>
#include	<devCommonGpib.h>

#ifndef VALID_ALARM
#define	VALID_ALARM INVALID_ALARM
#endif

/*
 * This external structure contains the entry points to the GPIB drver.
 */

extern struct {
  long          number;
  DRVSUPFUN     report;		/* call this to get stats about the link */
  DRVSUPFUN     init;		/* do NOT call this...  epics calls it */
  int           (*qGpibReq)();
  int           (*registerSrqCallback)();
  int           (*writeIb)();
  int           (*readIb)();
  int           (*writeIbCmd)();
  int           (*ioctl)();
} drvGpib;

extern int	ibSrqDebug;

/******************************************************************************
 *
 * Print a report of operating statistics for all devices supported by this
 * module.
 *
 ******************************************************************************/

long
devGpibLib_report(dset)
gDset *dset;
{
  struct hwpvt  *phwpvt;
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(dset->funPtr[dset->number]);
  phwpvt = parmBlock->hwpvtHead;

  while (phwpvt != NULL)
  {
    if (phwpvt->linkType == GPIB_IO)
        printf("  NI-link %d, node %d, timeouts %ld\n", phwpvt->link, phwpvt->device, phwpvt->tmoCount);
    else if (phwpvt->linkType == BBGPIB_IO)
        printf("  BB-link %d, bug %d, node %d, timeouts %ld\n", phwpvt->link, phwpvt->bug, phwpvt->device, phwpvt->tmoCount);

    phwpvt = phwpvt->next;
  }
  return(0);
}

/******************************************************************************
 *
 * Initialization for device support
 * This is called one time before any records are initialized with a parm
 * value of 0.  And then again AFTER all record-level init is complete
 * with a param value of 1.
 *
 ******************************************************************************/

long 
devGpibLib_initDevSup(parm, dset)
int	parm;	/* set to 0 on pre-rec init call, and 1 on post-rec init call */
gDset	*dset;	/* pointer to dset used to reference the init function */
{
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(dset->funPtr[dset->number]);

  if ((parm == 0) && *parmBlock->debugFlag)
    printf("%s: Device Support Initializing ...\n", parmBlock->name);

  return(OK);
}

/******************************************************************************
 *
 * Initialization routines.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * ai record init
 *
 ******************************************************************************/

long 
devGpibLib_initAi(pai, process)
struct aiRecord	*pai;
void		(*process)();
{
  long		result;
  char		message[100];
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(((gDset*)(pai->dset))->funPtr[pai->dset->number]);

  /* Do common initialization */
  if (result = devGpibLib_initXx((caddr_t) pai, &pai->inp))
  {
    return(result);
  }

  /* make sure the command type makes sendse for the record type */

  if (parmBlock->gpibCmds[((struct gpibDpvt *)pai->dpvt)->parm].type != GPIBREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pai->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pai->dpvt)->parm].type != GPIBRAWREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pai->dpvt)->parm].type != GPIBREADW)
  {
    sprintf(message, "%s: devGpibLib_initAi: invalid command type for an AI record in param %d\n", pai->name, ((struct gpibDpvt *)pai->dpvt)->parm);
    errMessage(S_db_badField, message);
    pai->pact = TRUE;
    return(S_db_badField);
  }
  return(0);
}

/******************************************************************************
 *
 * ao record init
 *
 ******************************************************************************/

long 
devGpibLib_initAo(pao, process)
struct aoRecord	*pao;
void 		(*process)();
{
  long 		result;
  char		message[100];
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(((gDset*)(pao->dset))->funPtr[pao->dset->number]);

  /* do common initialization */
  if (result = devGpibLib_initXx((caddr_t) pao, &pao->out))
  {
    return(result);
  }
  /* make sure the command type makes sendse for the record type */

  if (parmBlock->gpibCmds[((struct gpibDpvt *)pao->dpvt)->parm].type != GPIBWRITE &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pao->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pao->dpvt)->parm].type != GPIBCMD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pao->dpvt)->parm].type != GPIBCNTL)
  {
      sprintf(message, "%s: devGpibLib_initAo: invalid command type for an AO record in param %d\n", pao->name, ((struct gpibDpvt *)pao->dpvt)->parm);
      errMessage(S_db_badField, message);
      pao->pact = TRUE;
      return(S_db_badField);
  }
  return(2);	/* Don't let record processing do an RVAL to VAL conversion */
}

/******************************************************************************
 *
 * li record init
 *
 ******************************************************************************/

long
devGpibLib_initLi(pli, process)
struct longinRecord *pli;
void            (*process)();
{
  long          result;
  char		message[100];
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(((gDset*)(pli->dset))->funPtr[pli->dset->number]);

  /* Do common initialization */
  if (result = devGpibLib_initXx((caddr_t) pli, &pli->inp))
  {
    return(result);
  }

  /* make sure the command type makes sendse for the record type */

  if (parmBlock->gpibCmds[((struct gpibDpvt *)pli->dpvt)->parm].type != GPIBREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pli->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pli->dpvt)->parm].type != GPIBRAWREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pli->dpvt)->parm].type != GPIBREADW)
  {
    sprintf(message, "%s: devGpibLib_initLi: invalid command type for an LI record in param %d\n", pli->name, ((struct gpibDpvt *)pli->dpvt)->parm);
    errMessage(S_db_badField, message);
    pli->pact = TRUE;
    return(S_db_badField);
  }

  return(0);
}

/******************************************************************************
 *
 * lo record init
 *
 ******************************************************************************/

long
devGpibLib_initLo(plo, process)
struct longoutRecord *plo;
void            (*process)();
{
  long          result;
  char		message[100];
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(((gDset*)(plo->dset))->funPtr[plo->dset->number]);

  /* do common initialization */
  if (result = devGpibLib_initXx((caddr_t) plo, &plo->out))
  {
    return(result);
  }
  if (parmBlock->gpibCmds[((struct gpibDpvt *)plo->dpvt)->parm].type != GPIBWRITE &&
      parmBlock->gpibCmds[((struct gpibDpvt *)plo->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)plo->dpvt)->parm].type != GPIBCMD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)plo->dpvt)->parm].type != GPIBCNTL)
  {
      sprintf(message, "%s: devGpibLib_initLo: invalid command type for an LO record in param %d\n", plo->name, ((struct gpibDpvt *)plo->dpvt)->parm);
      errMessage(S_db_badField, message);
      plo->pact = TRUE;
      return(S_db_badField);
  }

  return(0);
}

/******************************************************************************
 *
 * bi record init
 *
 ******************************************************************************/

long 
devGpibLib_initBi(pbi, process)
struct biRecord	*pbi;
void (*process)();
{
  long 		result;
  char		message[100];
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(((gDset *)(pbi->dset))->funPtr[pbi->dset->number]);

  /* Do common initialization */
  if (result = devGpibLib_initXx((caddr_t) pbi, &pbi->inp))
  {
    return(result);
  }
  /* make sure the command type makes sendse for the record type */

  if (parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].type != GPIBREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].type != GPIBEFASTI &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].type != GPIBEFASTIW &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].type != GPIBRAWREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].type != GPIBREADW)
  {
    sprintf(message, "%s: devGpibLib_initLi: invalid command type for an BI record in param %d\n", pbi->name, ((struct gpibDpvt *)pbi->dpvt)->parm);
    errMessage(S_db_badField, message);
    pbi->pact = TRUE;
    return(S_db_badField);
  }

  /* See if there are names asociated with the record that should */
  /* be filled in */
  if (parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].namelist != NULL)
  {
    if (pbi->znam[0] == '\0')
    {
        strcpy(pbi->znam, parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].namelist->item[0]);
    }
    if (pbi->onam[0] == '\0')
    {
      strcpy(pbi->onam, parmBlock->gpibCmds[((struct gpibDpvt *)pbi->dpvt)->parm].namelist->item[1]);
    }
  }
  return(0);
}

/******************************************************************************
 *
 * bo record init
 *
 ******************************************************************************/

long 
devGpibLib_initBo(pbo, process)
struct boRecord	*pbo;
void 		(*process)();
{
  long 	result;
  char	message[100];
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(((gDset *)(pbo->dset))->funPtr[pbo->dset->number]);

  /* do common initialization */
  if (result = devGpibLib_initXx((caddr_t) pbo, &pbo->out))
  {
      return(result);
  }
  if (parmBlock->gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].type != GPIBWRITE &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].type != GPIBCMD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].type != GPIBEFASTO &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].type != GPIBCNTL)
  {
      sprintf(message, "%s: devGpibLib_initBo: invalid command type for an BO record in param %d\n", pbo->name, ((struct gpibDpvt *)pbo->dpvt)->parm);
      errMessage(S_db_badField, message);
      pbo->pact = TRUE;
      return(S_db_badField);
  }
  /* see if there are names asociated with the record that should */
  /* be filled in */
  if (parmBlock->gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].namelist != NULL)
  {
      if (pbo->znam[0] == '\0')
      {
          strcpy(pbo->znam, parmBlock->gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].namelist->item[0]);
      }
      if (pbo->onam[0] == '\0')
      {
          strcpy(pbo->onam, parmBlock->gpibCmds[((struct gpibDpvt *)pbo->dpvt)->parm].namelist->item[1]);
      }
  }
  return(0);
}

/******************************************************************************
 *
 * mbbi record init
 *
 ******************************************************************************/

long 
devGpibLib_initMbbi(pmbbi, process)
struct mbbiRecord	*pmbbi;
void 			(*process)();
{
    long 	result;
    char	message[100];
    struct gpibDpvt *dpvt;	/* pointer to gpibDpvt, not yet assigned */
    int         name_ct;        /* for filling in the name strings */
    char        *name_ptr;      /* index into name list array */
    unsigned long *val_ptr;	/* indev into the value list array */
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pmbbi->dset))->funPtr[pmbbi->dset->number]);

    /* do common initialization */

    if (result = devGpibLib_initXx((caddr_t)pmbbi, &pmbbi->inp))
    {
        return(result);
    }
  /* make sure the command type makes sendse for the record type */

  if (parmBlock->gpibCmds[((struct gpibDpvt *)pmbbi->dpvt)->parm].type != GPIBREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbi->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbi->dpvt)->parm].type != GPIBEFASTI &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbi->dpvt)->parm].type != GPIBEFASTIW &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbi->dpvt)->parm].type != GPIBRAWREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbi->dpvt)->parm].type != GPIBREADW)
  {
    sprintf(message, "%s: devGpibLib_initLi: invalid command type for an MBBI record in param %d\n", pmbbi->name, ((struct gpibDpvt *)pmbbi->dpvt)->parm);
    errMessage(S_db_badField, message);
    pmbbi->pact = TRUE;
    return(S_db_badField);
  }


    dpvt = (struct gpibDpvt *)pmbbi->dpvt;  /* init pointer to gpibDpvt */

    /* do initialization of other fields in the record that are unique
     * to this record type */

    /* see if there are names asociated with the record that should */
    /* be filled in */
    if (parmBlock->gpibCmds[dpvt->parm].namelist != NULL)
    {
        if (parmBlock->gpibCmds[dpvt->parm].namelist->value == NULL)
        {
            sprintf(message, "%s: init_rec_mbbi: MBBI value list wrong for param #%d\n", pmbbi->name, dpvt->parm);
            errMessage(S_db_badField, message);
	    pmbbi->pact = TRUE;
            return(S_db_badField);
        }
        pmbbi->nobt = parmBlock->gpibCmds[dpvt->parm].namelist->nobt;
        name_ct = 0;                    /* current name string element */
        name_ptr = pmbbi->zrst;         /* first name string element */
        val_ptr = &(pmbbi->zrvl);	/* first value element */
        while (name_ct < parmBlock->gpibCmds[dpvt->parm].namelist->count)
        {
            if (name_ptr[0] == '\0')
            {
                strcpy(name_ptr, parmBlock->gpibCmds[dpvt->parm].namelist->item[name_ct]);
                *val_ptr = parmBlock->gpibCmds[dpvt->parm].namelist->value[name_ct];
            }
            name_ct++;
            name_ptr += sizeof(pmbbi->zrst);
            val_ptr++;
        }
    }
    return(0);
}

/******************************************************************************
 *
 * mbbo record init
 *
 ******************************************************************************/

long 
devGpibLib_initMbbo(pmbbo, process)
struct mbboRecord	*pmbbo;
void (*process)();
{
    long 	result;
    char	message[100];
    struct gpibDpvt *dpvt;	/* pointer to gpibDpvt, not yet assigned */
    int		name_ct;	/* for filling in the name strings */
    char	*name_ptr;	/* index into name list array */
    unsigned long *val_ptr;       /* index into the value list array */
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pmbbo->dset))->funPtr[pmbbo->dset->number]);

    /* do common initialization */
    if (result = devGpibLib_initXx((caddr_t)pmbbo, &pmbbo->out))
    {
        return(result);
    }
  if (parmBlock->gpibCmds[((struct gpibDpvt *)pmbbo->dpvt)->parm].type != GPIBWRITE &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbo->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbo->dpvt)->parm].type != GPIBCMD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbo->dpvt)->parm].type != GPIBEFASTO &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pmbbo->dpvt)->parm].type != GPIBCNTL)
  {
      sprintf(message, "%s: devGpibLib_initMbbo: invalid command type for an MBBO record in param %d\n", pmbbo->name, ((struct gpibDpvt *)pmbbo->dpvt)->parm);
      errMessage(S_db_badField, message);
      pmbbo->pact = TRUE;
      return(S_db_badField);
  }

    dpvt = (struct gpibDpvt *)pmbbo->dpvt;  /* init pointer to gpibDpvt */

    /* do initialization of other fields in the record that are unique
     * to this record type */

    /* see if there are names asociated with the record that should */
    /* be filled in */
    if (parmBlock->gpibCmds[dpvt->parm].namelist != NULL)
    {
        if (parmBlock->gpibCmds[dpvt->parm].namelist->value == NULL)
        {
            sprintf(message, "%s: init_rec_mbbo: MBBO value list wrong for param #%d\n", pmbbo->name, dpvt->parm);
            errMessage(S_db_badField, message);
	    pmbbo->pact = TRUE;
            return(S_db_badField);
        }

        pmbbo->nobt = parmBlock->gpibCmds[dpvt->parm].namelist->nobt;
        name_ct = 0;			/* current name string element */
        name_ptr = pmbbo->zrst;		/* first name string element */
        val_ptr = &(pmbbo->zrvl);       /* first value element */
	while (name_ct < parmBlock->gpibCmds[dpvt->parm].namelist->count)
        {
            if (name_ptr[0] == '\0')
            {
                strcpy(name_ptr, parmBlock->gpibCmds[dpvt->parm].namelist->item[name_ct]);
                *val_ptr = parmBlock->gpibCmds[dpvt->parm].namelist->value[name_ct];
            }
            name_ct++;
            name_ptr += sizeof(pmbbo->zrst);
            val_ptr++;
        }
    }
    return(2);
}

/******************************************************************************
 *
 * stringin record init
 *
 ******************************************************************************/

long 
devGpibLib_initSi(psi, process)
struct stringinRecord	*psi;
void 			(*process)();
{
  long 	result;
  struct gpibDpvt *dpvt;	/* pointer to gpibDpvt, not yet assigned */
  char		message[100];
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(((gDset*)(psi->dset))->funPtr[psi->dset->number]);

  /* do common initialization */
  if (result = devGpibLib_initXx((caddr_t) psi, &psi->inp))
  {
    return(result);
  }
  /* make sure the command type makes sendse for the record type */

  if (parmBlock->gpibCmds[((struct gpibDpvt *)psi->dpvt)->parm].type != GPIBREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)psi->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)psi->dpvt)->parm].type != GPIBRAWREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)psi->dpvt)->parm].type != GPIBREADW)
  {
    sprintf(message, "%s: devGpibLib_initLi: invalid command type for an SI record in param %d\n", psi->name, ((struct gpibDpvt *)psi->dpvt)->parm);
    errMessage(S_db_badField, message);
    psi->pact = TRUE;
    return(S_db_badField);
  }
  return(0);
}

/******************************************************************************
 *
 * stringout record init
 *
 ******************************************************************************/

long 
devGpibLib_initSo(pso, process)
struct stringoutRecord	*pso;
void 			(*process)();
{
  long		result;
  struct gpibDpvt *dpvt;	/* pointer to gpibDpvt, not yet assigned */
  char		message[100];
  struct devGpibParmBlock *parmBlock;

  parmBlock = (struct devGpibParmBlock *)(((gDset*)(pso->dset))->funPtr[pso->dset->number]);

  /* do common initialization */
  if (result = devGpibLib_initXx((caddr_t) pso, &pso->out))
  {
      return(result);
  }
  /* do initialization of other fields in the record that are unique
   * to this record type */

  if (parmBlock->gpibCmds[((struct gpibDpvt *)pso->dpvt)->parm].type != GPIBWRITE &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pso->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pso->dpvt)->parm].type != GPIBCMD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pso->dpvt)->parm].type != GPIBCNTL)
  {
      sprintf(message, "%s: devGpibLib_initSo: invalid command typefor a SO record in param %d\n", pso->name, ((struct gpibDpvt *)pso->dpvt)->parm);
      errMessage(S_db_badField, message);
      pso->pact = TRUE;
      return(S_db_badField);
  }

  return(0);
}

/******************************************************************************
 *
 * This init routine is common to all record types
 *
 ******************************************************************************/
long 
devGpibLib_initXx(prec, plink)
struct dbCommon	*prec;
struct link	*plink;
{
    struct devGpibParmBlock *parmBlock;
    struct gpibDpvt	*pdpvt;
    struct dbCommon	*pdbCommon = (struct dbCommon *)prec;
    char 		message[100];
    struct gpibCmd	*pCmd;
    char		foundIt;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(prec->dset))->funPtr[prec->dset->number]);

    if (*parmBlock->debugFlag)
      printf("initXx dealing with record >%s< device >%s<\n", prec->name, parmBlock->name);
    /* allocate space for the private structure */
    pdpvt = (struct gpibDpvt *) malloc(sizeof(struct gpibDpvt));
    prec->dpvt = (void *) pdpvt;

    pdpvt->head.dmaTimeout = parmBlock->dmaTimeout;

    pdpvt->precord = prec;
    pdpvt->parm = -1;     /* In case the sscanf fails */
    pdpvt->linkType = plink->type;

    switch (plink->type) {
    case GPIB_IO:         /* Is a straight Network Instruments link */
	pdpvt->head.link = plink->value.gpibio.link;   /* NI link number */
	pdpvt->head.device = plink->value.gpibio.addr; /* gpib dev address */
	sscanf(plink->value.gpibio.parm, "%hd", &(pdpvt->parm));
	pdpvt->head.bitBusDpvt = NULL;      /* no bitbus data needed */
	break;

    case BBGPIB_IO:       /* Is a bitbus -> gpib link */
	pdpvt->head.device = plink->value.bbgpibio.gpibaddr; /* dev address */
        sscanf(plink->value.bbgpibio.parm, "%hd", &(pdpvt->parm));
	pdpvt->head.bitBusDpvt = (struct dpvtBitBusHead *) malloc(sizeof(struct dpvtBitBusHead));
	pdpvt->head.bitBusDpvt->txMsg.data = (unsigned char *) malloc(BB_MAX_DAT_LEN);
	pdpvt->head.bitBusDpvt->rxMsg.data = (unsigned char *) malloc(BB_MAX_DAT_LEN);
	pdpvt->head.bitBusDpvt->txMsg.node = plink->value.bbgpibio.bbaddr; /* bug node address */
	pdpvt->head.bitBusDpvt->link = plink->value.bbgpibio.link;  /* bug link number */
	pdpvt->head.link = plink->value.bbgpibio.link;
	pdpvt->head.bitBusDpvt->rxMaxLen = sizeof(struct bitBusMsg);
	break;

    default:
        strcpy(message, pdbCommon->name);
	sprintf(message,": init_record : GPIB link type %ld is invalid", plink->type);
	errMessage(S_db_badField, message);
        prec->pact = TRUE;		/* keep record from being processed */
	return(S_db_badField);
	break;
    }
    /* Try to find the hardware private structure */

    if (*parmBlock->debugFlag > 5)
      printf("%s: looking for hwpvt structure for link %d device %d\n",
		 parmBlock->name, pdpvt->head.link, pdpvt->head.device);

    foundIt = 0;
    pdpvt->phwpvt = parmBlock->hwpvtHead;
    while ((pdpvt->phwpvt != NULL) && !foundIt)
    {
      if (*parmBlock->debugFlag > 5)
	printf("%s: Checking hwpvt 0x%08.8X, type %d, link %d, device %d\n",
	    parmBlock->name, pdpvt->phwpvt, pdpvt->phwpvt->linkType, 
	    pdpvt->phwpvt->link, pdpvt->phwpvt->device);

      if (pdpvt->phwpvt->linkType == plink->type && 
	  pdpvt->phwpvt->link == pdpvt->head.link && 
	  pdpvt->phwpvt->device == pdpvt->head.device)
        if (plink->type == BBGPIB_IO)
	{
	  if (pdpvt->phwpvt->bug == pdpvt->head.bitBusDpvt->txMsg.node)
	    foundIt = 1;
          else
	    pdpvt->phwpvt = pdpvt->phwpvt->next;
	}
	else
	  foundIt = 1;
      else
	pdpvt->phwpvt = pdpvt->phwpvt->next;	/* check the next one */
    }
    if (!foundIt)
    { /* I couldn't find it.  Allocate a new one */

      if (*parmBlock->debugFlag)
	printf("%s: allocating a hwpvt structure for link %d device %d\n",
		 parmBlock->name, pdpvt->head.link, pdpvt->head.device);

      pdpvt->phwpvt = (struct hwpvt *) malloc(sizeof (struct hwpvt));
      pdpvt->phwpvt->next = parmBlock->hwpvtHead;	/* put in the list */
      parmBlock->hwpvtHead = pdpvt->phwpvt;

      pdpvt->phwpvt->linkType = plink->type;
      pdpvt->phwpvt->link = pdpvt->head.link;
      pdpvt->phwpvt->device = pdpvt->head.device;

      if (pdpvt->phwpvt->linkType == BBGPIB_IO)
        pdpvt->phwpvt->bug = pdpvt->head.bitBusDpvt->txMsg.node;

      pdpvt->phwpvt->tmoVal = 0;
      pdpvt->phwpvt->tmoCount = 0;
      pdpvt->phwpvt->srqCallback = NULL;
      pdpvt->phwpvt->unsolicitedDpvt = NULL;
      pdpvt->phwpvt->parm = (caddr_t)NULL;

      if (*parmBlock->debugFlag)
        printf("issuing IBGENLINK ioctl for link %d\n", pdpvt->phwpvt->link);

      /* Tell the driver we are going to use this link some time */
      (*(drvGpib.ioctl))(pdpvt->phwpvt->linkType, pdpvt->phwpvt->link, pdpvt->phwpvt->bug, IBGENLINK, -1, NULL);
    }
    /* Fill in the dpvt->ibLink field (The driver uses it) */

    if (*parmBlock->debugFlag)
      printf("initXx doing an ioctl-IBGETLINK for record >%s< device >%s<\n", prec->name, parmBlock->name);
    
    (*(drvGpib.ioctl))(pdpvt->phwpvt->linkType, pdpvt->phwpvt->link, pdpvt->phwpvt->bug, IBGETLINK, -1, &(pdpvt->head.pibLink));

    if (*parmBlock->debugFlag)
      printf("ioctl-IBGETLINK returned 0x%08.8X\n", pdpvt->head.pibLink);

    if (*parmBlock->debugFlag > 5)
      printf("initXx checking GPIB address for record >%s< device >%s<\n", prec->name, parmBlock->name);

    /* Check for valid GPIB address */
    if ((pdpvt->head.device < 0) || (pdpvt->head.device >= IBAPERLINK))
    {
        strcpy(message, pdbCommon->name);
        strcat(message,": init_record : GPIB address out of range");
        errMessage(S_db_badField,message);
	prec->pact = TRUE;		/* keep record from being processed */
        return(S_db_badField);
    }
  
    if (*parmBlock->debugFlag > 5)
      printf("initXx checking param entry for record >%s< device >%s<\n", prec->name, parmBlock->name);

    /* Check for valid param entry */
    if ((pdpvt->parm < 0) || (pdpvt->parm > parmBlock->numparams))
    {
        strcpy(message, pdbCommon->name);
        strcat(message,": init_record : GPIB parameter # out of range");
        errMessage(S_db_badField,message);
	prec->pact = TRUE;		/* keep record from being processed */
        return(S_db_badField);
    }

    if (*parmBlock->debugFlag > 5)
      printf("initXx checking record coherency for record >%s< device >%s<\n", prec->name, parmBlock->name);

    /* make sure that the record type matches the GPIB port type (jrw) */
    if (parmBlock->gpibCmds[pdpvt->parm].rec_typ != (gDset *)prec->dset )
    {
	sprintf(message, "%s: init_record: record type invalid for spec'd GPIB param #%d", pdbCommon->name, pdpvt->parm);
        errMessage(S_db_badField,message);
	prec->pact = TRUE;		/* keep record from being processed */
        return(S_db_badField);
    }
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);
    if(pCmd->msgLen > 0)
	pdpvt->msg = (char *)(malloc(pCmd->msgLen));
    if(pCmd->rspLen > 0)
	pdpvt->rsp = (char *)(malloc(pCmd->rspLen));

    if(parmBlock->srqHandler != NULL)
    {
      /* If user spec'd the process on unsolicited SRQ parameter mark it */
      if (pdpvt->parm == parmBlock->magicSrq)
      {
        if (pdpvt->phwpvt->unsolicitedDpvt != NULL)
        {
	  strcpy(message, pdbCommon->name);
	  strcat(message,": init_record: can only have 1 SRQ scanned record per GPIB device");
	  errMessage(S_db_badField,message);
	  prec->pact = TRUE;		/* keep record from being processed */
	  return(S_db_badField);
        }
        if (pdbCommon->scan == SCAN_IO_EVENT)
        {
          pdpvt->phwpvt->unsolicitedDpvt = pdpvt;
        }
      }

     /*
      * Ok to re-register a handler for the same device.
      * Just don't do it after init time is over!
      */
      (*(drvGpib.registerSrqCallback))(pdpvt->head.pibLink, pdpvt->head.device, parmBlock->srqHandler, pdpvt->phwpvt);
    }

    /* fill in the work routine pointer */
    pdpvt->head.workStart = (int (*)()) (((gDset*)(prec->dset))->funPtr[(((gDset*)(prec->dset))->number) + 1]);
    return(0);
}

/******************************************************************************
 *
 * These are the functions that are called to actually communicate with
 * the GPIB device.
 *
 * They are called by record-processing to perform an I/O operation.  In the
 * GPIB case, all I/O is done via anynchronous processing, so all these
 * functions do is queue requests for the driver to take care of when it is
 * ready.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * read_ai()
 *
 ******************************************************************************/

long 
devGpibLib_readAi(pai)
struct aiRecord	*pai;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pai->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pai->dset))->funPtr[pai->dset->number]);

    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]); 
    if (pai->pact)
    {
        return(2);	/* work is all done, return '2' to indicate val */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {	/* put pointer to dpvt field on transaction fifo */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
        {
            devGpibLib_setPvSevr(pai,MAJOR_ALARM,VALID_ALARM);
	    return(0);
        }
	pai->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * write_ao()
 *
 ******************************************************************************/

long
devGpibLib_writeAo(pao)
struct aoRecord	*pao;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pao->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pao->dset))->funPtr[pao->dset->number]);

    pCmd = &parmBlock->gpibCmds[pdpvt->parm]; 

    if (pao->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {		/* put pointer to dvpt field on transaction fifo */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
        {
	    devGpibLib_setPvSevr(pao,WRITE_ALARM,VALID_ALARM);
	    return(0);
        }
	pao->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * read_li()
 *
 ******************************************************************************/

long
devGpibLib_readLi(pli)
struct longinRecord *pli;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pli->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pli->dset))->funPtr[pli->dset->number]);

   pCmd = &parmBlock->gpibCmds[pdpvt->parm];
   if (pli->pact)
   {
        return(2);      /* work is all done, return '2' to indicate val */
    }
    else if (pCmd->type == GPIBSOFT)
    {
         return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {   /* put pointer to dpvt field on transaction fifo */
        if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
        {
            devGpibLib_setPvSevr(pli,MAJOR_ALARM,VALID_ALARM);
            return(0);
        }
	pli->pact = TRUE;
        return(0);
    }
}

/******************************************************************************
 *
 * write_lo()
 *
 ******************************************************************************/

long
devGpibLib_writeLo(plo)
struct longoutRecord *plo;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(plo->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(plo->dset))->funPtr[plo->dset->number]);

    pCmd = &parmBlock->gpibCmds[pdpvt->parm];

    if (plo->pact)
    {
        return(0);      /* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
         return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {           /* put pointer to dvpt field on transaction fifo */
        if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
        {
            devGpibLib_setPvSevr(plo,WRITE_ALARM,VALID_ALARM);
            return(0);
        }
	plo->pact = TRUE;
        return(0);
    }
}

/******************************************************************************
 *
 * read_bi()
 *
 ******************************************************************************/

long 
devGpibLib_readBi(pbi)
struct biRecord	*pbi;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pbi->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pbi->dset))->funPtr[pbi->dset->number]);

    pCmd = &parmBlock->gpibCmds[pdpvt->parm];
 
    if (pbi->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {	/* put pointer to dvpt field on transaction fifo */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
        {
	    devGpibLib_setPvSevr(pbi,READ_ALARM,VALID_ALARM);
	    return(0);
        }
	pbi->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * write_bo()
 *
 ******************************************************************************/

long 
devGpibLib_writeBo(pbo)
struct boRecord	*pbo;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pbo->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pbo->dset))->funPtr[pbo->dset->number]);

    if (*parmBlock->debugFlag)
      printf("devGpibLib_writeBo() entered\n");

    pCmd = &parmBlock->gpibCmds[pdpvt->parm];
 
    if (pbo->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {	/* put pointer to dvpt field on transaction fifo */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
	{
	    devGpibLib_setPvSevr(pbo,WRITE_ALARM,VALID_ALARM);
	    return(0);
	}
	pbo->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * read_mbbi()
 *
 ******************************************************************************/

long 
devGpibLib_readMbbi(pmbbi)
struct mbbiRecord	*pmbbi;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pmbbi->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pmbbi->dset))->funPtr[pmbbi->dset->number]);

    pCmd = &parmBlock->gpibCmds[pdpvt->parm];

    if (pmbbi->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {	/* put pointer to dvpt field on transaction fifo */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
	{
	    devGpibLib_setPvSevr(pmbbi,READ_ALARM,VALID_ALARM);
	    return(0);
	}
	pmbbi->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * write_mbbo()
 *
 ******************************************************************************/

long 
devGpibLib_writeMbbo(pmbbo)
struct mbboRecord	*pmbbo;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pmbbo->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pmbbo->dset))->funPtr[pmbbo->dset->number]);

    pCmd = &parmBlock->gpibCmds[pdpvt->parm];
 
    if (pmbbo->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {	/* put pointer to dvpt field on transaction fifo */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
	{
	    devGpibLib_setPvSevr(pmbbo,WRITE_ALARM,VALID_ALARM);
	    return(0);
	}
	pmbbo->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * read_stringin()
 *
 ******************************************************************************/

long 
devGpibLib_readSi(psi)
struct stringinRecord	*psi;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(psi->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(psi->dset))->funPtr[psi->dset->number]);

    pCmd = &parmBlock->gpibCmds[pdpvt->parm]; 
    if (psi->pact)
    {
        return(2);	/* work is all done, return '2' to indicate val */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {	/* put pointer to dvpt field on transaction fifo */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
	{
	    devGpibLib_setPvSevr(psi,MAJOR_ALARM,VALID_ALARM);
	    return(0);
	}
	psi->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * write_stringout()
 *
 ******************************************************************************/

long 
devGpibLib_writeSo(pso)
struct stringoutRecord	*pso;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pso->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pso->dset))->funPtr[pso->dset->number]);

    pCmd = &parmBlock->gpibCmds[pdpvt->parm]; 

    if (pso->pact)
    {
        return(0);	/* work is all done, finish processing */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {	/* put pointer to dvpt field on transaction fifo */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
	{
	    devGpibLib_setPvSevr(pso,WRITE_ALARM,VALID_ALARM);
	    return(0);
	}
	pso->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * Routines that do the actual GPIB work.  They are called by the linkTask in
 * response to work requests passed in from the read_xx and write_xx functions
 * above.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * devGpibLib_aiGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_aiGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct aiRecord *pai= ((struct aiRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pai->dset))->funPtr[pai->dset->number]);

    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* go send predefined cmd msg and read response into msg[] */

    if(*(parmBlock->debugFlag))
        printf("devGpibLib_aiGpibWork: starting ...command type = %d\n",pCmd->type);

    if (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR) 
    {
	devGpibLib_setPvSevr(pai,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }
    else
    {
        if (pCmd->type != GPIBREADW)
            devGpibLib_aiGpibFinish(pdpvt);	/* If not waiting on SRQ, finish */
	else
	{
            if (*(parmBlock->debugFlag) || ibSrqDebug)
	      printf("%s: marking srq Handler for READW operation\n", parmBlock->name);
            pdpvt->phwpvt->srqCallback = (int (*)())(((gDset*)(pai->dset))->funPtr[pai->dset->number + 2]);
            pdpvt->phwpvt->parm = (caddr_t)pdpvt; /* mark the handler */
	    return(BUSY);		/* indicate device still in use */
	}
    }
    return(IDLE);			/* indicate device is now idle */
}

/******************************************************************************
 *
 * devGpibLib_aiGpibSrq()
 *
 ******************************************************************************/

int 
devGpibLib_aiGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int		srqStatus;
{
    struct aiRecord *pai= ((struct aiRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pai->dset))->funPtr[pai->dset->number]);

    if (*parmBlock->debugFlag || ibSrqDebug)
        printf("devGpibLib_aiGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    pdpvt->phwpvt->srqCallback = NULL;	/* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;

    /* read the response back */
    if (devGpibLib_xxGpibWork(pdpvt, GPIBRAWREAD, -1) == ERROR)
    {
        devGpibLib_setPvSevr(pai,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }

    devGpibLib_aiGpibFinish(pdpvt);	/* and finish the processing */
    return(IDLE);		/* indicate device now idle */
}

/******************************************************************************
 *
 * devGpibLib_aiGpibFinish()
 *
 ******************************************************************************/

int
devGpibLib_aiGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    double	value;
    struct aiRecord *pai = ((struct aiRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;
    struct gpibCmd      *pCmd;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pai->dset))->funPtr[pai->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    if (pCmd->convert != NULL)
    {
        if(*parmBlock->debugFlag)
            printf("devGpibLib_aiGpibWork: calling convert ...\n");

        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else  /* interpret msg with predefined format and write into .val */
    {
        /* scan response string, return value will be 1 if successful */
        if(sscanf(pdpvt->msg,pCmd->format,&value))
        {
            pai->val = value;
	    pai->udf = FALSE;
        }
        else        /* sscanf did not find or assign the parameter */
        {
            devGpibLib_setPvSevr(pai,READ_ALARM,VALID_ALARM);
        }
    }
    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);

    return(0);
}

/******************************************************************************
 *
 * devGpibLib_aoGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_aoGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int	cnvrtStat = OK;
    struct aoRecord *pao= ((struct aoRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;
    struct gpibCmd      *pCmd;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pao->dset))->funPtr[pao->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt, pCmd->P1,pCmd->P2, pCmd->P3);   
    }
    else	/* generate msg using predefined format and current val */
    {
        if (pCmd->type == GPIBWRITE)    /* only if needs formatting */
            sprintf(pdpvt->msg, pCmd->format, pao->val);
    }

    /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR)) 
    {
	devGpibLib_setPvSevr(pao,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);		/* jrw */
    return(IDLE);
}

/******************************************************************************
 *
 * devGpibLib_liGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_liGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct longinRecord *pli= ((struct longinRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;
    struct gpibCmd      *pCmd;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pli->dset))->funPtr[pli->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* go send predefined cmd msg and read response into msg[] */

    if(*parmBlock->debugFlag)
        printf("devGpibLib_liGpibWork: starting ...\n");

    if (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR) 
    {
	devGpibLib_setPvSevr(pli,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }
    else
    {
        if (pCmd->type != GPIBREADW)
            devGpibLib_liGpibFinish(pdpvt);	/* If not waiting on SRQ, finish */
	else
	{
            pdpvt->phwpvt->srqCallback = (int(*)())(((gDset*)(pli->dset))->funPtr[pli->dset->number + 2]);
            pdpvt->phwpvt->parm = (caddr_t)pdpvt; /* mark the handler */
	    return(BUSY);		/* indicate device still in use */
	}
    }
    return(IDLE);			/* indicate device is now idle */
}

/******************************************************************************
 *
 * devGpibLib_liGpibSrq()
 *
 ******************************************************************************/

int 
devGpibLib_liGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int		srqStatus;
{
    struct longinRecord *pli= ((struct longinRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pli->dset))->funPtr[pli->dset->number]);

    if (*parmBlock->debugFlag || ibSrqDebug)
        printf("devGpibLib_liGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    pdpvt->phwpvt->srqCallback = NULL;	/* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;

    /* do actual SRQ processing in here */

    /* read the response back */
    if (devGpibLib_xxGpibWork(pdpvt, GPIBRAWREAD, -1) == ERROR)
    {
        devGpibLib_setPvSevr(pli,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }

    devGpibLib_liGpibFinish(pdpvt);	/* and finish the processing */
    return(IDLE);		/* indicate device now idle */
}

/******************************************************************************
 *
 * devGpibLib_liGpibFinish()
 *
 ******************************************************************************/

int
devGpibLib_liGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    unsigned long	value;
    struct longinRecord *pli = ((struct longinRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pli->dset))->funPtr[pli->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    if (pCmd->convert != NULL)
    {
        if(*parmBlock->debugFlag)
            printf("devGpibLib_liGpibWork: calling convert ...\n");
        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else  /* interpret msg with predefined format and write into .val */
    {
        /* scan response string, return value will be 1 if successful */
        if(sscanf(pdpvt->msg,pCmd->format,&value))
        {
            pli->val = value;
	    pli->udf = FALSE;
        }
        else        /* sscanf did not find or assign the parameter */
        {
            devGpibLib_setPvSevr(pli,READ_ALARM,VALID_ALARM);
        }
    }
    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);

    return(0);
}

/******************************************************************************
 *
 * devGpibLib_loGpibWork()
 *
 ******************************************************************************/

int
devGpibLib_loGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int cnvrtStat = OK;
    struct longoutRecord *plo= ((struct longoutRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(plo->dset))->funPtr[plo->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL)
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt, pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else        /* generate msg using predefined format and current val */
    {
        if (pCmd->type == GPIBWRITE)    /* only if needs formatting */
            sprintf(pdpvt->msg, pCmd->format, plo->val);
    }

    /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR))
    {
        devGpibLib_setPvSevr(plo,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);             /* jrw */
    return(IDLE);
}

/******************************************************************************
 *
 * devGpibLib_biGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_biGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct biRecord *pbi= ((struct biRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pbi->dset))->funPtr[pbi->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* go send predefined cmd msg and read response into msg[] */

    if (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR) 
    {
	devGpibLib_setPvSevr(pbi,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }
    else   	/* interpret response that came back */   
    {
        if (pCmd->type != GPIBREADW && pCmd->type != GPIBEFASTIW)
            devGpibLib_biGpibFinish(pdpvt);        /* If not waiting on SRQ, finish */
        else
        {
            pdpvt->phwpvt->srqCallback = (int(*)())(((gDset*)(pbi->dset))->funPtr[pbi->dset->number + 2]);
            pdpvt->phwpvt->parm = (caddr_t)pdpvt; /* mark the handler */
            return(BUSY);               /* indicate device still in use */
        }
    }
    return(IDLE);
}

/******************************************************************************
 *
 * devGpibLib_biGpibSrq()
 *
 ******************************************************************************/

int
devGpibLib_biGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int             srqStatus;
{
    struct biRecord *pbi= ((struct biRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pbi->dset))->funPtr[pbi->dset->number]);

    if (*parmBlock->debugFlag || ibSrqDebug)
        printf("devGpibLib_biGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    pdpvt->phwpvt->srqCallback = NULL;   /* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;   /* unmark the handler */

    /* read the response back */
    if (devGpibLib_xxGpibWork(pdpvt, GPIBRAWREAD, -1) == ERROR)
    {
        devGpibLib_setPvSevr(pbi,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }

    devGpibLib_biGpibFinish(pdpvt);        /* and finish the processing */
    return(IDLE);               /* indicate device now idle */
}

/******************************************************************************
 *
 * devGpibLib_biGpibFinish()
 *
 ******************************************************************************/

int
devGpibLib_biGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    unsigned long	value;
    int			status;
    struct biRecord *pbi= ((struct biRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pbi->dset))->funPtr[pbi->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    if (pCmd->convert != NULL)
    {
        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else  /* interpret msg with predefined format and write into .rval */
    {
	if (pCmd->type == GPIBEFASTI || pCmd->type == GPIBEFASTIW)
        {    /* Check the response against enumerations */
	    if ((status = checkEnums(pdpvt->msg, pCmd->P3)) >= 0)
		pbi->rval = status;
	    else
		devGpibLib_setPvSevr(pbi,READ_ALARM,VALID_ALARM);
        }
	else
	{   /* Scan response string, return value will be 1 if successful */
            if(sscanf(pdpvt->msg,pCmd->format, &value))
		pbi->rval = value;
            else        /* sscanf did not find or assign the parameter */
		devGpibLib_setPvSevr(pbi,READ_ALARM,VALID_ALARM);
	}
    }
    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);             /* jrw */

    return(0);
}

/******************************************************************************
 *
 * devGpibLib_boGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_boGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int	cnvrtStat = OK;
    int	strStat = OK;
    struct boRecord *pbo= ((struct boRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pbo->dset))->funPtr[pbo->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);   
    }
    else	/* generate msg using predefined format and current val */
    {
        if (pCmd->type == GPIBWRITE)    /* only if needs formatting */
            sprintf(pdpvt->msg, pCmd->format, (unsigned int)pbo->val);
    }

    /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (devGpibLib_xxGpibWork(pdpvt, pCmd->type, pbo->val? 1 : 0) == ERROR)) 
    {
	devGpibLib_setPvSevr(pbo,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);
    return(IDLE);
}

/******************************************************************************
 *
 * devGpibLib_mbbiGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_mbbiGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct mbbiRecord *pmbbi= ((struct mbbiRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pmbbi->dset))->funPtr[pmbbi->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);
    
    /* go send predefined cmd msg and read string response into msg[] */

    if (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR) 
    {
	devGpibLib_setPvSevr(pmbbi,WRITE_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }
    else 
    {
        if (pCmd->type != GPIBREADW)
            devGpibLib_mbbiGpibFinish(pdpvt);        /* If not waiting on SRQ, finish */
        else
        {
            pdpvt->phwpvt->srqCallback = (int(*)())(((gDset*)(pmbbi->dset))->funPtr[pmbbi->dset->number + 2]);
            pdpvt->phwpvt->parm = (caddr_t)pdpvt;     /* mark the handler */
            return(BUSY);               /* indicate device still in use */
        }
    } 
    return(IDLE);
}

/******************************************************************************
 *
 * devGpibLib_mbbiGpibSrq()
 *
 ******************************************************************************/

int
devGpibLib_mbbiGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int             srqStatus;
{
    struct mbbiRecord *pmbbi= ((struct mbbiRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pmbbi->dset))->funPtr[pmbbi->dset->number]);

    if (*parmBlock->debugFlag || ibSrqDebug)
        printf("devGpibLib_mbbiGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    /* do actual SRQ processing in here */

    pdpvt->phwpvt->srqCallback = NULL;   /* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;   /* unmark the handler */

    /* read the response back */
    if (devGpibLib_xxGpibWork(pdpvt, GPIBRAWREAD, -1) == ERROR)
    {
        devGpibLib_setPvSevr(pmbbi,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }

    devGpibLib_mbbiGpibFinish(pdpvt);        /* and finish the processing */
    return(IDLE);               /* indicate device now idle */
}

/******************************************************************************
 *
 * devGpibLib_mbbiGpibFinish()
 *
 ******************************************************************************/

int
devGpibLib_mbbiGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    unsigned long  value;
    int		status;
    struct mbbiRecord *pmbbi= ((struct mbbiRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pmbbi->dset))->funPtr[pmbbi->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    if (pCmd->convert != NULL)
    {
        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else  /* interpret msg with predefined format and write into .rval */
    {
        if (pCmd->type == GPIBEFASTI || pCmd->type == GPIBEFASTIW)
        {    /* Check the response against enumerations */
            if ((status = checkEnums(pdpvt->msg, pCmd->P3)) >= 0)
                pmbbi->rval = status;
            else
                devGpibLib_setPvSevr(pmbbi,READ_ALARM,VALID_ALARM);
        }
        else
        {   /* Scan response string, return value will be 1 if successful */
            if(sscanf(pdpvt->msg, pCmd->format, &value))
		pmbbi->rval = value;
            else        /* sscanf did not find or assign the parameter */
		devGpibLib_setPvSevr(pmbbi,READ_ALARM,VALID_ALARM);
	}
    }
    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);

    return(0);
}

/******************************************************************************
 *
 * devGpibLib_mbboGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_mbboGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int	cnvrtStat = OK;
    int	strStat = OK;
    struct mbboRecord *pmbbo= ((struct mbboRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pmbbo->dset))->funPtr[pmbbo->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);   
    }
    else	/* generate msg using predefined format and current val */
    {
        if (pCmd->type == GPIBWRITE)	/* only if needs formatting */
           sprintf(pdpvt->msg, pCmd->format, (unsigned int)pmbbo->rval);
    }

    /* go access board with this message, unless convert was unsuccessful */
    /* NOTE the use of val instead of rval for the EFASTO operation index! */
    if ((cnvrtStat == ERROR) || (devGpibLib_xxGpibWork(pdpvt, pCmd->type, pmbbo->val) == ERROR))
    {
	devGpibLib_setPvSevr(pmbbo,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);		/* jrw */
    return(IDLE);
}

/******************************************************************************
 *
 * devGpibLib_stringinGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_stringinGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct stringinRecord *psi=((struct stringinRecord*)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(psi->dset))->funPtr[psi->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* go send predefined cmd msg and read response into msg[] */

    if(*parmBlock->debugFlag)
        printf("devGpibLib_stringinGpibWork: starting ...\n");

    if (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR) 
    {
	devGpibLib_setPvSevr(psi,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);             /* jrw */
    }
    else 
    {
        if (pCmd->type != GPIBREADW)
            devGpibLib_stringinGpibFinish(pdpvt);  /* If not waiting on SRQ, finish */
        else
        {
            pdpvt->phwpvt->srqCallback = (int(*)())(((gDset*)(psi->dset))->funPtr[psi->dset->number + 2]);
            pdpvt->phwpvt->parm = (caddr_t)pdpvt; /* mark the handler */
            return(BUSY);               /* indicate device still in use */
        }
    }    
    return(IDLE);
}

/******************************************************************************
 *
 * devGpibLib_stringinGpibSrq()
 *
 ******************************************************************************/

int
devGpibLib_stringinGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int             srqStatus;
{
    struct stringinRecord *psi= ((struct stringinRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(psi->dset))->funPtr[psi->dset->number]);

    if (*parmBlock->debugFlag || ibSrqDebug)
        printf("devGpibLib_stringinGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    /* do actual SRQ processing in here */

    pdpvt->phwpvt->srqCallback = NULL;   /* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;   /* unmark the handler */

    /* read the response back */
    if (devGpibLib_xxGpibWork(pdpvt, GPIBRAWREAD, -1) == ERROR)
    {
        devGpibLib_setPvSevr(psi,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest(pdpvt);
    }

    devGpibLib_stringinGpibFinish(pdpvt);        /* and finish the processing */
    return(IDLE);               /* indicate device now idle */
}

/******************************************************************************
 *
 * devGpibLib_stringinGpibFinish()
 *
 ******************************************************************************/

int
devGpibLib_stringinGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct stringinRecord *psi=((struct stringinRecord*)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(psi->dset))->funPtr[psi->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    if (pCmd->convert != NULL)
    {
        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else
    {	/* BUG -- why can't we just read it into the val field? */
	/* BUG -- length should not be hard coded here */

        strncpy(psi->val,pdpvt->msg,39);
        psi->val[40] = '\0';
	psi->udf = FALSE;
    }
    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);             /* jrw */

    return(0);
}

/******************************************************************************
 *
 * devGpibLib_stringoutGpibWork()
 *
 ******************************************************************************/

int
devGpibLib_stringoutGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    int	cnvrtStat = OK;
    struct stringoutRecord *pso= ((struct stringoutRecord *)(pdpvt->precord));
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pso->dset))->funPtr[pso->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* generate command string to be sent */

    /* call convert routine, if defined */
    if (pCmd->convert != NULL) 
    {
        cnvrtStat = (*(pCmd->convert))(pdpvt, pCmd->P1, pCmd->P2, pCmd->P3);   
    }
    else
    {   /* BUG -- why can't we just write it from the val field? */
        strncpy(pdpvt->msg, pso->val, 40);
    }

    /* go access board with this message, unless convert was unsuccessful */
    if ((cnvrtStat == ERROR) || (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR)) 
    {
	devGpibLib_setPvSevr(pso,WRITE_ALARM,VALID_ALARM);
    }

    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest(pdpvt);		/* jrw */
    return(IDLE);
}

/******************************************************************************
 *
 * devGpibLib_xxGpibWork()
 *
 * Generic function used to read and write strings to the GPIB instruments
 * by calling the device driver.
 *
 ******************************************************************************/

int 
devGpibLib_xxGpibWork(pdpvt, cmdType, val)
struct gpibDpvt *pdpvt;
int	cmdType;
unsigned short	val;	/* used for EFAST operations only */
{
    int status = OK;
    short ibnode = pdpvt->head.device;
    short bbnode = -1;	/* In case is a bitbus->gpib type link */
    struct gpibCmd      *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pdpvt->precord->dset))->funPtr[pdpvt->precord->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* If is a BBGPIB_IO link, the bug address is needed */
    if (pdpvt->linkType == BBGPIB_IO)
        bbnode = pdpvt->head.bitBusDpvt->txMsg.node;

    /*
     * check to see if this node has timed out within last 10 sec
     */
    if(tickGet() < (pdpvt->phwpvt->tmoVal + parmBlock->timeWindow) )
    {
        if (*parmBlock->debugFlag)
            printf("devGpibLib_xxGpibWork(): timeout flush\n");

        return(ERROR);
    }
    switch (cmdType) {
    case GPIBWRITE:		/* write the message to the GPIB listen adrs */

        status = (*(drvGpib.writeIb))(pdpvt->head.pibLink, ibnode, pdpvt->msg, 
				strlen(pdpvt->msg), pdpvt->head.dmaTimeout);


	if ((status != ERROR) && (parmBlock->respond2Writes) != -1)
	{   /* device responds to write commands, read the response */
	    if (parmBlock->respond2Writes > 0)
	    {
	      taskDelay(parmBlock->respond2Writes);
	    }

	    status = (*(drvGpib.readIb))(pdpvt->head.pibLink, ibnode, pdpvt->rsp, 
				pCmd->rspLen, pdpvt->head.dmaTimeout);

            /* if user specified a secondary convert routine, call it */

	    if (parmBlock->wrConversion != NULL)
		status = (*(parmBlock->wrConversion))(status, pdpvt);
	}
        break;

    case GPIBREAD:		/* write the command string */
    case GPIBEFASTI:

        status = (*(drvGpib.writeIb))(pdpvt->head.pibLink, ibnode, pCmd->cmd, 
				strlen(pCmd->cmd), pdpvt->head.dmaTimeout);
        if (status == ERROR)
        {
	    break;
        }

        /* This is probably not the best way to do this... */
        /* because the read turnaround time delay should be independant of */
        /* the responds to writes flag. */

        if (parmBlock->respond2Writes > 0)
        {
            taskDelay(parmBlock->respond2Writes);
        }

	/* NOTICE -- This falls thru to the raw read code below! */

    case GPIBRAWREAD:   /* for SRQs, read the data w/o a sending a command */

	/* read the instrument  */
	status = (*(drvGpib.readIb))(pdpvt->head.pibLink, ibnode, pdpvt->msg, 
				pCmd->msgLen, pdpvt->head.dmaTimeout);
	if (status == ERROR)
	{
	    break;
	}
 	else if (status >( (pCmd->msgLen) - 1) ) /* check length of resp */
	{   /* This may or may not be an error */

	    printf("GPIB Response length equaled allocated space !!!\n");
	    pdpvt->msg[(pCmd->msgLen)-1] = '\0';    /* place \0 at end */
	}
	else
	{
 	    pdpvt->msg[status] = '\0'; /* terminate response with \0 */
	}
        break;

    case GPIBREADW:		/* for SRQs, write the command first */
    case GPIBEFASTIW:
        status = (*(drvGpib.writeIb))(pdpvt->head.pibLink, ibnode, pCmd->cmd, 
				strlen(pCmd->cmd), pdpvt->head.dmaTimeout);
	break;

    case GPIBCMD:		/* write the cmd to the GPIB listen adrs */
        status = (*(drvGpib.writeIb))(pdpvt->head.pibLink, ibnode, pCmd->cmd, 
				strlen(pCmd->cmd), pdpvt->head.dmaTimeout);

        if ((status != ERROR) && (parmBlock->respond2Writes) != -1)
        {   /* device responds to write commands, read the response */
            if (parmBlock->respond2Writes > 0)
            {
              taskDelay(parmBlock->respond2Writes);
            }


	    status = (*(drvGpib.readIb))(pdpvt->head.pibLink, ibnode, pdpvt->rsp, 
				pCmd->rspLen, pdpvt->head.dmaTimeout);

            /* if user specified a secondary convert routine, call it */

	    if (parmBlock->wrConversion != NULL)
		status = (*(parmBlock->wrConversion))(status, pdpvt);
	}
	break;
    case GPIBCNTL:		/* send cmd with atn line active */
        status = (*(drvGpib.writeIbCmd))(pdpvt->head.pibLink, pCmd->cmd, 
				strlen(pCmd->cmd));
        break;
    case GPIBEFASTO:		/* write the enumerated cmd from the P3 array */
        if (pCmd->P3[val] != NULL)
	{
	    status = (*(drvGpib.writeIb))(pdpvt->head.pibLink,ibnode, pCmd->P3[val], 
				strlen(pCmd->P3[val]), pdpvt->head.dmaTimeout);

            if ((status != ERROR) && (parmBlock->respond2Writes) != -1)
            {   /* device responds to write commands, read the response */
                if (parmBlock->respond2Writes > 0)
                {
                  taskDelay(parmBlock->respond2Writes);
                }
    
	        status = (*(drvGpib.readIb))(pdpvt->head.pibLink, ibnode, 
				pdpvt->rsp, pCmd->rspLen, pdpvt->head.dmaTimeout);
    
                /* if user specified a secondary convert routine, call it */
    
	        if (parmBlock->wrConversion != NULL)
		    status = (*(parmBlock->wrConversion))(status, pdpvt);
	    }
	}
        break;
    }
    if(*parmBlock->debugFlag)
        printf("devGpibLib_xxGpibWork : done, status = %d\n",status);

    /* if error occurrs then mark it with time */
    if(status == ERROR)
    {
	(pdpvt->phwpvt->tmoCount)++;		/* count number of timeouts */
        pdpvt->phwpvt->tmoVal = tickGet();	/* set last timeout time */
    }
    return(status);
}

/******************************************************************************
 *
 * This function is called by the callback task.  The callback task
 * calls it after being given the 'go ahead' by callbackRequest()
 * function calls made in the GpibWork routines defined above.
 *
 * The reason it is done this way is because the process() call may
 * recursively call itself when records are chained and the callback
 * task's stack is larger... just for this purpose.
 *
 ******************************************************************************/

void 
devGpibLib_processCallback(pDpvt)
struct  gpibDpvt       *pDpvt;
{
    dbScanLock(pDpvt->precord);
    (*(struct rset *)(pDpvt->precord->rset)).process(pDpvt->precord);
    dbScanUnlock(pDpvt->precord);
}

/******************************************************************************
 *
 * This function is used to set alarm status information.
 *
 ******************************************************************************/
long 
devGpibLib_setPvSevr(pPv, status, severity)
struct dbCommon     *pPv;
short             severity;
short             status;
{
    if (severity > pPv->nsev )
    {
        pPv->nsta = status;
        pPv->nsev = severity;
    }
}

/******************************************************************************
 *
 * This function is used to parse enumerated response strings.
 *
 * Only as many bytes as are found in the enums array elements are compared
 * when searching for a match.  The \0 at the end of the enums array elements
 * is not considered in the comparison.
 *
 ******************************************************************************/
int
checkEnums(msg, enums)
char	*msg;
char	**enums;
{
    int		i;
    int		j;

    i = 0;
    while (enums[i] != NULL)	/* check each enum until match found */
    {				/* will stop on first match */

	j = 0;			/* only check as much as found in enums[i] */
        while (enums[i][j] && enums[i][j] == msg[j])
	    j++;

        if (enums[i][j] == '\0')
	    return(i);		/* found, return index */

	i++;
    }
    return(-1);		/* not found, return error */
}

/******************************************************************************
 *
 * wf record init
 *
 ******************************************************************************/

long 
devGpibLib_initWf(pwf, process)
struct waveformRecord	*pwf;
void		(*process)();
{
  long		result;
  char		message[100];
  struct devGpibParmBlock *parmBlock;
  
  parmBlock = (struct devGpibParmBlock *)(((gDset*)(pwf->dset))->funPtr[pwf->dset->number]);

  /* Do common initialization */
  if (result = devGpibLib_initXx((caddr_t) pwf, &(pwf->inp) ))
  {
    return(result);
  }

  /* make sure the command type makes sendse for the record type */

  if (parmBlock->gpibCmds[((struct gpibDpvt *)pwf->dpvt)->parm].type != GPIBREAD &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pwf->dpvt)->parm].type != GPIBSOFT &&
      parmBlock->gpibCmds[((struct gpibDpvt *)pwf->dpvt)->parm].type != GPIBREADW)
  {
    sprintf(message, "%s: devGpibLib_initWf: invalid command type for an WF record in param %d\n", pwf->name, ((struct gpibDpvt *)pwf->dpvt)->parm);
    errMessage(S_db_badField, message);
    pwf->pact = TRUE;
    return(S_db_badField);
  }
  return(0);
 }

/******************************************************************************
 *
 * These are the functions that are called to actually communicate with
 * the GPIB device.
 *
 * They are called by record-processing to perform an I/O operation.  In the
 * GPIB case, all I/O is done via anynchronous processing, so all these
 * functions do is queue requests for the driver to take care of when it is
 * ready.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * read_wf()
 *
 ******************************************************************************/

long 
devGpibLib_readWf(pwf)
struct waveformRecord	*pwf;
{
    struct gpibDpvt *pdpvt = (struct gpibDpvt *)(pwf->dpvt);
    struct gpibCmd *pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pwf->dset))->funPtr[pwf->dset->number]);

    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]); 
    if (pwf->pact)
    {
        return(2);	/* work is all done, return '2' to indicate val */
    }
    else if (pCmd->type == GPIBSOFT)
    {
	 return((*pCmd->convert)(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3));
    }
    else
    {	/* put pointer to dpvt field on ring buffer */
	if((*(drvGpib.qGpibReq))(pdpvt, pCmd->pri) == ERROR)
        {
            devGpibLib_setPvSevr(pwf,MAJOR_ALARM,VALID_ALARM);
	    return(0);
        }
	pwf->pact = TRUE;
	return(0);
    }
}

/******************************************************************************
 *
 * Routines that do the actual GPIB work.  They are called by the linkTask in
 * response to work requests passed in from the read_xx and write_xx functions
 * above.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * devGpibLib_wfGpibWork()
 *
 ******************************************************************************/

int 
devGpibLib_wfGpibWork(pdpvt)
struct gpibDpvt *pdpvt;
{
    struct waveformRecord *pwf= ((struct waveformRecord *)(pdpvt->precord));
    struct gpibCmd	*pCmd;
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pwf->dset))->funPtr[pwf->dset->number]);

    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    /* go send predefined cmd msg and read response into msg[] */

    if(*(parmBlock->debugFlag))
        printf("devGpibLib_wfGpibWork: starting ...command type = %d\n",pCmd->type);

    if (devGpibLib_xxGpibWork(pdpvt, pCmd->type, -1) == ERROR) 
    {
	devGpibLib_setPvSevr(pwf,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest((void *)pdpvt);
    }
    else
    {
        if (pCmd->type != GPIBREADW)
            devGpibLib_wfGpibFinish(pdpvt);	/* If not waiting on SRQ, finish */
	else
	{
            if (*(parmBlock->debugFlag) || ibSrqDebug)
	      printf("%s: marking srq Handler for READW operation\n", parmBlock->name);
            pdpvt->phwpvt->srqCallback = (int (*)())(((gDset*)(pwf->dset))->funPtr[pwf->dset->number + 2]);
            pdpvt->phwpvt->parm = (caddr_t)pdpvt; /* mark the handler */
	    return(BUSY);		/* indicate device still in use */
	}
    }
    return(IDLE);			/* indicate device is now idle */
}

/******************************************************************************
 *
 * devGpibLib_wfGpibSrq()
 *
 ******************************************************************************/

int 
devGpibLib_wfGpibSrq(pdpvt, srqStatus)
struct gpibDpvt *pdpvt;
int		srqStatus;
{
    struct waveformRecord *pwf= ((struct waveformRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pwf->dset))->funPtr[pwf->dset->number]);

    if (*parmBlock->debugFlag || ibSrqDebug)
        printf("devGpibLib_wfGpibSrq(0x%08.8X, 0x%02.2X): processing srq\n", pdpvt, srqStatus);

    pdpvt->phwpvt->srqCallback = NULL;	/* unmark the handler */
    pdpvt->phwpvt->parm = (caddr_t)NULL;

    /* read the response back */
    if (devGpibLib_xxGpibWork(pdpvt, GPIBRAWREAD, -1) == ERROR)
    {
        devGpibLib_setPvSevr(pwf,READ_ALARM,VALID_ALARM);

        pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
        pdpvt->head.header.callback.priority = priorityLow;
        callbackRequest((void *)pdpvt);
    }

    devGpibLib_wfGpibFinish(pdpvt);	/* and finish the processing */
    return(IDLE);		/* indicate device now idle */
}

/******************************************************************************
 *
 * devGpibLib_wfGpibFinish()
 *
 ******************************************************************************/

int
devGpibLib_wfGpibFinish(pdpvt)
struct gpibDpvt *pdpvt;
{
    double	value;
    struct waveformRecord *pwf = ((struct waveformRecord *)(pdpvt->precord));
    struct devGpibParmBlock *parmBlock;
    struct gpibCmd      *pCmd;

    parmBlock = (struct devGpibParmBlock *)(((gDset*)(pwf->dset))->funPtr[pwf->dset->number]);
    pCmd = &(parmBlock->gpibCmds[pdpvt->parm]);

    if (pCmd->convert != NULL)
    {
        if(*parmBlock->debugFlag)
            printf("devGpibLib_wfGpibWork: calling convert ...\n");

        (*(pCmd->convert))(pdpvt,pCmd->P1,pCmd->P2, pCmd->P3);
    }
    else  /* for waveforms no standard conversion is supplied */
    {
               devGpibLib_setPvSevr(pwf,READ_ALARM,VALID_ALARM);
    }
    pdpvt->head.header.callback.finishProc = devGpibLib_processCallback;
    pdpvt->head.header.callback.priority = priorityLow;
    callbackRequest((void *)pdpvt);

    return(0);
}
