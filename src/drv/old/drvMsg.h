/* share/epicsH $Id$ */
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
 * .02	05-26-92	jrw	added enumeration of record types
 * .03  08-10-92	jrw	cleaned up the documentation 
 *				& removed multi-parm stuff
 */

#ifndef	DEVXXMSG_H
#define DEVXXMSG_H
/******************************************************************************
 *
 * This structure holds device-related data on a per-device basis and is
 * referenced by the xact structures.  They are built using a linked
 * list starting from parmBlock.phwpvt.  There is one linked list for each
 * specific device type.
 *
 * Different types of device links could have different formatted hwpvt structs.
 * The msgHwpvt structure includes those fields that are common to all device
 * types.  The customized part should be attached to msgHwpvt.p during the
 * genHwpvt phase if the initilization process.
 *
 ******************************************************************************/
struct msgHwpvt {
  struct msgHwpvt	*next;		/* Next structure for same type device */

  struct msgLink	*pmsgLink;	/* Associated link structure */

  unsigned long		tmoVal;		/* Time last timeout occurred */
  unsigned long		tmoCount;	/* Total timeouts since boot time */

  void			*p;		/* Place to add device-specific fields */
};
typedef struct msgHwpvt msgHwpvt;
/******************************************************************************
 *
 * Any I/O request made must consist of the msgXact structure defined below.
 *
 * In general, one EPICS database record will have one and only one of these
 * msgXact attached to it's dbCommon.dpvt field.
 *
 * The message driver system does not reference any database record fields that
 * are not in dbCommon -- unless a user-provided function calls a 
 * record-specific support function.
 *
 ******************************************************************************/
struct msgXact {
  CALLBACK	callback;
  SEM_ID        *psyncSem;      /* if not NULL, points to sem given w/RX */

  struct msgXact	*next;		/* used to link dpvt's together */
  struct msgXact	*prev;		/* used to link dpvt's together */

  struct msgHwpvt	*phwpvt;	/* assoc'd hwpvt structure */
  struct msgParmBlock	*pparmBlock;	/* assoc'd parm block */
  struct dbCommon	*prec;		/* useful to msg based device support */
  unsigned int		parm;		/* operation parm number */
  int			status;		/* transaction completion status */

  void		*p;		/* A place to add on device specific data */
};
typedef struct msgXact msgXact;
/******************************************************************************
 *
 * Valid msgXact.status values are:
 *
 ******************************************************************************/
#define XACT_OK         0       /* all went as expected */
#define XACT_LENGTH     1       /* received message overflowed the buffer */
#define XACT_TIMEOUT    3       /* response took too long from node */
#define XACT_BADLINK    4       /* request sent to an invalid link number */
#define XACT_BADPRIO    5       /* request had bad queueing priority */
#define XACT_IOERR      6       /* some sort of I/O error occurred */
#define	XACT_BADCMD	7	/* bad command ID-number */

/******************************************************************************
 *
 * Transaction requests are placed on queues (by the drvMsg_qXact() function)
 * when submitted.  These queues are described with the xactQueue structure.
 *
 ******************************************************************************/
struct xactQueue {
  struct msgXact	*head;  /* head of the linked list */
  struct msgXact	*tail;  /* tail of the linked list */
  FAST_LOCK		lock;    /* semaphore for the queue list */
};
typedef struct xactQueue xactQueue;
/******************************************************************************
 *
 * A task is started to manage each message based link.  It uses the msgLink
 * structure to access the queues of work.
 *
 * A link is defined by the device-specific part of the genHwpvt() function.
 * A link task will be started if the genHwpvt() function invokes the genLink()
 * function to define a new link.
 *
 ******************************************************************************/
