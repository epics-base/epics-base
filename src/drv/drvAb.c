
/* drvAb.c */
/* share/src/drv $Id$ */

/* drvAb.c -  Driver Support Routines for Alleb Bradley */

/* share/src/drv @(#)ab_driver.c	1.4     5/15/92 */
/*
 * routines that are used, below the ai, ao, bi and bo drivers to interface
 * to the Allen-Bradley Remote Serial IO
 *
 * 	Author:	Bob Dalesio
 * 	Date:	6-21-88
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
 * Notes:	
 *   1. This driver uses Asynchronous Communications to the AB scanner
 *	The manual description for this type of interface is
 *	misleading in the area of interrupt handling.
 *   2.	We are using double slot addressing as the ab1771il card only works
 *      in this addressing mode. This dictates that analog io cards alternate 
 *	between input and output. Therefore there are 6 input slots and 
 *	6 output slots per adapter. The binaries may be placed in either slot.
 *      Slots 1,3,5,7,9,11 are analog input slots
 *      Slots 2,4,6,8,10,12 are analog output slots 
 *      The database uses card numbers 0-11 to address slots 1-12
 *   3. The binary io is memory mapped and the analog io is handled through
 *	block transfers of whole cards of data.
 *   4. The timing rates for the analog inputs should not be made lower as
 *      the cards will not respond to the AB scanner any faster.
 *
 * Modification Log:
 * -----------------
 * .01 09-15-88		lrd	only spawn tasks if Allen-Bradley IO is present
 * .02 09-15-88		lrd	use the ##open(name,flags,mode)## circuit detect
 *				from the 6008 for read failure indication
 * .03 09-15-88		lrd	documentation for data and command flow
 * .04 12-18-88		lrd	fix the conversions
 * .05 02-01-89		lrd	changed vxTas to sysBusTas!!!!!!!!!!!!
 *				delete tasks before spawning again
 *				add 1771IFE card support
 * .06	02-10-89	lrd	read OFE at initialization
 * .07	02-24-89	lrd	modify for vxWorks 4.0
 *				change spawn to taskSpawn and rearranged args
 *				change sysSetBCL to sysIntEnable
 *				move task data to task_params.h
 * .08	03-10-89	lrd	keep the scan list at initialization
 * .09	03-14-89	lrd	move interrupt enable to ioc_init
 * .10	03-17-89	lrd	modify link_init for restart
 *				provide read routines for outputs
 * .11  03-23-89        lrd     fixed ab_bidriver address not even numbers only
 *                              and the mask and pvalue args were switched
 * .12	03-25-89	lrd	routines for the database to read the outputs
 *				at initialization
 * .13	04-22-89	lrd	implement binary IO to ignore addressing mode
 *				implement more than one serial link
 * .14	04-24-89	lrd	have one outstanding attempt for initialization
 * .15	04-26-89	lrd	removed card type checking for binary IO
 *				as the binaries are memory mapped into a
 *				seperate area
 * .16	04-27-89	lrd	recover from IO failures and pass the failure
 *				status back to the database
 * .17	04-28-89	lrd	fleshed out timeout counters and added check
 *				for conversion errors on IL and IFE modules
 * .18	05-15-89	lrd	took out scaling error check and made thre
 *				consecutive timeouts before a change of status
 * .19	08-01-89	lrd	changed mr_wait and bt_queue to always set 
 *				length to 0 for compatibility with PLC-5
 *				communication
 * .20	09-29-89	ba/lrd	changed the interrupt routine to remove response
 *				data from the dual port memory and queueing the
 *				entire response
 * .21	09-29-89	lrd	add scan_list command and support to build the
 *				scan list according to the database thus polling
 *				racks that may not have been powered at
 *				initialization
 * .22	09-29-89	lrd	add more error checking
 * .23  10-17-89	cbf	modified the way interrupts are dispatched in 
 *				ab_intr
 * .24  10-17-89	cbf	added logic to avoid queueing requests to cards
 *				which already had a request pending to avoid
 *				overflowing scanner's queue when I/O card is missing
 * .25  10-17-89	cbf	added code to grab the operating status word
 *				in the interrupt routine to determine when the
 *				status of any scanned adapter has changed
 * .26	10-17-89	cbf	added logic to see if the scanner was using the
 *				dual port when we had it locked.  it is!
 * ****************************************************************************
 * *    Many of the above were needed to work around problems in the          *
 * *    6008-SV firmware.  These problems have been reported to Allen-        *
 * *    Bradley and fixes are promised.  The PROMs we are presently using     *
 * *	are both labeled "91332-501".  U63 chksum = A7A1.  U64 chksum = 912B  *
 * *	(Series A, Revision C.)						      *
 * ****************************************************************************
 * .27	12-11-89	cbf	new PROMs implemented which allow output image table
 *				to be preserved during SYSTEM RESET. This driver
 *				is still compatible with the old firmware also.
 *				A piece of code was added to store the firmware
 *				revision level in array ab_firmware_info.
 * .28  12-11-89	cbf	With the new firmware, the scanner no longer uses
 *				the dual port when we have it locked so the tests
 *				for this condition have been removed.
 * .29  01-23-90	cbf	In the previous rev of this driver we made an attempt
 *				to build a scan list automatically by waiting until
 *				the periodic scan task had asked for data from each
 *				needed adapter.  This scheme caused problems with the
 *				the initialization of BOs and AOs, because they were
 *				not initially in the scan list and so could not be read
 *				at initialization.  In this rev, that code has been
 *				removed and the scan list is always built to include all
 *				eight possible adapters.  This has some performance
 *				implications because each non-existant adapter which
 *				appears in the scan list adds an extra 5 ms to each
 *				I/O scan time for the 6008SV.  The decreased performance
 *				does not appear to be very significant, however.  The
 *				6008SV front panel "SER" LED blinks if not all adapters
 *				in the scan list are responding.  With this rev of the
 *				driver, the light will therefore be blinking. 
 * .30	01-30-90	lrd	add plc/adapter distinction
 * .31	05-24-90	mk	added 4-20Ma IFE support
 * .32	05-25-90	jcr	added PLC readback support
 * .33	07-03-90	lrd	fixed PLC readback verification
 * .34	07-19-90	mk/lrd	fixed the overrange indications (<< -> <<=)
 * .35	11-02-90	lrd	initialize the adapter buffers whether or not
 *				the link is there for detection by the bi and
 *				bo driver interface to the database library
 * .36	11-27-90	lrd	add support for the 16 BI and BO cards
 * .37	12-19-90	lrd	added 0-5V IFE support
 *****************************************************************************************
 **	Allen-Bradley has given us a new firmware revision. This revision allows the reset	*
 **	of the scanner without using the sysReset on the VME backplane. It also maintains	*
 **	the output image table for the binary outputs. This reset is used to recover from	*
 **	a scanner fault, where the 6008 firmware deadends - ab_reset provides this function.	*
 ************************************************************************************************
 * .38	03-07-91	lrd	modify ab_driver_init to not reinitialize semaphores and interrupts
 *				if this is not the first invocation
 * .39	03-07-91	lrd	modify the ab_aodriver to only set the update bit if the new value
 *				is different than the old value
 * .40	03-08-91	lrd	fix ab_boread to verify card is present
 * .41	05-16-91	lrd	have the ab_scan_task driver analog outputs
 *				periodically as well as on change
 * .42	08-06-91	rac	remove include for alarm.h--it's not used
 * .43  09-11-91	joh 	updated for v5 vxWorks
 * .44  11-16-91	 bg 	moved io_report for Allen Bradley to this module. Broke
 *                              io_report up into subroutines.Added sysBusToLocalAdrs 
 *                              and SysIntEnable
 * .45   1-13-92         bg     added level to io_report and added the ability to print raw 
 *                              values if the level is > 0 .    
 * .46	05-22-92	lrd	added the task that monitors the binary inputs and simulates a change
 *				of state interrupt - wakes up the io event scanner
 * .47	06-10-92	 bg	combined drvAb.c and ab_driver.c                                 
 */

/*
 * Binary Input Code Portions:
 *
 *           process_bi
 *                 |
 *                 |
 *                 |
 *                 V
 *           ab_bidriver
 *      |                 ^
 *      | mark card       | data 
 *      | as BI           |
 *      V                 |
 * ab_config     AB dual ported memory
 *                        ^
 *                        |
 *                        |
 *                        |
 *              AB 6008 scanner card
 */



/*
 * Binary Output Code Portions:
 *
 *           process_bo
 *                 |
 *                 |
 *                 |
 *                 V
 *           ab_bodriver
 *      |                 |
 *      | mark card       | data 
 *      | as BO           | 
 *      V                 V
 * ab_config     AB dual ported memory
 *                        |
 *                        |
 *                        |
 *                        V
 *              AB 6008 scanner card
 */

/*
 * Analog Input Code Portions:
 *
 *           process_ai
 *                 |
 *                 V
 *           ab_aidriver
 *      |                 ^
 *      | mark card       |  
 *      | as AI           | data
 *      V                 |
 *  ab_config         ab_btdata
 *      |                 ^
 *      | cards           | data
 *      | present         | returned
 *      V                 |
 * abScanTask         abDoneTask
 *      |                 ^
 *      | commands        | data
 *      V                 |
 * bt_queue             bt_done
 *      |                 ^
 *      | commands        | data
 *      V                 |
 *      AB dual ported memory
 *      |                 ^
 *      | commands        | data
 *      V                 |
 *      AB 6008 scanner card
 */

/*
 * Analog Output Code Portions:
 *
 *           process_ao
 *                 |
 *                 V
 *           ab_aidriver
 *      |                 |
 *      | mark card       |  
 *      | as AI           | data
 *      V                 V
 *  ab_config         ab_btdata
 *      |                 |
 *      | cards           | data
 *      | present         | returned
 *      V                 V
 *           abScanTask
 *      |
 *      | command/data
 *      V
 * bt_queue             bt_done
 *      |                 ^
 *      | command/data    | acknowledge
 *      V                 |
 *      AB dual ported memory
 *      |                 ^
 *      | command/data    | acknowledge
 *      V                 |
 *      AB 6008 scanner card
 */

