/* drvAb.h */
/*share/src/drv $Id$ */
/* header file for the Allen-Bradley Remote Serial IO
 *
 * Author:	Bob Dalesio
 * Date:	6-21-88
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	02-01-89	lrd	added IFE module support
 *				changed max chan from 8 to 16
 * .02	04-24-89	lrd	moved AB_* to module_types.h
 *				added bit to config table for attempting 
 *				initialization
 * .03	01-30-90	lrd	add plc/adapter distinction
 * .04  03-03-92        mrk     added defs for Type E,T,R,S Tcs
 *
 */
/* NOTES:							
 * 1. We are using double slot addressing which dictates that there is an 
 *    output slot for each input slot. 
 *    Slots 1,3,5,7,9,11 are input slots
 *    Slots 2,4,6,8,10,12 are output slots 
 *    The database uses card numbers 0-11 to address slots 1-12
 * 2. This code has not been tested for more than one link (6008). More
 *    extensive modifications are required to implement this
 */

/* osw - operating status word definitions */
#define	PROGRAM_MODE	0x01		/* program/reset mode */
#define TEST_MODE	0x02		/* test/reset mode */
#define RUN_MODE	0x04		/* run mode */
#define DEBUG_MODE	0x08		/* we are debugging */
#define UNSOLICITED_BT	0x10		/* detected block xfer we didn't want */
#define BTS_QUEUED	0x20		/* block xfers queued */
#define ADAPTER_FAULT	0x40		/* at least one faulted adapter in scan
					   list */
#define ADAP_FLT_CHNG	0x80		/* a change has occurred in an adapter's
					   status...do link status check */
#define OSW_MASK	0xdf		/* these are the bits of interest */

/* commands to the 6008 link controller */
#define AUTO_CONF	0x10
#define SCAN_LIST	0x11
#define SET_UP		0x13
#define SET_MODE	0x20
#define	LINK_STATUS	0x21

/* block transfer command definitions */
#define AB_READ		0x01
#define AB_WRITE	0x02

/* these are used for the SET_UP command */
#define DEF_RATE	0x01		/* default baud rate 57.6K */
#define FAST_RATE	0x02		/* 115.2 KB */
#define NO_CHANGE	0xff		/* no change */
#define AB_IL		0x05		/* interrupt level */
#define DEBUG		0x01		/* debug - turn off watchdog timer */
#define AB_INT_ENABLE	0x00		/* interrupt enabled */
#define AB_SYSFAIL_DISABLE	0x01	/* disable  VMEbus SYSFAIL signal */

/* status returned through the dual ported memory */
#define SCANNER_POWERUP	0x90		/* power up status */
#define BT_ACCEPTED	0x2f		/* block trans accepted by scanner */
#define	BT_TIMEOUT	0x23		/* block transfer timeout */


/* DUAL PORTED MEMORY AREA DEFINITION */
/* mail box definition */
struct dp_mbox {
	unsigned short conf_stat;	/* confirmation status word */
	unsigned short command;		/* command word */
	unsigned short address;		/* module address */
	unsigned short bt_tag;		/* block transfer tag number */
	unsigned short resv[9];		/* 18 bytes reserved */
	unsigned char  dummy;
	unsigned char  fl_lock;		/* semaphore word byte */
	unsigned short data_len;	/* length of data sent/returned */
	union {
		unsigned short wd[64];	/* 64 words of data */
		unsigned char bd[128];	/* or 128 bytes of data */
	} da;
};

/* entire region with the mailbox */
struct ab_region {
	unsigned char oit[128];		/* output image table */
	unsigned char iit[128];		/*  input image table */
	unsigned short osw;		/* operating status word */
	struct dp_mbox mail;		/* dual port mail box */
	unsigned short gda[1872-66];	/* unused part gen data area */
	unsigned short	sys_fail_set1;	/* first byte to set for recovering from SYSFAIL */
	unsigned short	sys_fail_set2;	/* second byte to set for recovering from SYSFAIL */
	unsigned char vmeid[60];	/* scanner id */
	unsigned short sc_intr;		/* to interrupt the scanner */
	unsigned short sc_pad;		/* last word in scanner shared mem */
};

/* Allen-Bradley IO configuration array equates */
#define AB_NOT_INIT	0x0000
#define AB_NO_CARD	0xffff
#define AB_CARD_TYPE		0x001f
#define AB_INTERFACE_TYPE	0x00e0
#define	AB_CONVERSION		0x0f00
#define AB_PLC			0x1000
#define AB_SENT_INIT		0x2000
#define AB_UPDATE		0x4000
#define AB_INIT_BIT		0x8000

/* interface types */
#define AB_NOT_ASSIGNED 0x0000
#define AB_BI_INTERFACE 0x0020
#define AB_BO_INTERFACE 0x0040
#define AB_AI_INTERFACE 0x0060
#define AB_AO_INTERFACE 0x0080