struct msgLink{
  struct xactQueue queue[NUM_CALLBACK_PRIORITIES]; /* prioritized request queues */
  struct msgLink 	*next;	/* next in msgDrvBlock list */
  SEM_ID		linkEventSem;
  void			*p;	/* A place to add on device specific data */
};
typedef struct msgLink msgLink;
/******************************************************************************
 *
 * The msgParmBlock is defined by the user-contributed device support module.
 * It is used to tell the message-driver what device driver to used to perform
 * the I/O operations as well as to provide access to the command table, and
 * other parameters.
 *
 * The msgParmBlock contains those parts that are common to all device types.
 * Some devices may require more operating parameters.  These additional
 * parameters should be added via msgParmBlock.p.
 *
 ******************************************************************************/
struct msgParmBlock{
  int		*debugFlag;	/* pointer to debug flag */
  struct msgHwpvt *pmsgHwpvt;	/* pointer to the hwpvt list for device type */
  struct msgCmd	*pcmds;		/* pointer to command list */
  int		numCmds;	/* number of elements in the command list */
  char		*name;		/* pointer to a string containing device name */
  struct msgDrvBlock *pdrvBlock; /* link to the driver hook data structure */
  DRVSUPFUN	doOp;		/* user spec'd operation wedge function */
  void	*p;			/* A place to add device specific data */
};
typedef struct msgParmBlock msgParmBlock;

/******************************************************************************
 *
 * The msgCmd structure is used to define each entry in the command table.
 * This table is used to define all the commands that a specific device type
 * can handle.
 *
 * When a transaction is processed, the following happens:
 *
 * 1) if A defered read event has occurred, cmd.readOp is called.
 * otherwise:
 * 1) if cmd.writeOp is non-NULL, it is called and passed wrParm
 *         The job of writeOp is to send a string to the instrument
 * 2) if cmd.readOp is non-NULL, it is called and passed rdParm
 *         The job of readOp is to read a string from the instrument
 * 
 ******************************************************************************/
struct msgCmdOp {
  unsigned int	op;
  void		*p;
};
typedef	struct msgCmdOp msgCmdOp;

struct msgCmd{
  struct msgRecEnum	*recTyp;/* used to indicate record type supported */
  unsigned int		flags;	/* 1 if readback is deferred, 0 if not */

  struct msgCmdOp	writeOp;
  struct msgCmdOp	readOp;

  struct devMsgNames   *namelist;/* pointer to name strings */
  int	companion;
};
typedef struct msgCmd msgCmd;

/* Possible values for msgCmd.flags (OR the set required together) */
#define	READ_NDLY	0	/* Do read now */
#define	READ_DEFER	1	/* Defer read, an event will signal it */

/******************************************************************************
 *
 * Valid writeOp and readOp values.
 *
 * Any user-add-on codes start at MSG_OP_USER and continue upward.
 *
 ******************************************************************************/
#define	MSG_OP_NOP	0	/* Do nothing */
#define	MSG_OP_WRITE	1	/* Unformatted character write */

#define	MSG_OP_FAI	2	/* Read and parse an analog input */
#define	MSG_OP_FAO	3	/* Format and output an analog output */
#define	MSG_OP_FBI	4	/* Read and parse */
#define MSG_OP_FBO	5	/* Format and output */
#define MSG_OP_FMI	6	/* Read and parse */
#define MSG_OP_FMO	7	/* Format and output */
#define MSG_OP_FLI	8	/* Read and parse */
#define MSG_OP_FLO	9	/* Format and output */
#define MSG_OP_FSI	10	/* Read and parse */
#define MSG_OP_FSO	11	/* Format and output */

#define MSG_OP_RSI	12	/* Read a string raw */
#define MSG_OP_RSO	13	/* Write a string raw */

#define	MSG_OP_ACK	14	/* Read string and set alarm if not match */

#define	MSG_OP_SBI	15	/* Read string, set BI on substring comparison */

/* NOT CURRENTLY SUPPORTED OPERATIONS */
#if FALSE
#define MSG_OP_EBI		/* enumerated operations */
#define MSG_OP_EBO
#define MSG_OP_EMI
#define MSG_OP_EMO
#endif

#define	MSG_OP_USER	200	/* Start of user added operation codes */