#include	<vxWorks.h>
#include	<sysLib.h>		/* library for task  support */
#include	<semLib.h>		/* library for semaphore support */
#include	<vme.h> 
#include 	<wdLib.h>		/* library for watchdog timer support */
#include 	<rngLib.h>             /* library for ring buffer support */
#include	<task_params.h>
#include	<stdioLib.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<ab_driver.h>

		/* AllenBradley serial link and ai,ao,bi and bo cards */
#define LOCK -2
		/* AllenBradley serial link and ai,ao,bi and bo cards */

/* If any of the following does not exist replace it with #define <> NULL */
static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvAb={
	2,
	report,
	init};



static long report(fp)
    FILE	*fp;
{
   ab_io_report();
}


/* forward reference for ioc_reboot */
int ioc_reboot();

static long init()
{
    int status;

    ab_driver_init();
    return(0);
}


short	   ab_disable=0;

/*jcr */
/* response queue */
struct ab_response{
unsigned short	status;
unsigned short	link;
unsigned short	adapter;
unsigned short	card;
unsigned short	command;
unsigned short	data[64];
};

/* internal timeout variables */
short ab_tout[AB_MAX_LINKS];		/* time out flag */
WDOG_ID wd_id[AB_MAX_LINKS];	/* watchdog task ID */
int	ab_timeout();		/* internal timeout handler */

/* message complete variables */
SEMAPHORE	ab_data_sem;		/* transfer complete semaphore */
SEMAPHORE	ab_cmd_sem;		/* transfer complete semaphore */
		/* following flag indicates to interrupt handler that we're
		   requesting a block xfer (and expect immediate confirmation) */
short		ab_requesting_bt[AB_MAX_LINKS];
LOCAL RING_ID	ab_cmd_q;		/* links ready to be read */
int	ab_intr ();			/* interrupt service routine */
#define	AB_Q_SZ	32*sizeof(struct ab_response)	/* AB command Q size */

/*
 * The configuration table contains a word to describe each card 
 * in each adapter. The word contains the following data:
 * 0x0000 - not initialized
 * 0xffff - no card present
 * 0x8000 - initialized
 * 0x4000 - updated data flag (used for sending analog outputs)
 * 0x2000 - real time scan initialized
 * 0x1000 - adapter/plc flag
 *	0 - adapter
 *	1 - PLC
 * 0x0f00 - conversion (from ~gta/dbcon/h/fields.h)
 *	0 - no conversion (use millivolt range)
 *	1 - linear (use millivolt range)
 *	2 - k - degrees F
 *	3 - k - degrees K
 *	4 - j - degrees F
 *	5 - j - degrees K
 * 0x00e0 - interface type
 * 	0 - Not Assigned
 *	1 - Binary Input
 *	2 - Binary Output
 *	3 - Analog Input
 *	4 - Analog Output
 * 0x001f - the Allen-Bradley card type (from ~gta/h/module_types.h)
 *	unique only per interface type
 */
unsigned short	ab_config[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];

/*
 * Allen-Bradley Raw Data read through the abScanTask. This table
 * will contain the analog inputs read using block transfers. It
 * is also where analog outputs are placed to affect a change in an
 * analog output signal Binary cards read and write directly into the 
 * Allen-Bradley dual ported memory
 */
short	ab_btdata[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS][AB_CHAN_CARD];

/* array of pointers to the Allen-Bradley 6008 serial interface cards present */
struct ab_region	*p6008s[AB_MAX_LINKS];

/* Allen-Bradley Block Transfer Task */
int	abDoneId;		/* id of the Allen-Bradley io complete task */
int	abCOSId;		/* id of the Allen-Bradley binary input change of state scan wakeup */
int	abScanId;		/* id of the Allen-Bradley scan task */