/* scale data - 16 bit min and a 16 bit max */
struct	ascale	{
	unsigned short	a_min;
	unsigned short	a_max;
};

/* defines and structures for analog outputs	*/
/* configuration word 5 for the OFE module			*/
/*	FxxxHHHHLLLLPPPP					*/
/*		F - Data Format					*/
/*			0x0 specifies BCD			*/
/*			0x1 specifies Binary			*/
/*		HHHH - Max Scaling Polarity			*/
/*		LLLL - Min Scaling Polarity			*/
/*		PPPP - Value Polarity				*/
#define	OFE_BINARY	0x8000		/* talk binary instead of BCD	*/
#define	OFE_SCALING	0x0000		/* all positive			*/

struct	ab1771ofe_write{
	unsigned short	data[4];	/* data to write */
	unsigned short	conf;		/* config word */
	struct ascale	scales[4];	/* min & max scaling */
};

/* defines and structures for analog inputs       */
/* confgiuration word 0 for the IL module:			*/
/*	7766554433221100					*/
/*		00 - Signal Range for Channel 0			*/
/*			0x0 specifies 1 - 5 Volts DC		*/
/*			0x1 specifies 0 - 5 Volts DC		*/
/*			0x2 specifies -5 - +5 Volts DC		*/
/*			0x3 specifies -10 - +10 Volts DC	*/
/*		11-77 - Signal Ranges for Channels 1 - 7	*/
#define IL_RANGE	0xffff	/* volt inp for all channels -10 - 10V */

/* configuration word 1 for the IL module			*/
/*	00000DD000000000					*/
/*		DD - Data Format				*/
/*			0x0 specifiesBinary Coded Decimal	*/
/*			0x2 specifies 2s compliment binary	*/
/*			0x3 specifies signed magnitude binary	*/
#define IL_DATA_FORMAT	0x600		/* signed magnitude binary */

/* configuration word 2 for the IL module			*/
/*	HHHHHHHHLLLLLLLL					*/
/*		HHHHHHHH - Sign Bits for each of the max scaling value	*/
/*		LLLLLLLL - Sign Bits for each of the min scaling value	*/
#define IL_MAX_SCALING_SIGN	0	/* max scale is always positive */
#define IL_MIN_SCALING_SIGN	0xff	/* min scale is always negative */
#define IL_MAX_SCALE		itobcd(4095)	/* max scale is +4095 */
#define IL_MIN_SCALE		itobcd(4095)	/* min scale is -4095 */
#define IL_SCALE_SIGN	((IL_MAX_SCALING_SIGN << 8) | IL_MIN_SCALING_SIGN)
#define IL_SCALE	((IL_MAX_SCALE << 8) | IL_MIN_SCALE)

struct	ab1771il_config {
	unsigned short	ranges;		/* range for all channels */
	unsigned short	conf;		/* config word */
	unsigned short	signs;		/* scaling bits for sign */
	struct ascale	scales[8];	/* min & max for 8 channels */
};

struct	ab1771il_read {
	unsigned short	diag;	/* diagnostic word */
	unsigned short	urange;	/* low byte - under range channels */
	unsigned short	orange;	/* low byte - over  range channels */
	unsigned short	sign;	/* low byte - polarity 1 = negative */
	short		data[8];	/* 8 data values (can be signed) */
};

/* confgiuration word 0  and 1 for the IFE module:		*/
/*	7766554433221100					*/
/*	1514131211109988					*/
/*		00 - Signal Range for Channel 0			*/
/*			0x0 specifies 1 - 5 Volts DC		*/
/*			0x1 specifies 0 - 5 Volts DC		*/
/*			0x2 specifies -5 - +5 Volts DC		*/
/*			0x3 specifies -10 - +10 Volts DC	*/
/*		11-15 - Signal Ranges for Channels 1 - 15	*/
#define IFE_RANGE	0xffff	/* volt inp for all channels -10 - 10V */

/* configuration word 2 for the IFE module			*/
/*	00000DDT 00000000					*/
/*		DD - Data Format				*/
/*			0x0 specifiesBinary Coded Decimal	*/
/*			0x2 specifies 2s compliment binary	*/
/*			0x3 specifies signed magnitude binary	*/
/*		T - Input Type					*/
/*			0x0 specifies single ended		*/
/*			0x1 specifies differential		*/
#define IFE_DATA_FORMAT	0x700		/* signed magnitude binary/ differential */
#define IFE_SE_DATA_FORMAT 0x600	/* signed magnitude binary/ single-ended */