/******************************************************************************
 *
 * The following represent the op-codes that can be passed to the 
 * msgDrvBlock.genlink function.
 *
 ******************************************************************************/
#define	MSG_GENLINK_CREATE	0	/* Create structures for a new link */
#define	MSG_GENLINK_ABORT	1	/* Link failed init */

/******************************************************************************
 *
 * Codes that can be returned from event checker routines.
 *
 ******************************************************************************/
#define	MSG_EVENT_NONE		0
#define	MSG_EVENT_READ		1
#define	MSG_EVENT_WRITE		2

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

struct drvMsgNames {
  int           count;          /* CURRENTLY only used for MBBI and MBBO */
  char          **item;
  unsigned long *value;         /* CURRENTLY only used for MBBI and MBBO */
  short         nobt;           /* CURRENTLY only used for MBBI and MBBO */
};
typedef struct drvMsgNames drvMsgNames;
/******************************************************************************
 *
 * Public functions available from the message driver.
 *
 ******************************************************************************/
msgXact	*drvMsg_genXact();
msgHwpvt *drvMsg_genHwpvt();
msgLink	*drvMsg_genLink();
long	drvMsg_qXact();
long   	drvMsg_reportMsg(); 
long	drvMsg_initMsg();
long	drvMsg_xactWork();
long	drvMsg_initCallback();
long	drvMsg_checkParm();
long	drvMsg_drvWrite();
long	drvMsg_drvRead();

long	drvMsg_initAi(), drvMsg_initAo();
long	drvMsg_initBi(), drvMsg_initBo();
long	drvMsg_initMi(), drvMsg_initMo();
long	drvMsg_initLi(), drvMsg_initLo();
long	drvMsg_initSi(), drvMsg_initSo();
long	drvMsg_initWf();

long	drvMsg_procAi(), drvMsg_procAo();
long	drvMsg_procBi(), drvMsg_procBo();
long	drvMsg_procMi(), drvMsg_procMo();
long	drvMsg_procLi(), drvMsg_procLo();
long	drvMsg_procSi(), drvMsg_procSo();
long	drvMsg_procWf();

long	drvMsg_proc();

/******************************************************************************
 *
 * The msgDrvBlock is used by a message driver designer to specify functions
 * that are to be invoked to initialize structures and to check for events
 * that might have caused the link task to wake up.  This is where hooks to
 * device specific functions and data structures are added to the message
 * based driver.
 *
 ******************************************************************************/
struct msgDrvBlock{
  char		*taskName;	/* Used for the taskSpawn and messages */
  int		taskPri;	/* Used for the taskSpawn */
  int		taskOpt;	/* Used for the taskSpawn */
  int		taskStack;	/* Used for the taskSpawn */
  struct msgLink *pmsgLink;	/* associated msgLink list head */

  DRVSUPFUN	drvWrite;	/* Write string from buffer */
  DRVSUPFUN	drvRead;	/* Read string into buffer */
  DRVSUPFUN	drvIoctl;	/* perform device specific control function */
};
typedef struct msgDrvBlock msgDrvBlock;

/* IOCTL functions that each message driver MUST support */
#define	MSGIOCTL_REPORT		0
#define	MSGIOCTL_INIT		1
#define	MSGIOCTL_INITREC	2
#define MSGIOCTL_GENXACT	3
#define MSGIOCTL_GENHWPVT	4
#define MSGIOCTL_GENLINK	5
#define	MSGIOCTL_CHECKEVENT	6
#define MSGIOCTL_COMMAND	7	/* device-type specific */

/******************************************************************************
 *
 * The parameter passed to the IOCTL function is different depending on the
 * function-code used in the IOCTL call.  These parameters are defined below.
 *
 ******************************************************************************/
struct	msgDrvIniParm {
  int    	 parm;
  struct msgDset *pdset;
};
typedef struct msgDrvIniParm msgDrvIniParm;

struct msgDrvGenXParm {
 struct link		*plink;
 struct msgXact		*pmsgXact;
};
typedef struct msgDrvGenXParm msgDrvGenXParm;