/* Timeout counters for outstanding data requests */
/* The driver has to be smart about when the data requests are made because  */
/* the Allen-Bradley IO will hold up all requests if an IO card isn't ready  */
/* to talk. The thermocouple input card has a max scan rate of .5 second and */
/* if it isn't ready when a request is made, all proceeding block transfers  */
/* will be held up. This table is used so that requests are only made at     */
/* a rate that the card can accept.					 */
/* Some of the unpleasant side affects of making this number too small are: */
/*	1. the interrupt for command done comes when the dual port memory is */
/*	unlocked - so you have to wait for the lock flag in a loop          */
/*	2. timeouts from the scanner occur rendering the data useless       */
/*	Note that this problem increases exponentially                      */
/* scan task rate is .1 second						    */
short	ab_timers[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
#define	AB_IXE_RATE	5	/* .5 seconds */
#define AB_IL_RATE	4	/* .4 seconds */
#define AB_IFE_RATE	1	/* .1 seconds */
#define AB_OFE_RATE	10	/* 1 seconds - written immediately */
#define AB_INT_LEVEL    5

/* status on the Allen-Bradley driver interface */
		/* got back read/write info instead of cmd response */
short	ab_bad_response[AB_MAX_LINKS];
		/* got back cmd info instead of read/write response */
short	ab_rw_resp_err[AB_MAX_LINKS];
		/* watchdog timeouts through our internal timer */
short	ab_comm_to[AB_MAX_LINKS];
		/* data transfer timed out */
short	ab_data_to[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
		/* data xfer timeout detected through the AB scanner */
short	ab_cmd_to[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
		/* link status and scan list command timeouts */
short	ab_link_to[AB_MAX_LINKS];
		/* under range scaling error returned through block xfer */
short	ab_scaling_error[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
		/* over range scaling error returned through block xfer */
short	ab_or_scaling_error[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];
		/* awakened but dual ported memory was unlocked */
short	ab_post_no_lock[AB_MAX_LINKS];
		/* no. time dpm locked when we wanted to talk */
short	ab_not_ready[AB_MAX_LINKS];
		/* card status 0 - good  -1 - bad */
char	ab_btsts[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS][AB_CHAN_CARD];
		/* scanner operational status word */
short	ab_op_stat[AB_MAX_LINKS];
		/* Keeps track of how many times we've asked for data
		   before we actually receive it for a given card.  Keeps
		   us from asking for data faster than card can supply it */
unsigned short	ab_btq_cnt[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];

/*
 * flags a communication error on a link status
 * which in turn flags all channels on the adapter as hardware errors
 */
short	ab_adapter_status[AB_MAX_LINKS][AB_MAX_ADAPTERS];

/* disables the scanner during a restart - required for   */
/* successful restart of the IOC			  */

/* scan list management variables */
/* these are used to build and reset the scan list according to the */
/* database							    */
		/* scan list built from the adapters_present array */
char	ab_scan_list[AB_MAX_LINKS][AB_MAX_ADAPTERS];
		/* scan attempts while waiting for initialization */
char	ab_init_cnt[AB_MAX_LINKS][AB_MAX_ADAPTERS][AB_MAX_CARDS];

/* debug variable (must be >1 to see all messages) */
short	ab_debug = 0;

/* location to store scanner firmware revision info which is avail only temporarily */
char	ab_firmware_info[AB_MAX_LINKS][96];

static char *ab_stdaddr;

/* forward references */
void wtrans();
int abScanTask();


/*
 * READ_AB_ADAPTER
 *
 * read an adapter of AB IO 
 */
read_ab_adapter(link,adapter,pass)
register unsigned short	link;
register unsigned short	adapter;
register short	pass;
{
    register unsigned short     card;
    register unsigned short     *pcard;
    short    			btq_err;
    short			msg[64];

    /* each card */
    for (card = 0, pcard = &ab_config[link][adapter][0]; 
      card < AB_MAX_CARDS;
      card++, pcard++){

	/* card determined not present */
	if (*pcard == AB_NO_CARD) continue;

	/* card present */
	if (*pcard & AB_INTERFACE_TYPE){

	    /* need intialization */ /* jcr */
	    if ((*pcard  & AB_INIT_BIT) == 0){
		if ((*pcard & AB_SENT_INIT) == 0){
		    if (*pcard & AB_PLC) {
		    	    *pcard |= AB_INIT_BIT;
			    *pcard |= AB_SENT_INIT;
		    }
		    else if (ab_card_init(pcard,adapter,card,link) == 0){
			    *pcard |= AB_SENT_INIT;
			    ab_init_cnt[link][adapter][card] = 0;
		    }
		/* did the init message get lost - try each second */
		}else if (ab_init_cnt[link][adapter][card]++ > 10){
		    if (ab_card_init(pcard,adapter,card,link) == 0){
			    ab_init_cnt[link][adapter][card] = 0;
		    }
		}
		continue;
	    }

	    /* don't make another block transfer request if one's outstanding... */
	    if((ab_btq_cnt[link][adapter][card] % 50) != 0) {
		/* ...but try again periodically in case a request got lost. */
		ab_btq_cnt[link][adapter][card]++;
		continue;
	    }

	    /* need block transfer */
	    btq_err = OK;	/* assume success */
	    if ((*pcard & AB_INTERFACE_TYPE) == AB_AI_INTERFACE){
		    switch (*pcard&AB_CARD_TYPE){
		    case (AB1771IL):
			if ((pass % AB_IL_RATE) == 0) {
		  		btq_err = bt_queue(AB_READ,link,adapter,card,12,&msg[0]);
			}	
			break;
		    case (AB1771IXE ):
			if ((pass % AB_IXE_RATE) == 0) {
		  		btq_err = bt_queue(AB_READ,link,adapter,card,12,&msg[0]);
			}
			break;
		    case (AB1771IFE):
		    case (AB1771IFE_4to20MA):
		    case (AB1771IFE_0to5V):
		  	btq_err = bt_queue(AB_READ,link,adapter,card,12,&msg[0]);
			break;
		    case (AB1771IFE_SE):
		  	btq_err = bt_queue(AB_READ,link,adapter,card,20,&msg[0]);
			break;
		    }
		    if(btq_err != OK){
			if(ab_debug)
			    logMsg("Error on AI BT request for L%x A%x C%x\n",
				link,adapter,card);
		    }

	    }else if ((*pcard&AB_INTERFACE_TYPE)==AB_AO_INTERFACE){
              	if (((pass % AB_OFE_RATE) == 0) || (*pcard & AB_UPDATE)){
		    ab_btwrite(pcard,adapter,card,link);

		}
	    }
	}
    }
}

/*
 * ab_card_init
 *
 * Allen-Bradley card initialization routines
 */
ab_card_init(pcard,adapter,card,link)
register unsigned short	*pcard;		/* AB configuration word */
register unsigned short	adapter;
register unsigned short	card;
short			link;
{
    short		msg[64];
    register short	*pmsg = &msg[0];
    register short	length;
    register short 	i;
    register short	*pab_table;

    bfill(pmsg,64*2,0);

    switch (*pcard & AB_INTERFACE_TYPE){
    case (AB_AI_INTERFACE):
        switch (*pcard & AB_CARD_TYPE){
        case (AB1771IXE):       /* millivolt module */
	    /* need to base this on conversion */
	    switch ((*pcard & AB_CONVERSION) >> 8){
	    case (K_DGF):
		*pmsg = IXE_K | IXE_DEGF | IXE_SIGNED | IXE_HALFSEC;
		break;
	    case (K_DGC):
		*pmsg = IXE_K | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC;
		break;
	    case (J_DGF):
		*pmsg = IXE_J | IXE_DEGF | IXE_SIGNED | IXE_HALFSEC;
		break;
	    case (J_DGC):
		*pmsg = IXE_J | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC;
		break;
	    default:
		*pmsg = IXE_MILLI | IXE_DEGC | IXE_SIGNED | IXE_HALFSEC;
		break;
	    }
	    length = 27;
            break;
        case (AB1771IL):
	    *pmsg = IL_RANGE;		/* -10 to +10 volts */
	    *(pmsg+1) = IL_DATA_FORMAT;	/* signed magnitude */
	    *(pmsg+2) = 0x0ff;
	    length = 19;
            break;
        case (AB1771IFE):
	    *pmsg = 0xffff;/*IFE_RANGE;			 -10 to +10 volts */
	    *(pmsg+1) = 0xffff;/*IFE_RANGE;		 -10 to +10 volts */
	    *(pmsg+2) = 0x0700;/*IFE_DATA_FORMAT; /* signed magnitude, differential */
	    *(pmsg+3) = 0x0ffff;
	    for (i = 6; i <= 36; i+=2) *(pmsg+i) = 0x4095;
	    length = 37;
            break;
        case (AB1771IFE_SE):
	    *pmsg = 0xffff;/*IFE_RANGE;			 -10 to +10 volts */
	    *(pmsg+1) = 0xffff;/*IFE_RANGE;		 -10 to +10 volts */
	    *(pmsg+2) = 0x0600;/*IFE_SE_DATA_FORMAT;  signed magnitude, single-ended */
	    *(pmsg+3) = 0x0ffff;
	    for (i = 6; i <= 36; i+=2) *(pmsg+i) = 0x4095;
	    length = 37;
            break;
        case (AB1771IFE_4to20MA):
	    *pmsg = 0x0000;/*IFE_RANGE;			 4to20 MilliAmps  */
	    *(pmsg+1) = 0x0000;/*IFE_RANGE;		 4to20 MilliApms  */
	    *(pmsg+2) = 0x0700;/*IFE_DATA_FORMAT; /* signed magnitude, double */
	    for (i = 6; i <= 36; i+=2) *(pmsg+i) = 0x4095;
	    length = 37;
            break;
        case (AB1771IFE_0to5V):
	    *pmsg = 0x5555;/*IFE_RANGE;			 0 to 5 Volts  */
	    *(pmsg+1) = 0x5555;/*IFE_RANGE;		 0 to 5 Volts  */
	    *(pmsg+2) = 0x0700;/*IFE_DATA_FORMAT; /* signed magnitude, double */
	    for (i = 6; i <= 36; i+=2) *(pmsg+i) = 0x4095;
	    length = 37;
            break;
        default:
            return(-1);
        }
        break;

    case (AB_AO_INTERFACE):
	/* initializing an analog output card includes writing the values */
	/* see also ab_aoread below */
	/* analog output reads the current outputs at initialization */
        if ((*pcard & AB_CARD_TYPE) == AB1771OFE){
	    /* configuration data */
	    *(pmsg+4) = OFE_BINARY | OFE_SCALING;
	    for (i = 6; i <= 12; i+=2) *(pmsg+i) = 0x4095;
	    length = 5;
	    return(bt_queue(AB_READ,link,adapter,card,length,pmsg));
	}else
            return(-1);
        break;
    default:
	return(-1);
    }

    /* initate block tranfer */
    return(bt_queue(AB_WRITE,link,adapter,card,length,pmsg));
}

/*
 * ab_btwrite
 *
 * block transfer - to write data
 */
ab_btwrite(pcard,adapter,card,link)
unsigned short		*pcard;
register unsigned short	adapter;
register unsigned short	card;
register unsigned short	link;
{
	short	msg[64];
	register short	*pmsg = &msg[0];
	register i;
	register short	*pab_table = &ab_btdata[link][adapter][card][0];
	short    btq_err;

	/* clear out the dual ported memory */
	bfill(&msg[0],64*2,0);

        if ((*pcard & AB_CARD_TYPE) == AB1771OFE){
		/* take the data from the Allen-Bradley data table */
		for (i = 0; i < 4; i++, pab_table++)
			*(pmsg+i) = *pab_table;
		/* configuration data */
		*(pmsg+4) = OFE_BINARY | OFE_SCALING;
	    	for (i = 6; i <= 12; i+=2) *(pmsg+i) = 0x4095;

		/* initate block tranfer */
	    	if(btq_err = bt_queue(AB_WRITE,link,adapter,card,5,pmsg)){ 
	    	   if(ab_debug)
		     logMsg("link %x, ab_btwrite error %x\n",link,btq_err);
	    	   return (btq_err);
		}
	}else{
		return(-1);
	}
	return(0);
}

/*
 * bt_queue
 *
 * queue a block transfer request
 */
bt_queue(command,link,adapter,card,length,pmsg)
register unsigned short	command;
register unsigned short link;
register unsigned short	adapter;
unsigned short		card;
unsigned short		length;
unsigned short		*pmsg;
{
	unsigned short *pcard = &ab_config[link][adapter][card];
	register struct ab_region	*p6008 = p6008s[link];
	register struct dp_mbox 	*pmb = (struct dp_mbox *)(&p6008->mail);
	register unsigned short		*pmb_msg = (unsigned short *)&pmb->da;
	register unsigned short		lock_stat,i;
	int				status;
	register unsigned short	command_back;

	/* try to get access to (lock) the dual port memory */
	/* This prevents the Allen-Bradley scanner from accessing the dual */
	/* ported memory while a request is being made.			   */
	for(i=0; i<100; i++) {
	    if ((lock_stat = sysBusTas (&pmb->fl_lock)) == TRUE) break;
	    taskDelay(1);
	}
	if(lock_stat == FALSE) {
		if(ab_debug != 0)
		    logMsg("link %x BTQ-dpm locked on %x cmd\n",
			link,command);
		ab_not_ready[link] += 1;
		return (LOCK);
	}

	/* As the scanner will not be sending us data during this command */
	/* request, we direct the interrupts here and wait for command    */
	/* accepted.							  */
	ab_requesting_bt[link] = TRUE;

	/* clear the timeout flag */
	ab_tout[link] = OK;

	/* copy the message into the dual-ported memory */

	/* clear the semaphore to make sure we stay in sync */
	if((semClear(&ab_cmd_sem) == 0) && ab_debug)
		logMsg("link %x, semaphore set before bt_queue cmd %x,%x,%x\n",
		    link,command,pmb->command,pmb->conf_stat);

	pmb->command = command;
	/* jcr */
	if ((*pcard & AB_PLC)==0) {
		pmb->address = (adapter << 4) + card;
		pmb->data_len = length;
	}else{
		pmb->address = (adapter << 4);	/* always writes to card 0 */
		pmb->data_len = 0;		/* PLCs govern the length  */
		/* used by the PLC to determine where the data comes from  */
		/* the PLC programmers have to use this knowledge fopr the */
		/* interface						   */
		pmsg[62] = card;
	}
	wtrans(pmsg,pmb_msg);

	/* initiate the timeout clock */
	wdStart(wd_id[link],SECOND,ab_timeout,link);
	/* alert the scanner to the new request */
	p6008->sc_intr = 1;

	/* wait for the command to be accepted */
#	ifdef V5_vxWorks
		semTake(&ab_cmd_sem, WAIT_FOREVER);
#	else
		semTake(&ab_cmd_sem);
#	endif

	/* determine if the command was accepted */
	if (ab_tout[link] == ERROR){
		if(ab_debug)
		    logMsg("link %x, BTQ cmd %x...timeout\n",link,command);
		ab_cmd_to[link][adapter][card]++;
		ab_requesting_bt[link] = FALSE;
		/* unlock the dual ported memory */
		pmb->fl_lock = 0;
		return(ERROR);
	}

	/* get the status & command from the dual-ported memory */
	status = pmb->conf_stat;
	command_back = pmb->command;
	if(command_back != command){
		if (ab_debug)
			logMsg("link %x, BTQ cmd %x, resp=%x\n"
			    ,link,command,command_back);
		ab_bad_response[link]++;
		status = ERROR;
	}
	if (status != BT_ACCEPTED){
		if (status == 0x14){
			taskDelay(SECOND/10);
			if(ab_debug >1) 
			    logMsg("link %x BTQ full, delaying...\n",link);
		}else{
			if(ab_debug != 0)
			    logMsg("link %x BTQ failure %x\n",link,status);
		}
	}else
		status = OK;
	/* return the interrupts to the data received state */
	ab_requesting_bt[link] = FALSE;

	/* unlock the dual ported memory */
	pmb->fl_lock = 0;

	/* indicate that a response is queued for this card */
	if(command == AB_READ) ab_btq_cnt[link][adapter][card]++;

	return(0);
}

/*
 * abDoneTask
 *
 * This task handles block transfer data returned from the Allen-Bradley 
 * scan task through the dual ported memory.
 * It needs to acknowledge that a block tranfer card (analog io) has been
 * successfully configured or place the read data into the block transfer
 * memory.
 */
abDoneTask(){
    register short		i,sign_bit,card,plc_card;
    unsigned short		adapter,link;
    register unsigned short	*pcard;		/* ptr to configuration word */
    register short		*pab_table;
    char			*pab_sts;
    struct ab_response		response;
    struct ab_response		*presponse = &response;
    while(TRUE){
	/* wait for block transfer completion */
#	ifdef V5_vxWorks
		semTake(&ab_data_sem, WAIT_FOREVER);
#	else
		semTake(&ab_data_sem);
#	endif

	while (rngBufGet(ab_cmd_q,&response,sizeof(struct ab_response)) 
	  == sizeof(struct ab_response)){
	    link = presponse->link;

	    /* make sure we got a response to a read or write command */
	    if ((presponse->command != AB_READ)
	       &&  (presponse->command != AB_WRITE)){
		ab_rw_resp_err[link]++;
		continue;
	    }

	    /* find whose data this is */
	    /* unpacks the card pack out of the message */
	    /* this codes determines if it is a PLC or  */
	    /* adapter response                         */
	    adapter = presponse->adapter;
	    card = presponse->card;
	    plc_card = presponse->data[62];

    	    /* let's determine if this is a PLC - verify with config table */
	    if ((plc_card <= 16) && (plc_card >= 0)
	      && (ab_config[link][adapter][plc_card] & AB_PLC))
	    	card = plc_card;

	    pcard = &ab_config[link][adapter][card];

	    /* zero counter to indicate that a requested BT was received */
	    ab_btq_cnt[link][adapter][card] = 0;

	    /* block transfer timeout */
	    if (presponse->status == 0x23){
		if (ab_debug >1)
		    logMsg("link %x adapter %x card %x timeout %x\n"
			,link,adapter,card,presponse->status);
		ab_data_to[link][adapter][card]++;
		/* mark the channels on this card as harware error */
		pab_sts = &ab_btsts[link][adapter][card][0];
		for (i=0; i<AB_CHAN_CARD; i++,pab_sts++)
		    *pab_sts -= 1;

		/* mark for reinitialization */
		if (ab_btsts[link][adapter][card][0] <= 0)
			*pcard &= ~(AB_INIT_BIT | AB_SENT_INIT);
		continue;
	    }else if (presponse->status != 0){ 
		if (ab_debug != 0)
			logMsg("L%x A%x C%x, Bad Done Status=%x\n"
			,link,adapter,card,presponse->status);
		continue;
	    }

	    /* was it a response to configuration */
	    if ((*pcard & AB_INIT_BIT) == 0){
		*pcard |= AB_INIT_BIT;
		ab_init_cnt[link][adapter][card] = 0;

		/* analog outputs return values on init */
		if ((*pcard & AB_INTERFACE_TYPE)==AB_AO_INTERFACE)
		{
		    register struct ab1771ofe_write *pmsg
		      = (struct ab1771ofe_write *)presponse->data;
		    pab_table = &ab_btdata[link][adapter][card][0];
		    for (i = 0; i < 4; i++,pab_table++)
			*pab_table = pmsg->data[i];
		}

	    /* it was a response to a command */
	    /* analog input response */
	    }else if ((*pcard & AB_INTERFACE_TYPE) == AB_AI_INTERFACE){
		if ((*pcard & AB_CARD_TYPE) == AB1771IL)
		{
		register struct ab1771il_read *pmsg
		  = (struct ab1771il_read *)presponse->data;
		    pab_table = &ab_btdata[link][adapter][card][0];
		    pab_sts = &ab_btsts[link][adapter][card][0];
		    for (i=0, sign_bit=1; i < ai_num_channels[AB1771IL]; i++, sign_bit>>= 1){
			/* status */
			if((pmsg->urange & sign_bit) || (pmsg->orange & sign_bit)){
			    *pab_sts = -3;
			    ab_scaling_error[link][adapter][card]++;
			}else{
			    *pab_sts = 0;
			    /* data */
			    *pab_table = pmsg->data[i];
			}
			pab_sts++;
			pab_table++;
		    }
		}
		else if ((*pcard & AB_CARD_TYPE) == AB1771IXE)
		{
		register struct ab1771ixe_read *pmsg
		  = (struct ab1771ixe_read *)presponse->data;
    		    pab_table = &ab_btdata[link][adapter][card][0];
		    pab_sts = &ab_btsts[link][adapter][card][0];
		    for (i=0, sign_bit=0x100; i<ai_num_channels[AB1771IXE]; i++, sign_bit<<=1){

			/* status */
			if ((pmsg->out_of_range & sign_bit) 
			  || (pmsg->out_of_range & (sign_bit << 8))){
			    *pab_sts = -3;
			    ab_scaling_error[link][adapter][card]++;
			}else{
			    *pab_sts = 0;
			    /* data */
			    *pab_table = pmsg->data[i];
			}
			pab_sts++;
			pab_table++;
		    }
		}
		else if ((*pcard & AB_CARD_TYPE) == AB1771IFE)
		{
		register struct ab1771ife_read *pmsg
		  = (struct ab1771ife_read *)presponse->data;
		    pab_table = &ab_btdata[link][adapter][card][0];
		    pab_sts = &ab_btsts[link][adapter][card][0];
		    /* check each channel as overrange */
		    for (i=0, sign_bit=0x1; i<ai_num_channels[AB1771IFE]; i++, sign_bit<<=1){

			/* status */
			if (pmsg->urange & sign_bit){
				*pab_sts = -3;
			    ab_scaling_error[link][adapter][card]++;
			}else if(pmsg->orange & sign_bit){
			    *pab_sts = -3;
			    ab_or_scaling_error[link][adapter][card]++;
			}else if ((pmsg->data[i] & 0xf000) > 0){
			    ab_or_scaling_error[link][adapter][card]++;
			}else{
			    *pab_sts = 0;
			    *pab_table = pmsg->data[i];	/* put data in table */
			}
			pab_sts++;
			pab_table++;
		    }
		}
		else if ((*pcard & AB_CARD_TYPE) == AB1771IFE_SE)
		{
		register struct ab1771ife_read *pmsg
		  = (struct ab1771ife_read *)presponse->data;
		    pab_table = &ab_btdata[link][adapter][card][0];
		    pab_sts = &ab_btsts[link][adapter][card][0];
		    /* check each channel as overrange */
		    for (i=0, sign_bit=0x1; i<ai_num_channels[AB1771IFE_SE]; i++, sign_bit<<=1){

			/* status */
			if (pmsg->urange & sign_bit){
				*pab_sts = -3;
			    ab_scaling_error[link][adapter][card]++;
			}else if(pmsg->orange & sign_bit){
			    *pab_sts = -3;
			    ab_or_scaling_error[link][adapter][card]++;
			}else if ((pmsg->data[i] & 0xf000) > 0){
			    ab_or_scaling_error[link][adapter][card]++;
			}else{
			    *pab_sts = 0;
			    *pab_table = pmsg->data[i];	/* put data in table */
			}
			pab_sts++;
			pab_table++;
		    }
		}
		else if ( ((*pcard & AB_CARD_TYPE) == AB1771IFE_4to20MA)
		      ||  ((*pcard & AB_CARD_TYPE) == AB1771IFE_0to5V  )   )
		{
		register struct ab1771ife_read *pmsg
		  = (struct ab1771ife_read *)presponse->data;
		    pab_table = &ab_btdata[link][adapter][card][0];
		    pab_sts = &ab_btsts[link][adapter][card][0];
		    /* check each channel as overrange */
		    for (i=0, sign_bit=0x1;
		    i<ai_num_channels[AB1771IFE_4to20MA]; i++, sign_bit<<=1){

			/* status */
			if (pmsg->urange & sign_bit){
				*pab_sts = -3;
			    ab_scaling_error[link][adapter][card]++;
			}else if(pmsg->orange & sign_bit){
			    *pab_sts = -3;
			    ab_or_scaling_error[link][adapter][card]++;
			}else if ((pmsg->data[i] & 0xf000) > 0){
			    ab_or_scaling_error[link][adapter][card]++;
			}else{
			    *pab_sts = 0;
			    *pab_table = pmsg->data[i];	/* put data in table */
			}
			pab_sts++;
			pab_table++;
		    }
		}
           /* analog output response */
            }else if ((*pcard & AB_CARD_TYPE) == AB1771OFE){ 
	           *pcard = *pcard & (~AB_UPDATE); /* mark as updated */
	    }
	}
    }
}


/*
 * AB_BI_COS_SIMULATOR
 *
 * simulate a change of state interrupt from the Allen-Bradley
 */
unsigned char		ab_old_binary_ins[AB_MAX_LINKS*AB_MAX_ADAPTERS*AB_MAX_CARDS];
ab_bi_cos_simulator()
{
        register struct ab_region       *p6008;
        register unsigned short 	card_inx,link;
	register unsigned short		*pcard;
	unsigned short			*ps_input,*ps_oldval;
	short				adapter,card,inpinx;

	for(;;){
		/* check each link */
		link = 0;
		for (link = 0; link < AB_MAX_LINKS; link++){
		    	if ((p6008 = p6008s[link]) == 0) continue;

			/* check each card that is configured to be a binary input card */
			pcard = &ab_config[link][0][0];
			for (card_inx = 0; card_inx < AB_MAX_ADAPTERS*AB_MAX_CARDS; card_inx++,pcard++){
				if ((*pcard & AB_INTERFACE_TYPE) != AB_BI_INTERFACE) continue;
					
				if ((*pcard & AB_CARD_TYPE) == ABBI_16_BIT){
					/* sixteen bit byte ordering in dual ported memory 	*/
					/* byte 0011 2233 4455 6677 8899 AABB			*/
					/* card 0000 2222 4444 6666 8888 AAAA			*/
					if (card_inx & 0x1)	continue;
					ps_input = (unsigned short *)&(p6008->iit[card_inx]);
					ps_oldval = (unsigned short *)&(ab_old_binary_ins[card_inx]);
					if (*ps_input != *ps_oldval){
						adapter = card_inx / AB_MAX_CARDS;
						card = card_inx - (adapter * AB_MAX_CARDS);
						io_event_scanner_wakeup(IO_BI,ABBI_16_BIT,card,link,adapter);
						*ps_oldval = *ps_input;
					}
				}else{
					/* eight bit byte ordering in dual ported memory */
					/* 1100 3322 5544 7766 9988 BBAA 		 */
					inpinx = card_inx;
					if (inpinx & 0x1)	inpinx--;	/* shuffle those bytes */
					else inpinx++;
					if (p6008->iit[inpinx] != ab_old_binary_ins[inpinx]){
						adapter = card_inx / AB_MAX_CARDS;
						card = card_inx - (adapter * AB_MAX_CARDS);
						io_event_scanner_wakeup(IO_BI,ABBI_08_BIT,card,link,adapter);
						ab_old_binary_ins[inpinx] = p6008->iit[inpinx];
					}
				}
			}
		}

		/* check for changes at about 15 Hertz */
		taskDelay(SECOND/15);
	}
}

/*
 * ALLEN-BRADLEY DRIVER INITIALIZATION CODE
 */
ab_driver_init()
{
	unsigned short		cok,got_one;
	register unsigned short	err_cnt;
	register unsigned short	not_init;
	register unsigned short	link;
	struct ab_region	*p6008;
        int status;

	/* initialize the Allen-Bradley 'done' semaphore */
	if (abScanId == 0){
		semInit(&ab_cmd_sem);
		semInit(&ab_data_sem);
		if ((ab_cmd_q = rngCreate(AB_Q_SZ)) == (RING_ID)NULL)
		    panic ("ab_init: ab_cmd_q\n");
	}
	got_one = FALSE;
        if ((status = sysBusToLocalAdrs(VME_AM_STD_SUP_DATA,AB_BASE_ADDR,&ab_stdaddr)) != OK){
           logMsg("Addressing error in ab driver\n");
           return ERROR;
        }
        p6008 =(struct ab_region *)ab_stdaddr; 
	for (link=0;link < AB_MAX_LINKS; link++,p6008++){
		/* initialize the AB config buffer for this link */
		bfill(&ab_config[link][0][0],
		  AB_MAX_ADAPTERS*AB_MAX_CARDS*sizeof(short),
		  0);
		bfill(&ab_btdata[link][0][0][0],
		  AB_MAX_ADAPTERS*AB_MAX_CARDS*AB_CHAN_CARD*sizeof(short),
		  0);

		/* initialize each 6008 that exists */
		if (vxMemProbe(p6008,READ,1,&cok) != ERROR){
			p6008s[link] = p6008;
			err_cnt = 0;
			not_init = TRUE; 
			/* create the internal watchdog timer */
			if (wd_id[link] == 0)
				wd_id[link] = wdCreate(); 

			/*connect intr service routine to intr vector */
			intConnect((AB_VEC_BASE+link)*4,ab_intr,link);
                        sysIntEnable(AB_INT_LEVEL); 

			while ((err_cnt < 5) && (not_init)){
				if (link_init(link) >=0) not_init = FALSE;
				else {
					err_cnt++;
					logMsg("link %x, link_init error\n",link);
				}
			}
			if (err_cnt >= 5){
				p6008s[link] = 0;
			}else{
				got_one = TRUE;
			} 
		}else{
			p6008s[link] = 0;
		}
	}

	if (got_one){ 

		/* spawn the scan task to handle block transfer requests */
		if (abScanId) td(abScanId);
        	abScanId = taskSpawn(ABSCAN_NAME,ABSCAN_PRI,ABSCAN_OPT,
					ABSCAN_STACK,abScanTask);

		 /* spawn the done task to handle block transfer responses */
		if (abDoneId) td(abDoneId);
		abDoneId = taskSpawn(ABDONE_NAME,ABDONE_PRI,ABDONE_OPT,
					ABDONE_STACK,abDoneTask);

		/* spawn the ab change of state interrupt simulator */
		if (abCOSId) td(abCOSId);
		abCOSId = taskSpawn(ABCOS_NAME,ABCOS_PRI,ABCOS_OPT,ABCOS_STACK,ab_bi_cos_simulator);
	}
}

/*
 * link_init
 *
 * establish the communication link with the AB scanner
 */
link_init(link)
register short	link;
{
	register struct ab_region	*p6008 = p6008s[link];
	char	buffer[80];
	char	*pbuffer = &buffer[0];
	register short	length;
	short mr_w_err,i;
	register struct dp_mbox *pmb = (struct dp_mbox *)(&p6008->mail);
	char				*pfirmware_info = &ab_firmware_info[link][0];

	ab_bad_response[link]=0;
	ab_rw_resp_err[link]=0;

	/* the scanner comes up with the dual ported memory locked */
	pmb->fl_lock = 0;		/* so unlock it */

	/* initialize the scanner on power up */
	if (pmb->conf_stat == SCANNER_POWERUP){
		logMsg("link %x, powerup...\n",link);
		/* on initialization the scanner puts its firmware revision info	*/
		/* into the general data are of the dual-port.  We save it here. 	*/
		/* (The most current revision is Series A, Revision D.)			*/
		strcpy(pfirmware_info,&pmb->da);
		if(ab_debug != 0)
			logMsg("link %x f/w = :\n\t%s\n",link,ab_firmware_info[link]);
		/*scanner comes up in program_mode */
		ab_op_stat[link] = PROGRAM_MODE;
		/*interrupt the scanner to wake it up */
		p6008->sc_intr = 1;
	}

	logMsg("Link %x, initial operating status word = %x\n",link,ab_op_stat[link]);	

	if((ab_op_stat[link] & PROGRAM_MODE) != PROGRAM_MODE){
		/* This link must already be initialized.  We're done */
		logMsg("Link %x already initialized\n",link);
		return;
	}

	/* setup scanner */
	bfill(pbuffer,80,0);
	*(pbuffer) = DEF_RATE;	/* baud rate */
	*(pbuffer+2) = DEBUG;	/* always in debug mode - don't know why */
	*(pbuffer+3) = AB_IL;	/* interrupt level */
	*(pbuffer+4) = AB_VEC_BASE+link;	/* interrupt vector */
	*(pbuffer+5) = AB_INT_ENABLE;		/* enable interrupt */
	*(pbuffer+6) = AB_SYSFAIL_DISABLE; 	/* disable VMEbus SYSFAIL sig */
	length = 7;
	if((mr_w_err = mr_wait(SET_UP,length,pbuffer,link)) != OK){
		if (ab_debug != 0)
			logMsg("link %x link_init set_up cmd - mr_wait error %x\n"
			,link,mr_w_err);
		return(-1);
	}

	/* Once scanner has been placed in RUN_MODE, putting it back into	*
	 * PROGRAM_MODE will disable binary outputs until it is placed back in	*
	 * RUN_MODE.  Some scanner commands, such as SCAN_LIST, can only be	*
	 * performed in PROGRAM_MODE.  These commands should only be issued	*
	 * immediately after initialization.  Re-booting an IOC (without powering
	 * it down) is the presently the only way of getting it into PROGRAM_MODE
	 * without disabling binary outputs */

	/* initialize scan list for each link present */
	if(scan_list(link) != OK){
		logMsg("  scan_list error on link %x\n",link);
		return(-1);
	}

	return(0);
}

/*
 *  MR_WAIT
 *
 * scanner management command send
 */
mr_wait(command,length,pmsg,link)
register short	command;
register short	length;
short		*pmsg;
unsigned short	link;
{
	register struct ab_region	*p6008 = p6008s[link];
	register struct dp_mbox 	*pmb = (struct dp_mbox *)&p6008->mail;
	register short 			*pmb_msg, i, lock_stat;
	short				rtn_val;
	
	/* pointer to the message portion of the mailbox */
	pmb_msg = (short *)&pmb->da;

	/* try to get access to (lock) the dual port memory */
	for(i=0; i<100; i++) {
	    if ((lock_stat = sysBusTas (&pmb->fl_lock)) == TRUE) break;
	    taskDelay(1);
	}
	if(lock_stat == FALSE) {
		if(ab_debug != 0)
		    logMsg("link %x mr_wait-dpm locked on %x cmd\n",
			link,command);
		return (LOCK);
	}
	/* reset timeout */
	ab_tout[link] = OK;


	/* clear the semaphore to make sure we stay in sync */
	if((semClear(&ab_cmd_sem) == 0) && ab_debug)
		logMsg("link %x, semaphore set before mr_wait cmd %x,%x,%x\n",
		    link,command,pmb->command,pmb->conf_stat);

	/* process commands */
	pmb->command = command;
	pmb->data_len = length;
	rtn_val = OK; 			/* assume success */
	switch (command) {

	/* commands with response */
	case AUTO_CONF:
	case LINK_STATUS:
		wdStart(wd_id[link],SECOND*10,ab_timeout,link);
		p6008->sc_intr = 1;	/* wakeup scanner tsk */
#ifdef		V5_vxWorks
			semTake(&ab_cmd_sem, WAIT_FOREVER);
#		else
			semTake(&ab_cmd_sem);
#		endif
		/* was this a timeout? */
		if(ab_tout[link] == ERROR){
		    if(ab_debug != 0)
			logMsg("link %x mr_wait - timeout on %x cmd\n"
			    ,link,command);
		    rtn_val = ERROR;
		    break;
		}
		if ((pmb->conf_stat != 0) && (ab_debug != 0)){
			logMsg("link %x mr_wait cmd %x failed, status=%x\n"
			  ,link,pmb->command,pmb->conf_stat);
			rtn_val = ERROR;
			break;
		}
		if (pmb->command == command){
		    wtrans(pmb_msg,pmsg);	/* xfer data to local mailbox */
		}else{
		    ab_bad_response[link]++;
		    pmb->fl_lock = 0;
		    if(ab_debug != 0)
			logMsg("link %x mr_wait - bad resp on %x cmd\n"
			    ,link,command);
		    rtn_val = ERROR;
		}
		break;

	/* commands which send data */
	case SCAN_LIST:
	case SET_MODE:
	case SET_UP:
		wtrans(pmsg,pmb_msg);		/* xfer data to local mailbox */
		wdStart(wd_id[link],SECOND*5,ab_timeout,link);
		p6008->sc_intr = 1;	/* wakeup scanner */
#		ifdef V5_vxWorks
			semTake(&ab_cmd_sem, WAIT_FOREVER);
#		else
			semTake(&ab_cmd_sem);
#		endif
		/* was this a timeout? */
		if(ab_tout[link] == ERROR){
		    if(ab_debug != 0)
			logMsg("link %x mr_wait - timeout on %x cmd\n"
			    ,link,command);
		    rtn_val = ERROR;
		    break;
		}
		if ((pmb->conf_stat != 0) && (ab_debug != 0)){
			logMsg("link %x mr_wait cmd %x failed status %x\n"
			  ,link,pmb->command,pmb->conf_stat);
			rtn_val = ERROR;
		}
		break;
	}
	/* unlock the dual port memory */
	pmb->fl_lock = 0;
	return (rtn_val);
}

int	ab_intr_cnt[AB_MAX_LINKS];
/*
 * AB_INTR
 *
 * interrupt service routine
 *
 * The Allen-Bradley protocol requires that an interrupt be received when
 * a block transfer request is given to the scanner board through the dual
 * ported memory and then another when the command is complete. As the command
 * accept interrupt occurs while the Allen-Bradley scanner is locked out,
 * there is no danger of getting a data interrupt back. The documentation is
 * very misleading in this area: It gives no indication of how the interrupts
 * are used to accomplish asynchronous communication and implies that the 
 * dual-ported memory lock is controlled in a much different fashion than it is.
 */
ab_intr(link)
short		link;
{
	struct	ab_response		response;
	register struct ab_response	*presponse;
	register unsigned short		*presp_data;
	register unsigned short		*pdata;
	register struct dp_mbox		*pmb;
	register unsigned short		i;

	register short 		tmp_op_stat;
	register struct ab_region	*p6008 = p6008s[link];

	    pmb = &p6008s[link]->mail;

    /* Test the operating status word of link to see if adapter		  *
     * status has changed since the last time the scanner interrupted us. */
	tmp_op_stat = (p6008->osw & OSW_MASK);
	if (tmp_op_stat != ab_op_stat[link]){
/*		logMsg("old stat %x, new stat %x\n",ab_op_stat[link], tmp_op_stat);  */
		ab_op_stat[link] = tmp_op_stat;
	}

	ab_intr_cnt[link]++;
	if (((pmb->command != AB_WRITE) && (pmb->command != AB_READ)) || 
	    (ab_requesting_bt[link])){
		semGive(&ab_cmd_sem);
		wdCancel(wd_id[link]);
	}else{
	    presponse = &response;

	    /* wait for the genius, who posted us, to lock the data area */
	    while((pmb->fl_lock == 0) && (ab_post_no_lock[link] < 1000))
		ab_post_no_lock[link] += 1;
	    if (ab_post_no_lock[link] >= 1000){
		logMsg("link %x exceeded stop count\n",link);
	        ab_post_no_lock[link] = 0;
		return;
	    }

	    /* put the response on the queue for abDoneTask */
	    presponse->link = link;
	    presponse->card = pmb->address & 0x0f;
	    presponse->adapter = (pmb->address & 0x70) >> 4;
	    presponse->command = pmb->command;
	    presponse->status = pmb->conf_stat;
	    presp_data = &presponse->data[0];
	    pdata = &pmb->da.wd[0];
	    for (i = 0; i < 64; i++,pdata++,presp_data++)
	    	*presp_data = *pdata;
	    if (rngBufPut(ab_cmd_q,&response,sizeof(struct ab_response))
	      != sizeof(struct ab_response))
		logMsg("ab_cmd_q - Full\n");

	    /* wake up abDoneTask */
	    semGive(&ab_data_sem);
	    pmb->fl_lock = 0;
	}
}

/*
 * AB_TIMEOUT
 *
 * internal watchdog timer
 */
ab_timeout(link)
int	link;
{
	/* mark a timeout error */
	ab_tout[link] = ERROR;

	/* only the command receive gives a damn */
	semGive(&ab_cmd_sem);

	/* keep track in case someone wants to snoop */
	ab_comm_to[link]++;
}
static void wtrans (from, to)
register short *from, *to;
{
	register i;
	for (i=0;i<64;i++)
		*to++ = *from++;
}

/*
 * AB_AIDRIVER
 *
 * data request from the Analog Input driver
 */
ab_aidriver(card_type,link,adapter,card,signal,adapter_plc,pvalue,conversion)
unsigned short 		card_type;
register unsigned short	link;
register unsigned short adapter;
register unsigned short card;
unsigned short          signal;
unsigned short 		*pvalue;
unsigned short 		conversion;
unsigned short		adapter_plc;
{
	/* pointer to the Allen-Bradley configuration table */
        register unsigned short *pcard = &ab_config[link][adapter][card];

        /* verify that the card is initialized */
        if ((*pcard & AB_INIT_BIT) == 0){ 
	    if ((*pcard & AB_SENT_INIT) == 0){
		/* configuration data - the scan task takes over from here */
                *pcard = AB_AI_INTERFACE | card_type
                  | ((conversion << 8) & AB_CONVERSION);
		if (adapter_plc) *pcard |= AB_PLC;
	    }
	    /* sorry, but the initalization is not immediate */
            return(-1);
        }

       /* verify that link is ok and card type is correct */
	if (ab_adapter_status[link][adapter] == ERROR) return(-1);
        if ((*pcard & AB_INTERFACE_TYPE) != AB_AI_INTERFACE) return(-1);
        if ((*pcard & AB_CARD_TYPE) != card_type) return(-1);

        /* get the value from the table */
        *pvalue = ab_btdata[link][adapter][card][signal];
        if (ab_btsts[link][adapter][card][signal] < -2) return(-1);
        return(0);
}

/*
 * AB_AODRIVER
 *
 * data send from the Analog Output driver
 */
ab_aodriver(card_type,link,adapter,card,signal,adapter_plc,value)
unsigned short		card_type;
register unsigned short	link;
register unsigned short adapter;
register unsigned short card;
unsigned short          signal;
unsigned short		adapter_plc;
unsigned short          value;
{
        register unsigned short *pcard = &ab_config[link][adapter][card];

        /* verify that the card is initialized */
        if ((*pcard & AB_INIT_BIT) == 0){
	    if ((*pcard & AB_SENT_INIT) == 0){
 		/* configuration data - the scan task takes over from here */
		/* we assume the write will work and return 0 */
                *pcard = AB_AO_INTERFACE | card_type | AB_UPDATE;
		if (adapter_plc) *pcard |= AB_PLC;

		/* put the value into the table */
		ab_btdata[link][adapter][card][signal] = value;
	    }
	    return(-1);
        }

        /* verify that link is ok and card type is correct */
	if (ab_adapter_status[link][adapter] == ERROR) return(-1);
	if ((*pcard & AB_INTERFACE_TYPE) != AB_AO_INTERFACE) return(-1);
        if ((*pcard & AB_CARD_TYPE) != card_type) return(-1);

        /* put the value into the table */
	if (ab_btdata[link][adapter][card][signal] == value) return(0);
        ab_btdata[link][adapter][card][signal] = value;
	*pcard |= AB_UPDATE;

        return(0);
}

/*
 * AB_AOREAD
 *
 * read the analog output value
 *	called from read_ao in ../../dblib/src/iocao.c 
 *	used to initialize AOs
 */
ab_aoread(card_type,link,adapter,card,signal,adapter_plc,pvalue)
register unsigned short card_type;
unsigned short          link;
register unsigned short adapter;
register unsigned short card;
unsigned short          signal;
unsigned short		adapter_plc;
unsigned short          *pvalue;
{
        register unsigned short *pcard = &ab_config[link][adapter][card];

        if ((*pcard & AB_INIT_BIT) == 0){
	    /* add adapter to the scan list if neccessary */
	    if ((*pcard & AB_SENT_INIT) == 0){
		if (*pcard == 0){
 			/* configuration data-scan task takes over from here */
			*pcard = AB_AO_INTERFACE | card_type;
			if (adapter_plc) *pcard |= AB_PLC;
		}
	    }
	    return(-1);
        }
	*pvalue = ab_btdata[link][adapter][card][signal];
	return(0);
}

/*
 * AB_BIDRIVER
 *
 * data read from the Binary Input driver
 */
ab_bidriver(card_type,link,adapter,card,adapter_plc,mask,pvalue)
unsigned short          card_type;
register unsigned short	link;
register unsigned short adapter;
register unsigned short card;
unsigned short		adapter_plc;
unsigned long		*pvalue;
unsigned long           mask;
{
        register struct ab_region       *p6008 = p6008s[link];
        register unsigned short *pcard = &ab_config[link][adapter][card];
        register unsigned short inpinx;
	register unsigned short	*pshort;
	/* verify there is a scanner */
        if (p6008 == 0) return(-1);

	/* control-x has been used to restart - don't access the AB card */
	if (ab_disable) return(-1);

        /* claim the card as a binary input */
        if (((*pcard & AB_INIT_BIT) == 0) && ((*pcard & AB_SENT_INIT) == 0)){
            *pcard = AB_BI_INTERFACE | card_type | AB_INIT_BIT;
	    if (adapter_plc) *pcard |= AB_PLC;
	}

	/* verify link communication is OK */
	/* This doesn't work in a box with no analog IO */
	if (ab_adapter_status[link][adapter] == ERROR) return(-1);

	/* eight bit byte ordering in dual ported memory */
	/* 1100 3322 5544 7766 9988 BBAA 		 */
	if (card_type == ABBI_08_BIT){
		/* eight bit byte ordering in dual ported memory 	*/
		/* byte 1100 3322 5544 7766 9988 BBAA			*/
		inpinx =  (adapter * AB_CARD_ADAPTER) + card;
		if (card & 0x1)	inpinx--;	/* shuffle those bytes */
		else inpinx++;
		*pvalue = p6008->iit[inpinx] & mask;
	}else{
		/* sixteen bit byte ordering in dual ported memory 	*/
		/* byte 0011 2233 4455 6677 8899 AABB			*/
		/* card 0000 2222 4444 6666 8888 AAAA			*/
		inpinx =  (adapter * AB_CARD_ADAPTER) + card;
		if (card & 0x1)	return(-1);
		pshort = (unsigned short *)&(p6008->iit[inpinx]);
		*pvalue = *pshort & mask;
	}
	return(0);
}

/*
 * AB_BODRIVER
 *
 * data send from the Binary Output driver
 */
ab_bodriver(card_type,link,adapter,card,adapter_plc,value,mask)
unsigned short          card_type;
register unsigned short	link;
unsigned short 		adapter;
register unsigned short card;
unsigned short		adapter_plc;
unsigned long		value;
unsigned long           mask;
{
	register struct ab_region       *p6008 = p6008s[link];
	register unsigned short *pcard = &ab_config[link][adapter][card];
	register unsigned short outinx;
	register unsigned short	*pshort;

	/* verify there is a scanner */
        if (p6008 == 0) return(-1);

	/* control-x has been used to restart - don't access the AB card */
	if (ab_disable) return(-1);

	/* claim the card as a binary input */
        if (((*pcard & AB_INIT_BIT) == 0) && ((*pcard & AB_SENT_INIT) == 0)){
	    *pcard = AB_BO_INTERFACE | card_type | AB_INIT_BIT;
	    if (adapter_plc) *pcard |= AB_PLC;
	}

	/* verify link communication is OK */
	/* This doesn't work in a box with no analog IO */
	if (ab_adapter_status[link][adapter] == ERROR) return(-1);

	/* eight bit byte ordering in dual ported memory */
	/* 1100 3322 5544 7766 9988 BBAA 		 */
	if (card_type == ABBO_08_BIT){
		/* eight bit byte ordering in dual ported memory 	*/
		/* byte 1100 3322 5544 7766 9988 BBAA			*/
		outinx =  (adapter * AB_CARD_ADAPTER) + card;
		if (card & 0x1)	outinx--;	/* shuffle those bytes */
		else outinx++;
		p6008->oit[outinx] = (p6008->oit[outinx] & ~mask) | value;
	}else{
		/* sixteen bit byte ordering in dual ported memory 	*/
		/* byte 0011 2233 4455 6677 8899 AABB			*/
		/* card 1111 3333 5555 7777 9999 BBBB			*/
		outinx =  (adapter * AB_CARD_ADAPTER) + card;
		if (card & 0x1)	outinx--;	/* shuffle those bytes */
		else 		return(-1);
		pshort = (unsigned short *)&(p6008->oit[outinx]);
		*pshort = (*pshort & ~mask) | value;
	}
	return(0);
}
/*
 * AB_BOREAD
 *
 * read the binary output channel
 */
ab_boread(card_type,link,adapter,card,pvalue,mask)
unsigned short          card_type;
unsigned short          link;
register unsigned short adapter;
register unsigned short card;
register unsigned long  *pvalue;
unsigned long           mask;
{
	register struct ab_region       *p6008 = p6008s[link];
	register unsigned short outinx;
	register unsigned short	*pshort;

	/* verify there is a scanner */
        if (p6008 == 0) return(-1);

	/* control-x has been used to restart - don't access the AB card */
	if (ab_disable) return(-1);

	/* verify link communication is OK */
	/* This doesn't work in a box with no analog IO */
	if (ab_adapter_status[link][adapter] == ERROR) return(-1);

	/* eight bit byte ordering in dual ported memory */
	/* 1100 3322 5544 7766 9988 BBAA 		 */
	if (card_type == ABBO_08_BIT){
		/* eight bit byte ordering in dual ported memory 	*/
		/* byte 1100 3322 5544 7766 9988 BBAA			*/
		outinx =  (adapter * AB_CARD_ADAPTER) + card;
		if (card & 0x1)	outinx--;	/* shuffle those bytes */
		else outinx++;
		*pvalue = p6008->oit[outinx] & mask;
	}else{
		/* sixteen bit byte ordering in dual ported memory 	*/
		/* byte 0011 2233 4455 6677 8899 AABB			*/
		/* card 1111 3333 5555 7777 9999 BBBB			*/
		outinx =  (adapter * AB_CARD_ADAPTER) + card;
		if (card & 0x1)	outinx--;	/* shuffle those bytes */
		else 		return(-1);
		pshort = (unsigned short *)(&p6008->oit[outinx]);
		*pvalue = *pshort & mask;
	}
	return(0);
}

/*
 * LINK_STATUS
 *
 * Fetches the status of the adapters on the specified link
 * The ab_adapter_status table is used to determine hardware communication
 * errors and convey them to the database
 */
link_status(link)
unsigned short	link;
{
        short		buffer[200];
        register short	*pbuffer = &buffer[0];
	register short	i, mr_w_err;

	/* for no card present - mark all adapters as offline */
	if (p6008s[link] == 0){
		for (i = 0; i < AB_MAX_ADAPTERS; i++){
			ab_adapter_status[link][i] = -1;
		}
		return(0);
	}

	/* get the link status */
	if((mr_w_err = mr_wait(LINK_STATUS,0,pbuffer,link)) != OK){
		if (ab_debug != 0)
		    logMsg("  link_status cmd error\n");
                ab_link_to[link]++;
		return(-1);
        }else{
		/* check each adapter on this link */
		for (i = 0; i< AB_MAX_ADAPTERS; i++){
			/* good status */
			if (*(pbuffer+(i*4)) & 0x70){
				if (ab_adapter_status[link][i] == -1) {
			            printf("link %d adapter %d change bad to good\n"
				      ,link,i);
				}
				ab_adapter_status[link][i] = 0;
			/* bad status */
			}else {
				if (ab_adapter_status[link][i] == 0){
				    printf("link %d adapter %d change good to bad\n"
					  ,link,i);
				}
				ab_adapter_status[link][i] = -1;
			}
		}
        }
	return(0);
}

/*
 * SCAN_LIST
 *
 * sets the scan list for the Allen-Bradley scanner
 */
scan_list(link)
unsigned short	link;
{
	register short	i,sl_inx,fail,mr_w_err;
	char	buffer[80];
	register char	*pbuffer = &buffer[0];

	/* for no card present - mark all adapters as offline */
	if (p6008s[link] == 0)	return(0);


	/* A promised firmware change will allow us to RESET
	   the scanner over the vmeBus.  For now, the only way
	   to get the scanner into prog-mode without having
	   the BO's glitch is with the vmeBus SYSRESET signal,
	   which occurs when the RESET switch on the VME chassis 
	   is used.  Until the f/w change is made, changing the
	   scanner from run mode to program mode (to modify the
	   scan list, for instance) will cause the BO's to turn 
	   off until the scanner is returned to run mode.  It's 
	   not nice, but for now we'll have to assume that all
	   adapters are needed and put them all in the scan list. */

	/* build the scan list  */
	bfill(&ab_scan_list[link][0],AB_MAX_ADAPTERS,0);
	for (sl_inx = 0, i = 0; i < AB_MAX_ADAPTERS; i++){
			ab_scan_list[link][sl_inx] = (i << 2);
			sl_inx++;
	}

	/* set new scan list - assume we're in program mode */
	if((mr_w_err = mr_wait(SCAN_LIST,sl_inx,&ab_scan_list[link][0],link)) != OK){
		if (ab_debug != 0)
			logMsg("  scan_list SCAN_LIST cmd error\n");
		return(-1);
       	}


	/* place the scanner into run mode */
	bfill(pbuffer,80,0);
	*pbuffer = RUN_MODE;
	if((mr_w_err = mr_wait(SET_MODE,0,pbuffer,link)) != OK){
		if (ab_debug != 0)
			logMsg("  scan_list SET_MODE cmd error\n");
		return(-1);
	}
	return(0);
}

#define  MAX_8BIT 8
#define  IOR_MAX_COLS 4

ab_io_report(level)
    short int level;
  {
    
	short	i,j,k,l,m,card,adapter,plc_card,x,type;
        unsigned short jrval,krval,lrval,mrval;
        unsigned long val,jval,kval,lval,mval;
        extern masks[];
        
	/* report all of the Allen-Bradley Serial Links present */
	for (i = 0; i < AB_MAX_LINKS; i++){
	    if (p6008s[i])
		printf("AB-6008SV: Card %d\tcto: %d lto: %d badres: %d\n"
		  ,i,ab_comm_to[i],ab_link_to[i],ab_bad_response[i]);
	    else continue;

	    /* report all cards to which the database has interfaced */
	    /* as there is no way to poll the Allen-Bradley IO to      */
	    /* determine which card is there we assume that any interface */
	    /* which is successful implies the card type is correct       */
	    /* since all binaries succeed and some analog inputs will     */
	    /* succeed for either type this is a shakey basis             */
	    for (adapter = 0; adapter < AB_MAX_ADAPTERS; adapter++){
		for (card = 0; card < AB_MAX_CARDS; card++){
                    
                    /* Determine whether the card is in a plc or not. */

                    if(ab_config[i][adapter][card] & AB_PLC){
                       plc_card = 1;
                    /*   printf("This card is a plc card.\n"); */ 
                    } else {
                       plc_card = 0;
                    /*   printf("This card is not a plc card.\n"); */ 
                    } 

		    switch (ab_config[i][adapter][card] & AB_INTERFACE_TYPE){

		    case (AB_BI_INTERFACE):
			printf("\tAdapter %d Card %d:\tBI",adapter,card);

			if (level > 0)
				ab_bi_report(i,adapter,card,plc_card);
			break;
		    case (AB_BO_INTERFACE):
			printf("\tAdapter %d Card %d:\tBO",adapter,card);
                          if(level > 0)
                               ab_bo_report(i,adapter,card,plc_card);
			    break;
		    case (AB_AI_INTERFACE):
                        type = ab_config[i][adapter][card]&AB_CARD_TYPE;    
			if ((ab_config[i][adapter][card]&AB_CARD_TYPE)==AB1771IXE){
		          printf("\tAdapter %d Card %d:\tAB1771IXE\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
                              if (level > 0){
                                printf("\n");
                              	ab_ai_report(type,i,adapter,card,plc_card);
                              }
                        } else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IL){
			    printf("\tAdapter %d Card %d:\tAB1771IL\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
                              if (level > 0){
                                printf("\n");
                              	ab_ai_report(type,i,adapter,card,plc_card);
                              }
                        } else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IFE_SE){
			    printf("\tAdapter %d Card %d:\tAB1771IFE_SE\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
                              if (level > 0){
                                printf("\n");
                              	ab_ai_report(type,i,adapter,card,plc_card);
                              }
                        } else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IFE_4to20MA){
			    printf("\tAdapter %d Card %d:\tAB1771IFE_4to20MA\tcto: %d dto: %d sclerr: %d %d",
			      adapter,card,ab_cmd_to[i][adapter][card],
			      ab_data_to[i][adapter][card],
			      ab_scaling_error[i][adapter][card],
			      ab_or_scaling_error[i][adapter][card]);
                              if (level > 0){
                                printf("\n");
                              	ab_ai_report(type,i,adapter,card,plc_card);
                              }
                        } else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IFE){
			      printf("\tAdapter %d Card %d:\tAB1771IFE\tcto: %d dto: %d sclerr: %d %d",
			         adapter,card,ab_cmd_to[i][adapter][card],
			         ab_data_to[i][adapter][card],
			         ab_scaling_error[i][adapter][card],
			         ab_or_scaling_error[i][adapter][card]);
                              if (level > 0){
                                printf("\n");
                              	ab_ai_report(type,i,adapter,card,plc_card);
                              }
                        } else if ((ab_config[i][adapter][card] & AB_CARD_TYPE) == AB1771IFE_0to5V){
			      printf("\tAdapter %d Card %d:\tAB1771IFE_0to5V\tcto: %d dto: %d sclerr: %d %d",
			         adapter,card,ab_cmd_to[i][adapter][card],
			         ab_data_to[i][adapter][card],
			         ab_scaling_error[i][adapter][card],
			         ab_or_scaling_error[i][adapter][card]);
                              if (level > 0){
                                printf("\n");
                              	ab_ai_report(type,i,adapter,card,plc_card);
                              }
                         } 
			break;
		    case (AB_AO_INTERFACE):
			printf("\tAdapter %d Card %d:\tAB1771OFE\tcto: %d dto: %d",
			  adapter,card,ab_cmd_to[i][adapter][card],
			  ab_data_to[i][adapter][card]);
                              if (level > 0 ){
                                printf("\n");
                             	ab_ao_report(AB1771OFE,i,adapter,card,plc_card,&jrval,0);
                              }
			break;
		    default:
			continue;
		    }
		    if ((ab_config[i][adapter][card] & AB_INIT_BIT) == 0)
			printf(" NOT INITIALIZED\n");
		    else {
                        printf("\n");
                    }  
		}
	    }
	}

 }
/*  ab_bi_report. 
*   Reports  the raw value of all Allen Bradley Binary In cards. 
*
*
*/
ab_bi_report(link,adapter,card,plc_card)
   short link,adapter,card,plc_card;
  {
	short	i,j,k,l,m,x,num_chans;
        unsigned short type;
        unsigned long jval,kval,lval,mval;
        extern masks[];

        printf("\n");

        type = ab_config[link][adapter][card] & AB_CARD_TYPE;
  
        if(type == ABBI_08_BIT) 
             num_chans = MAX_8BIT; 
        else
             num_chans = AB_CHAN_CARD;

        for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
            j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,m += IOR_MAX_COLS){
        	if(j < num_chans){
                	ab_bidriver(type,link,adapter,card,plc_card,masks[j],&jval);
                 	if (jval != 0) 
                  		 jval = 1;
                         printf("Chan %d = %x\t ",j,jval);
                }  
         	if(k < num_chans){
         		ab_bidriver(type,link,adapter,card,plc_card,masks[k],&kval);
                        if (kval != 0) 
                        	kval = 1;
                        	printf("Chan %d = %x\t ",k,kval);
                }
                if(l < num_chans){
                	ab_bidriver(type,link,adapter,card,plc_card,masks[l],&lval);
                	if (lval != 0) 
                        	lval = 1;
                	printf("Chan %d = %x \t",l,lval);
                 }
                  if(m < num_chans){
                 	ab_bidriver(type,link,adapter,card,plc_card,masks[m],&mval);
                 	if (mval != 0) 
                        	mval = 1;
                 	printf("Chan %d = %x \n",m,mval);
                   }
             }
  }

/*  ab_bo_report. 
*   Reports  the raw value of all Allen Bradley Binary Out cards. 
*
*
*/
ab_bo_report(link,adapter,card)
   short link,adapter,card;
  {
	short	i,j,k,l,m,x,num_chans;
        unsigned short type;
        unsigned long jval,kval,lval,mval;
        extern masks[];

        printf("\n");

        type = ab_config[link][adapter][card] & AB_CARD_TYPE;
  
        if(type == ABBO_08_BIT) 
             num_chans = MAX_8BIT; 
        else
             num_chans = AB_CHAN_CARD;

        for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
            j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,m += IOR_MAX_COLS){
        	if(j < num_chans){
			ab_boread(type,link,adapter,card,&jval,masks[j]);
                 	if (jval != 0) 
                  		 jval = 1;
                         printf("Chan %d = %x\t ",j,jval);
                }  
         	if(k < num_chans){
			ab_boread(type,link,adapter,card,&kval,masks[k]);
                        if (kval != 0) 
                        	kval = 1;
                        	printf("Chan %d = %x\t ",k,kval);
                }
                if(l < num_chans){
			ab_boread(type,link,adapter,card,&lval,masks[l]);
                	if (lval != 0) 
                        	lval = 1;
                	printf("Chan %d = %x \t",l,lval);
                 }
                  if(m < num_chans){
			ab_boread(type,link,adapter,card,&mval,masks[m]);
                 	if (mval != 0) 
                        	mval = 1;
                 	printf("Chan %d = %x \n",m,mval);
                   }
             }
  }

/*  ab_ai_report. 
*   Reports  the raw value of all Allen Bradley Analog In cards. 
*
*
*/
ab_ai_report(type,link,adapter,card,plc_card)
      
   unsigned short type;
   short link,adapter,card,plc_card;
  {
	short	i,j,k,l,m,num_chans;
        unsigned short jrval,krval,lrval,mrval;

        printf("\n");

	num_chans = ai_num_channels[type];

        for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
            j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,m += IOR_MAX_COLS){
        	if(j < num_chans){
                        ab_aidriver(type,link,adapter,card,j,plc_card,&jrval,0);
                         printf("Chan %d = %x\t ",j,jrval);
                }  
         	if(k < num_chans){
                        ab_aidriver(type,link,adapter,card,k,plc_card,&krval,0);
                        	printf("Chan %d = %x\t ",k,krval);
                }
                if(l < num_chans){
                        ab_aidriver(type,link,adapter,card,l,plc_card,&lrval,0);
                	printf("Chan %d = %x \t",l,lrval);
                 }
                  if(m < num_chans){
                        ab_aidriver(type,link,adapter,card,m,plc_card,&mrval,0);
                 	printf("Chan %d = %x \n",m,mrval);
                   }
             }
  }