/* configuration word 3 for the IFE module			*/
/*	LLLLLLLLLLLLLLLL					*/
/*		LLLLLLLL - Sign Bits for each of the min scaling value	*/
#define IFE_MAX_SCALING_SIGN	0	/* max scale is always positive */
#define IFE_MIN_SCALING_SIGN	0xff	/* min scale is always negative */
#define IFE_MAX_SCALE		itobcd(4095)	/* max scale is +4095 */
#define IFE_MIN_SCALE		itobcd(4095)	/* min scale is -4095 */
#define IFE_SCALE_SIGN	((IFE_MAX_SCALING_SIGN << 8) | IFE_MIN_SCALING_SIGN)
#define IFE_SCALE	((IFE_MAX_SCALE << 8) | IFE_MIN_SCALE)

struct	ab1771ife_config {
	unsigned short	range1;		/* range for all channels */
	unsigned short	range2;		/* range for all channels */
	unsigned short	conf;		/* config word */
	unsigned short	minsigns;	/* scaling bits for sign */
	unsigned short	maxsigns;	/* scaling bits for sign */
	struct ascale	scales[16];	/* min & max for up to 16 channels */
};

struct	ab1771ife_read {
	unsigned short	diag;	/* diagnostic word */
	unsigned short	urange;	/* low byte - under range channels */
	unsigned short	orange;	/* low byte - over  range channels */
	unsigned short	sign;	/* low byte - polarity 1 = negative */
	short		data[16];	/* 16 data values (can be signed) */
};


/* defines and structures for thermocouple inputs */
/* configuration word 0 for the IXE module:			*/
/*	SSSSSFFCUxxxxTTT					*/
/*		TTT - Thermocouple Type				*/
/*		xxxx - Not Used					*/
/*		U - Update cold junction 3/sec to calibrate	*/
/*		C - Conversion into degrees F or C		*/
/*		FF - Data Format				*/
/*		SSSSS - Sample Time				*/

/* xxxxxxxxxxxxxTTT - Thermocouple Types			*/
#define IXE_MILLI	0x0000		/* Millivolt input */
#define IXE_E		0x0001		/* "E" Thermocouple */
#define IXE_J		0x0002		/* "J" Thermocouple */
#define IXE_K		0x0003		/* "K" Thermocouple */
#define IXE_T		0x0004		/* "T" Thermocouple */
#define IXE_R		0x0005		/* "R" Thermocouple */
#define IXE_S		0x0006		/* "S" Thermocouple */

#define K_DGF   2
#define K_DGC   3
#define J_DGF   4
#define J_DGC   5
#define E_DGF   6
#define E_DGC   7
#define T_DGF   8
#define T_DGC   9
#define R_DGF   10
#define R_DGC   11
#define S_DGF   12
#define S_DGC   13


/* xxxxxxxCxxxxxxxx - Conversion into degrees F or C */
#define	IXE_DEGC	0x0000
#define IXE_DEGF	0x0100

/* xxxxxFFxxxxxxxxx - Data Format */
#define IXE_BCD		0x0000		/* data is BCD */
#define IXE_2SCOMP	0x0200		/* twos complement binary data */
#define IXE_SIGNED	0x0400		/* signed magnitude  "     "   */

/* SSSSSxxxxxxxxxxx - Scan Rate */
#define IXE_HALFSEC		0x2800		/* sample time = 0.5 seconds */
#define IXE_1SEC		0x5000		/* sample time = 1.0 seconds */
#define IXE_2SECS		0xa000		/* sample time = 2.0 seconds */
#define IXE_3SECS		0xf000		/* sample time = 3.0 seconds */

/* configuration data transfer to the IXE card */
struct ab1771ixe_config {
	unsigned short	conv_rate;	/* conversion and scan rate data */
	unsigned short	alarm_enable;	/* low byte = channel alarm enable */
	unsigned short	limit_polarity;	/* low & hi alarm polarity bits */
	struct ascale	alarm_limits[8];/* low & hi alarm values */
	unsigned short	calibration[8];	/* gain & zero correction values */
};

#define IXE_STATUS	0xff
struct ab1771ixe_read {
	unsigned short	pol_stat;	/* status - polarity word */
	unsigned short	out_of_range;	/* under - over range channels */
	unsigned short	alarms;		/* inputs outside alarm limits */
	short		data[8];	/* current values */
	unsigned short	cjcw;		/* cold junction cal word */
};

/*Conversion value passed to abb_ai_driver*/
#define IR_degF	0
#define IR_degC 1
#define IR_Ohms 2
/*Register definitions*/
#define IR_UNITS_DEGF	0x40
#define IR_UNITS_OHMS	0x80
#define IR_COPPER	0x100
#define IR_SIGNED	0x400
/* configuration data transfer to the IR card */
struct ab1771ir_config {
	unsigned short	conv_rate;	/* conversion and scan rate data */
	unsigned short	resistance;	/* 10ohm resiatance @25 degC	*/
	unsigned short	bias[6];	/* bias				*/
	unsigned short	calibration[6];	/* 				*/
};

struct ab1771ir_read {
	unsigned short	status;		/* status and over/under range	*/
	unsigned short	pol_over;	/* polarity and overflow	*/
	short		data[6];	/* current values */
};