struct msgDrvGenHParm {
  struct msgParmBlock	*pparmBlock;
  struct link		*plink;
  struct msgHwpvt	*pmsgHwpvt;
};
typedef struct msgDrvGenHParm msgDrvGenHParm;

struct msgDrvGenLParm {
  struct msgLink	*pmsgLink;
  struct link		*plink;
  int			op;
  struct msgParmBlock	*pparmBlock;
};
typedef struct msgDrvGenLParm msgDrvGenLParm;

struct msgChkEParm {
  struct msgDrvBlock	*pdrvBlock;
  struct msgLink	*pmsgLink;
  struct msgXact	**pxact;
};
typedef struct msgChkEParm msgChkEParm;

/******************************************************************************
 *
 * Each DSET ppoints to a msgRecEnum structure.  This structure defines the
 * type of record the dset represents.  The idea here is that the address
 * of this structure represents the enumeration value for a given record type.
 *
 * New record types may be added by the application developer
 * by defining a msgRecEnum structure and then using its address in the
 * DSET for the new record type.
 *
 * In the future it is intended that this structure be used to describe
 * the locations and types of the I/O sensitive fields within a record.
 *
 ******************************************************************************/
struct msgRecEnum{
  char		recType[20];	/* Holds string describing record type */
};
typedef struct msgRecEnum msgRecEnum;

extern msgRecEnum drvMsgAi;
extern msgRecEnum drvMsgAo;
extern msgRecEnum drvMsgBi;
extern msgRecEnum drvMsgBo;
extern msgRecEnum drvMsgMi;
extern msgRecEnum drvMsgMo;
extern msgRecEnum drvMsgLi;
extern msgRecEnum drvMsgLo;
extern msgRecEnum drvMsgSi;
extern msgRecEnum drvMsgSo;
extern msgRecEnum drvMsgWf;

/******************************************************************************
 *
 * This is a modified DSET that is used by device support modules that use
 * the message driver's services.  It starts the same as a regular DSET, and
 * then includes 2 extra pointers.  One to the parmBlock structure for the
 * device type being defined.  And one that points to the record-type
 * enumeration structure.
 *
 ******************************************************************************/
struct msgDset{
  long         	number;
  DEVSUPFUN    	funPtr[10];
  msgParmBlock	*pparmBlock;
  msgRecEnum	*precEnum;
};
typedef struct msgDset msgDset;

/******************************************************************************
 *
 * This represents the parameters that are passed to the raw read and raw
 * write physical device drivers when data is to be transferred.
 *
 * It is assumed that the 'len' values found in the following structures
 * is the number of valid characters to transfer.  There is always a '\0'
 * added to the ens of this buffer, so the actual buffer size must be at
 * least len+1 bytes long.
 *
 ******************************************************************************/
typedef struct {
  char	*buf;		/* Buffer to hold the data bytes */
  int	len;		/* Max number of bytes to read/write */
  int	actual;		/* actual number of bytes transfered */
} msgStrParm;

/******************************************************************************
 *
 * The following are the parameters that are passed to the record-specific
 * conversion functions.  These functions are provided by the message driver
 * for convenience only.  They are only invoked if the command table contains
 * the proper op-codes and/or if the user-supplied code calls them.
 *
 ******************************************************************************/
typedef struct {
  char	*format;	/* Format string for the sprintf function */
} msgFoParm;

typedef struct {
  char	*format;	/* Format string for the sscanf function */
  int	startLoc;	/* where to start scanning the input string from */
  int	len;		/* Max number of bytes to read in */
} msgFiParm;

typedef struct {
  char	*str;		/* value to compare response against */
  int	index;		/* index to start location of comparison */
  int	len;		/* max number of chars to read from device */
} msgAkParm;

typedef struct {
  char	*str;		/* string to compare */
  int	index;		/* character position to start comparison */
  int	len;		/* max bytes to read from device */
} msgSBIParm;

#endif