/*  ab_ao_report. 
*   Reports  the raw value of all Allen Bradley Analog In cards. 
*
*
*/

ab_ao_report(type,link,adapter,card,plc_card)
   unsigned short type;
   short link,adapter,card,plc_card;
  {
	short	i,j,k,l,m,x,num_chans;
        unsigned short jrval,krval,lrval,mrval;

        printf("\n");

	num_chans = ao_num_channels[type];

        for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
            j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,m += IOR_MAX_COLS){
        	if(j < num_chans){
                        ab_aoread(type,link,adapter,card,j,plc_card,&jrval);
                        printf("Chan %d = %x\t ",j,jrval);
                }  
         	if(k < num_chans){
                        ab_aoread(type,link,adapter,card,k,plc_card,&krval);
                        printf("Chan %d = %x\t ",k,krval);
                }
                if(l < num_chans){
                        ab_aoread(type,link,adapter,card,l,plc_card,&lrval);
                	printf("Chan %d = %x \t",l,lrval);
                 }
                  if(m < num_chans){
                        ab_aoread(type,link,adapter,card,m,plc_card,&mrval);
                 	printf("Chan %d = %x \n",m,mrval);
                   }
             }
  }


/*
 * abScanTask
 *
 * Scans the AB IO according to the AB configuration table.
 * Entries are made in the AB configuration table when an IO
 * interface is attempted from the database scan tasks.
 * The sleep time assures that there is at least 1/10 second between the passes.
 * The time through the scan loop seems to be minimal so there are no provisions
 * for excluding the scan time from the sleep time.
 */

