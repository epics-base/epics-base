/* devCommonGpib.h */
/* share/epicsH/devCommonGpib.h  $Id$ */
/*
 *      Author: John Winans
 *      Date:   11-19-91
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
 * .01	11-19-91	jrw	Initial release
 * .02	02-26-92	jrw	removed declaration of callbackRequest()
 * .03	05-11-92	jrw	Added waveform record support
 */

#ifndef	DEVCOMMONGPIB_H
#define DEVCOMMONGPIB_H

long devGpibLib_report();
long devGpibLib_initDevSup();
long devGpibLib_initAi();
long devGpibLib_initAo();
long devGpibLib_initLi();
long devGpibLib_initLo();
long devGpibLib_initBi();
long devGpibLib_initBo();
long devGpibLib_initMbbo();
long devGpibLib_initMbbi();
long devGpibLib_initSi();
long devGpibLib_initSo();
long devGpibLib_initXx();
long devGpibLib_readAi();
long devGpibLib_writeAo();
long devGpibLib_readLi();
long devGpibLib_writeLo();
long devGpibLib_readBi();
long devGpibLib_writeBo();
long devGpibLib_readMbbi();
long devGpibLib_writeMbbo();
long devGpibLib_readSi();
long devGpibLib_writeSo();

int  devGpibLib_aiGpibWork();
int  devGpibLib_aiGpibSrq();
int  devGpibLib_aiGpibFinish();
int  devGpibLib_aoGpibWork();
int  devGpibLib_liGpibWork();
int  devGpibLib_liGpibSrq();
int  devGpibLib_liGpibFinish();
int  devGpibLib_loGpibWork();
int  devGpibLib_biGpibWork();
int  devGpibLib_biGpibSrq();
int  devGpibLib_biGpibFinish();
int  devGpibLib_boGpibWork();
int  devGpibLib_mbbiGpibWork();
int  devGpibLib_mbbiGpibSrq();
int  devGpibLib_mbbiGpibFinish();
int  devGpibLib_mbboGpibWork();
int  devGpibLib_stringinGpibWork();
int  devGpibLib_stringinGpibSrq();
int  devGpibLib_stringinGpibFinish();
int  devGpibLib_stringoutGpibWork();
int  devGpibLib_xxGpibWork();

int  devGpibLib_wfGpibFinish();
int  devGpibLib_wfGpibSrq();
int  devGpibLib_wfGpibWork();
long devGpibLib_readWf();
long devGpibLib_initWf();

void devGpibLib_processCallback();
long devGpibLib_setPvSevr();

typedef struct {
  long          number;
  DEVSUPFUN     funPtr[20];
} gDset;

/******************************************************************************
 *
 * This structure holds device-related data on a per-device basis and is
 * referenced by the gpibDpvt structures.  They are built using a linked
 * list entered from hwpvtHead.  This linked list is only for this specific
 * device type (other gpib devices may have their own lists.)
 *
 * The srqCallback() and parm fields are used for GPIBREADW type calls.  They
 * should place the address of the appropriate function (aiGpibSrq if an AI
 * record is being processed) into the srqCallback() field and the pdpvt
 * field for the device into the parm field.  This allows the SrqHandler()
 * function to locate the proper function and work areas to complete the
 * GPIBREADW operation.
 *
 * The unsolicitedDpvt field is used if the user specifies a record with
 * the parm number associated with handleing unsolicited SRQ interrupts.
 * What is done, is that the record will be processed when an SRQ is
 * detected for reasons other than GPIBREADW-related reasons.  There may
 * be at most, 1 single record in hte database that can specify the 'magic'
 * parm number.  If you need more, forward link it to a fanout record.
 *
 ******************************************************************************/
struct hwpvt {
  struct hwpvt  *next;          /* to next structure for same type device */

  int   linkType;               /* is a GPIB_IO, BBGPIB_IO... from link.h */
  int   link;                   /* link number */
  int   bug;                    /* used only on BBGPIB_IO links */
  int   device;                 /* gpib device number */

  unsigned long tmoVal;         /* time last timeout occurred */
  unsigned long tmoCount;       /* total number of timeouts since boot time */