abScanTask(){
    register unsigned short	adapter,link;
register short pass;
register int tmp_op_stat;
struct ab_region	*p6008;
int status;

    while(TRUE){
	pass++;
        p6008 =(struct ab_region *)ab_stdaddr;
	/* check each link */

	for (link=0; link < AB_MAX_LINKS; link++,p6008++){
		if (p6008s[link] == 0) continue;	/* no link */

		/* See if we've detected any unsolicited block transfers.	*/
		/* These can result if scanner writes discrete info to a slot	*/
		/* requiring a block transfer.					*/
		if(ab_op_stat[link] & UNSOLICITED_BT){
		    if (ab_debug != 0) 
			logMsg("link %x, unsolicited_block xfer\n",link);
			p6008->osw = 0;	/* scanner depends on us to clear some bits */
		}

		
		/* check each adapter */
		for (adapter = 0; adapter < AB_MAX_ADAPTERS; adapter++){
			if (ab_adapter_status[link][adapter] >= 0)
				read_ab_adapter(link,adapter,pass);
		}

		/* Every second perform a link check to see if any adapters */
		/* have  changed state.  (Don't want to queue up requests if*/
		/* they're off) */
		if((pass % 10) == 0){
			if (link_status(link) != 0){
				if(ab_debug)
					logMsg("%x link_stat error %x\n",link);
			}
		}

	}

	/* run every 1/10 second */
	taskDelay(SECOND/10);
    }
}

/* ioc_reboot - routine to call when IOC is rebooted with a control-x */

int ioc_reboot(boot_type)
int	boot_type;
{
	short i;
	static char	wait_msg[] = {"I Hate to WAIT"};
	register char	*pmsg = &wait_msg[0];

	/* Stop communication to the Allen-Bradley Scanner Cards */
	if (abScanId != 0){
		/* flag the analog output and binary IO routines to stop */ 
		ab_disable = 1;

		/* delete the scan task stops analog input communication */
		taskDelete(abScanId);

		/* this seems to be necessary for the AB card to stop talking */
		printf("\nReboot: delay ");
		for(i = 0; i <= 14; i++){
			printf("%c",*(pmsg+i));
			taskDelay(20);
		}
	}
}