  /* No semaphore guards here because can inly be mod'd by the linkTask */
  int           (*srqCallback)(); /* filled by cmds expecting SRQ callbacks */
  caddr_t       parm;           /* filled in by cmds expecting SRQ callbacks */

  struct gpibDpvt *unsolicitedDpvt; /* filled in if database calls for it */
  caddr_t	pupvt;		/* user defined pointer */
};

/******************************************************************************
 *
 * This structure will be attached to each pv (via psub->dpvt) to store the
 * appropriate head.workStart pointer, callback address to record support,
 * gpib link #, device address, and command information.
 *
 ******************************************************************************/

struct gpibDpvt {
  struct dpvtGpibHead   head;   /* fields used by the GPIB driver */

  short         parm;           /* parameter index into gpib commands */
  char          *rsp;           /* for read/write message error Responses*/
  char          *msg;           /*  for read/write messages */
  struct dbCommon *precord;     /* record this dpvt is part of */
  void          (*process)();   /* callback to perform forward db processing */
  int           processPri;     /* process callback's priority */
  long          linkType;       /* GPIB_IO, BBGPIB_IO... */
  struct hwpvt  *phwpvt;        /* pointer to per-device private area */
  caddr_t	pupvt;		/* user defined pointer */
};

#define GET_GPIB_HW_PVT(pGpibDpvt)			(((struct gpibDpvt*)(pGpibDpvt))->phwpvt->pupvt)
#define SET_GPIB_HW_PVT(pGpibDpvt, value)	(((struct gpibDpvt*)(pGpibDpvt))->phwpvt->pupvt = value)

/******************************************************************************
 *
 * This is used to define the strings that are used for button labels.
 * These strings are put into the record's znam & onam foelds if the
 * record is a BI or BO type and into the zrst, onst... fields of an
 * MBBI or MBBO record.
 *
 * Before these strings are placed into the record, the record is
 * check to see if there is already a string defined (could be user-entered
 * with DCT.)  If there is already a string present, it will be preserved.
 *
 * There MUST ALWAYS be 2 and only 2 entries in the names.item list
 * for BI and BO records if a name list is being specified for them here.
 * The names.count field is ignored for BI and BO record types, but
 * should be properly specified as 2 for future compatibility.
 *
 * NOTE:
 * If a name string is filled in an an MBBI/MBBO record, it's corresponding
 * value will be filled in as well.  For this reason, there MUST be
 * a value array and a valid nobt value for every MBBI/MBBO record that
 * contains an item array!
 *
 ******************************************************************************/

struct devGpibNames {
  int           count;          /* CURRENTLY only used for MBBI and MBBO */
  char          **item;
  unsigned long *value;         /* CURRENTLY only used for MBBI and MBBO */
  short         nobt;           /* CURRENTLY only used for MBBI and MBBO */
};

/******************************************************************************
 *
 * Enumeration of gpib command types supported.
 * 
 * Each transaction type is described below :
 *
 * GPIBREAD : (1) The cmd string is sent to the instrument
 *            (2) Data is read from the inst into a buffer (gpibDpvt.msg)
 *            (3) The important data is extracted from the buffer using the
 *                format string.
 *
 * GPIBWRITE: (1) An ascii string is generated using the format string and
 *                contents of the gpibDpvt->dbAddr->precord->val
 *            (2) The ascii string is sent to the instrument
 *
 * GPIBCMD :  (1) The cmd string is sent to the instrument
 *
 * GPIBCNTL : (1) The control string is sent to the instrument (ATN active)
 *
 * GPIBSOFT : (1) No GPIB activity involved - normally retrieves internal data
 *
 * GPIBREADW : (1) The cmd string is sent to the instrument
 *             (2) Wait for SRQ
 *             (3) Data is read from the inst into a buffer (gpibDpvt.msg)
 *             (4) The important data is extracted from the buffer using the
 *                 format string.
 *
 * GPIBRAWREAD:  Used internally with GPIBREADW.  Not useful from cmd table.
 *
 *
 * The following is only supported on mbbo and bo record types.
 *
 * GPIBEFASTO:	(1) Sends out the string pointed to by p3[VAL] w/o formating
 *
 * The following are only supported on mbbi and bi record types.
 *
 * GPIBEFASTI:	(1) Send out the cmd string 
 *		(2) Data is read from the inst into a buffer (gpibDpvt.msg)
 *              (3) Check the response against P3[0..?]
 *		(4) Set the value field to index when response = P3[index]
 *
 * GPIBEFASTIW:	(1) Send out the cmd string
 *		(2) Wait for SRQ
 *		(3) Data is read from the inst into a buffer (gpibDpvt.msg)
 *		(4) Check the response against P3[0..?]
 *		(5) Set the value field to index when response = P3[index]
 *
 * If a particular GPIB message does not fit one of these formats, a custom
 * routine may be provided. Store a pointer to this routine in the
 * gpibCmd.convert field to use it rather than the above approaches.
 *
 ******************************************************************************/

#define GPIBREAD        1
#define GPIBWRITE       2
#define GPIBCMD         3
#define GPIBCNTL        4
#define GPIBSOFT        5
#define GPIBREADW       6
#define GPIBRAWREAD     7
#define	GPIBEFASTO	8
#define	GPIBEFASTI	9
#define	GPIBEFASTIW	10


struct gpibCmd {
  gDset *rec_typ;               /* used to indicate record type supported */
  int   type;                   /* enum - GPIBREAD, GPIBWRITE, GPIBCMND */
  short	pri;                    /* request priority--IB_Q_HIGH or IB_Q_LOW*/
  char  *cmd;			/* CONSTANT STRING to send to instrument */
  char  *format;                /* string used to generate or interpret msg*/
  long rspLen;                 /* room for response error message*/
  long msgLen;                 /* room for return data message length*/

  int   (*convert)();           /* custom routine for conversions */
  int   P1;                     /* user defined parameter used in convert() */
  int   P2;                     /* user defined parameter used in convert() */
  char  **P3;                   /* user defined parameter used in convert() */

  struct devGpibNames   *namelist;  /* pointer to name strings */

  int	companion;		/* companion command (used at init time) */
};

#define FILL  {0,0,0,NULL,NULL,0,0,NULL,0,0,NULL,NULL,-1}
#define FILL10 FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL,FILL

/******************************************************************************
 * 
 * debugFlag:
 *   Must point to a flag used to request debugging traces for the device
 *   while executing library code.
 *
 * respond2Writes:
 *   Set to TRUE if the device responds to write operations.  This causes
 *   a read operation to follow each write operation.  (See also wrConversion)
 *
 * timeWindow:
 *   Set to the number of system ticks that should be skipped after a timeout
 *   is detected on a device.  All commands issued within this time window
 *   will be aborted and returned as errors.
 *
 * hwpvtHead:
 *   This is the root pointer for the per-hardware device private structure 
 *   list.  It should ALWAYS be initialized to NULL.
 *
 * gpibCmds:
 *   Pointer to the gpibCmds array.
 *
 * numparams:
 *   The number of parameters described in the gpibCmds array.
 *
 * magicSrq:
 *   Set to the parameter number that should be processed if an unsolicited
 *   SRQ is detected.
 *
 * name:
 *   Must point to a string containing the device name (used for 
 *   debug messages.)
 *
 * srqHandler:
 *   Must point to the SRQ handler for the device support module if SRQs are
 *   supported or NULL if not.
 *
 * wrConversion:
 *   If not set to NULL and respond2Writes is true and a GPIBWRITE or GPIBCMD
 *   operation has completed it's read portion, this secondary conversion
 *   routine is called.
 *
 ******************************************************************************/

typedef struct devGpibParmBlock {
  int	*debugFlag;		/* pointer to debug flag */
  int	respond2Writes;		/* set to true if a device responds to writes */
  int	timeWindow;		/* clock ticks to skip after a timeout */
  struct hwpvt *hwpvtHead;	/* pointer to the hwpvt list for device type */
  struct gpibCmd *gpibCmds;	/* pointer to gpib command list */
  int	numparams;		/* number of elements in the command list */
  int	magicSrq;		/* magic parm to handle unsolicited SRQs */
  char	*name;			/* pointer to a string containing device type */
  int	dmaTimeout;		/* clock ticks to wait for DMA to complete */

  int	(*srqHandler)();	/* user SRQ handler or NULL if not supported */

  int	(*wrConversion)();	/* secondary conversion routine */
} devGpibParmBlockStruct;

#endif
