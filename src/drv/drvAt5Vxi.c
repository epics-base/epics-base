/* at5vxi_driver.c */
/* share/src/drv @(#) $Id$ */

/*
 * 	at5vxi_driver.c
 *
 * 	driver for at5 designed VXI modules
 *
 * 	Author:      Jeff Hill
 * 	Date:        11-89
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
 * 	Modification Log:
 * 	-----------------
 *	.01 joh 021490	first release
 *	.02 joh 040490	took out init of binary outs as requested by AT5
 *	.03 joh 040490	KLUDGED so DC is invarient of model number
 *	.04 joh 072590	fixed case where a missing SC module could 
 *			be accessed before checking to prevent bus error
 *	.05 joh 102990	slightly improved sync to busy in init
 *	.06 joh 032191	added code to implement a new register 
 *			interface from AT5
 *	.07 joh 080291	disable ints during control X reboot
 *	.08 joh 080291	fixed ints on before handler installed problem
 *			introduced by .06
 *	.09 joh 080291	synch source with sorce release control version
 *	.10 joh 080891	delinting
 *	.11 joh 090591	converted to v5 vxWorks
 *	.12 joh 120591	reorganized for use with new vxi support and removed 
 *			KLUDGE introduced in .03
 *	.13 joh 042492	removed support for (ifdefs for) the old
 *			style register map
 *	.14 joh 071792	added model name registration
 *	.15 joh 072992	print more raw values in io report
 *	.16 joh 081092	merged at5vxi_models.h into this source
 *	.17 joh 081992	function name change	
 *
 *	Notes:
 *	------
 *	.01	Dont use C bitfields to write the AT5VXI CSR 
 *		directly because some bits have different meanings 
 *		for read than for write (bitfields writes generate RMW 
 *		instructions- performing reads with C bitfields 
 *		from an IO module works fine however).
 *
 *		To avoid this I write all bits in the CSR with
 *		each write. See the codes defined by at5vxi_init().
 */

/*
 *	Improvements:
 *
 *	change the pconfig pointer to be after the channel number 
 *	indexing in some cases?
 *
 *	use dev255 to switch between static and dynamic addressing?
 *
 *	Dont allow them to connect an interrupt if another device is
 *	installed there ! Perhaps intConnectSafe() should be written?
 */



/*
 * 	Code Portions
 *
 * 	AT5VXI_INIT		initialize all at5vxi cards
 * 	AT5VXI_INIT_CARD	initialize single at5vxi card
 *	AT5VXI_INT_SERVICE	update card busy writes, notify IO scanner
 *	AT5VXI_ONE_SHOT		setup AMD9513 STC with a one shot 
 *	AT5VXI_ONE_SHOT_READ	read back timing from an AMD 9513 STC 
 *	AT5VXI_STAT		print status for a single at5 vxi card
 *	AT5VXI_READ_TEST	diagnostic
 *	AT5VXI_AI_DRIVER	analog input driver	
 *	AT5VXI_AO_DRIVER	analog output driver
 *	AT5VXI_AO_READ		analog output read back
 *	AT5VXI_BI_DRIVER	binary input driver
 *	AT5VXI_BO_DRIVER	binary output driver
 *
 *
 */

#include <vxWorks.h>
#ifdef V5_vxWorks
#	include <iv.h>
#else
#	include <iv68k.h>
#endif
#include <types.h>
#include <module_types.h>
#include <task_params.h>
#include <fast_lock.h>
#include <epvxiLib.h>

static char SccsId[] = "$Id$\t$Date$";

typedef long (*DRVSUPFUN) ();   /* ptr to driver support function*/


void 		log_at5vxi_failure();
void		at5vxi_int_service();
void		at5vxi_init_card();
void		at5vxi_stat();
void		at5vxi_shutdown();
void		at5vxi_shutdown_card();
int 		at5vxi_report_timing();
long		at5vxi_init();

struct {
	long    number;
	DRVSUPFUN       report;
	DRVSUPFUN       init;
} drvAt5Vxi={
        2,
	NULL,			/* VXI io report takes care of this */
	at5vxi_init};

#define EXT_TICKS 5.0e06	/* GTA std speed of SRC1 in Hz	 */
	
/*
 *	Set this flag if you wish for the driver to
 *	to disable the busy period and operate
 *	without periodic interrupts
 *
#define CONTINUOUS_OPERATION
 *
 */

/*
 * all AT5VXI cards will use this VME interrupt level
 */
#ifdef CONTINUOUS_OPERATION
#	define AT5VXI_INT_LEVEL 	0
#	define AT5VXI_INT_ENABLE 	FALSE	
#	define AT5VXI_BUSY_ENABLE 	FALSE	
#else
#	define AT5VXI_INT_LEVEL 	5
#	define AT5VXI_INT_ENABLE 	TRUE
#	define AT5VXI_BUSY_ENABLE 	TRUE
#endif

#define abort(A)	taskSuspend(0)

/* 
 *	bit fields allocated starting with the ms bit
 */
struct at5vxi_status{
	unsigned	pad0:1;
	unsigned	modid_cmpl:1;
	unsigned	dev255:1;
	unsigned	busy:1;
	unsigned	pad1:2;
	unsigned	timer_bank:1;
	unsigned	ipend:1;
	unsigned	ienable:1;
	unsigned	ilevel:3;
	unsigned	ready:1;
	unsigned	passed:1;
	unsigned	sfinh:1;
	unsigned	sreset:1;
};


#define PSTATUS(PCSR)\
(&((struct vxi_csr *)PCSR)->dir.r.status)

struct at5vxi_control{
	unsigned	pad0:3;
	unsigned	busy_enable:1;
	unsigned	pad1:2;
	unsigned	timer_bank:1;
	unsigned	softint:1;
	unsigned	ienable:1;
	unsigned	ilevel:3;
	unsigned	pad2:2;
	unsigned	sfinh:1;
	unsigned	sreset:1;
};

/*
 * Insert or extract a bit field using the standard
 * masks and shifts defined below
 */
#define INSERT(FIELD,VALUE)\
(((VALUE)&(FD_/* */FIELD/* */_M))<<(FD_/* */FIELD/* */_S))
#define EXTRACT(FIELD,VALUE)\
( ((VALUE)>>(FD_/* */FIELD/* */_S)) &(FD_/* */FIELD/* */_M))

/*
 * in the constants below _M is a right justified mask
 * and _S is a shift required to right justify the field
 */
#define FD_INT_ENABLE_M		(0x1)
#define FD_INT_ENABLE_S		(7)
#define FD_BUSY_ENABLE_M	(0x1)
#define FD_BUSY_ENABLE_S	(12)
#define FD_BUSY_STATUS_M	(0x1)
#define FD_BUSY_STATUS_S	(12)
#define FD_INT_LEVEL_M		(0x7)
#define FD_INT_LEVEL_S		(4)
#define FD_TIMER_BANK_M		(0x1)
#define FD_TIMER_BANK_S		(9)

#define BUSY(PCSR)\
(((struct vxi_csr *)PCSR)->dir.r.status & INSERT(BUSY_STATUS,1))

/*
 * Some constants for the CSR. 
 *
 */
#ifndef CONTINUOUS_OPERATION
#	define INTDISABLE\
	( \
	INSERT(BUSY_ENABLE, AT5VXI_BUSY_ENABLE) | \
	INSERT(INT_ENABLE, FALSE) | \
	INSERT(INT_LEVEL, AT5VXI_INT_LEVEL) \
	)
#endif

/* 
 * Used to initialize the control register. 
 * (enables interrupts)
 */
#define CSRINIT\
	( \
	INSERT(BUSY_ENABLE, AT5VXI_BUSY_ENABLE) | \
	INSERT(INT_ENABLE, AT5VXI_INT_ENABLE) | \
	INSERT(INT_LEVEL, AT5VXI_INT_LEVEL) \
	)

/* 
 * Use bank zero of the timing. 
 */
#define BANK0\
	( \
	INSERT(TIMER_BANK, 0) | \
	INSERT(BUSY_ENABLE, AT5VXI_BUSY_ENABLE) | \
	INSERT(INT_LEVEL, AT5VXI_INT_LEVEL) \
	)
/* 
 * Use bank one of the timing. 
 */
#define BANK1\
	( \
	INSERT(TIMER_BANK, 1) | \
	INSERT(BUSY_ENABLE, AT5VXI_BUSY_ENABLE) | \
	INSERT(INT_LEVEL, AT5VXI_INT_LEVEL) \
	)

/* 
 * Some constants for the CSR. 
 * set at initialization for better readability
 */
#define PCONTROL(PCSR)\
(&((struct vxi_csr *)PCSR)->dir.w.control)




#define AT5VXI_TIMER_BANKS_PER_MODULE 2
#define AT5VXI_CHANNELS_PER_TIMER_BANK 5
#define AT5VXI_NTIMER_CHANNELS\
	(AT5VXI_TIMER_BANKS_PER_MODULE*AT5VXI_CHANNELS_PER_TIMER_BANK)


struct at5vxi_dd{
	unsigned short		bio[2];
	unsigned short		tdata;
	unsigned char		pad;
	unsigned char		tcmd;
	unsigned short		ai[8];
	unsigned short		ao[16];
};


struct at5vxi_setup{
#	define		UNITY	0
#	define		TIMES2	1
	unsigned	gainA:1;
	unsigned	gainB:1;
	unsigned	gainC:1;
	unsigned	gainD:1;
#	define		UNIPOLAR 0
#	define		BIPOLAR 1
	unsigned	modeA:1;
	unsigned	modeB:1;
	unsigned	modeC:1;
	unsigned	modeD:1;
	unsigned	ch2enbl:1;
	unsigned	ch2select:3;
	unsigned	ch1enbl:1;
	unsigned	ch1select:3;
};


#define AT5VXI_BUSY_PERIOD 2

struct bo_val{
	unsigned long	val;
	unsigned long	mask;
};

struct ao_val{
	short		mdt;
	unsigned short 	val;
};

struct time_val{
	unsigned	preset;		
	unsigned short	iedge0_delay;	
	unsigned short	iedge1_delay;	
	char		mdt;
	char		valid;
};

struct at5vxi_config{
	FAST_LOCK		lock;		/* mutual exclusion	*/
	struct bo_val		bv;		/* binary out values	*/
	struct ao_val		av[16];		/* analog out values	*/
	struct time_val		tv[10];		/* delayed pulse values	*/
	char			mdt;		/* modified data tag	*/
	struct vxi_csr		*pcsr;		/* vxi device hdr ptr 	*/
	struct at5vxi_dd	*pdd;		/* at5 device dep ptr	*/
};

LOCAL unsigned long at5vxiDriverID;

#define AT5VXI_PCONFIG(CARD) \
epvxiPConfig((CARD), at5vxiDriverID, struct at5vxi_config *)	

#define AT5VXI_CORRECT_MAKE(PCSR) 	(VXIMAKE(PCSR)==VXI_MAKE_AT5)

struct  at5vxi_model{
        char            *name;          /* AT5 VXI module name */
        char            *drawing;       /* AT5 VXI assembly drawing number */
};

#define AT5VXI_INDEX_FROM_MODEL(MODEL) ((unsigned)((MODEL)&0xff))
#define AT5VXI_MODEL_FROM_INDEX(INDEX) ((unsigned)((INDEX)|0xf00))

/*
        NOTE: The macro AT5VXI_INDEX_FROM_MODEL(MODEL) defined above
        should return an index into the correct data given the
        VXI device's model code.
*/
struct at5vxi_model at5vxi_models[] = {
        {"INTERFACE SIMULATOR",         "112Y-280158"},
        {"I CONTROLLER",                "112Y-280176"},
        {"CONTROL PREDISTORTER",        "112Y-280172"},
        {"VECTOR DETECTOR",             "112Y-280230"},
        {"VECTOR MODULATOR",            "112Y-280177"},
        {"425MHz ENVELOPE DETECTOR",    "112Y-280169"},
        {"425MHz DOWNCONVERTER",        "112Y-280165"},
        {"POLAR DETECTOR",              "112Y-280567"},
        {"UPCONVERTER",                 "112Y-280225"},
        {"MONITOR TRANSMITTER",         "112Y-280187"},
        {"TIMING DISTRIBUTION",         "112Y-280582"},
        {"LINE CONDITIONER",            "112Y-280305"},
        {"BEAM FEEDFORWARD",            "112Y-280564"},
        {"TIMING RECEIVER",             "112Y-280243"},
        {"FAST PROTECTION",             "112Y-280246"},
        {"ADAPTIVE FEEDFORWARD",        "112Y-280563"},
        {"CABLE CONTROLLER",            "112Y-280307"},
        {"Q CONTROLLER",                "112Y-280180"},
        {"ENVELOPE DETECTOR",           "112Y-280249"},
        {"DOWNCONVERTER",               "112Y-280456"},
        {"COAX MONITOR TRANSMITTER",    "112Y-280587"},
        {"CAVITY SIMULATOR",            "112Y-280232"},
};

#define AT5VXI_VALID_MODEL(MODEL) \
(AT5VXI_INDEX_FROM_MODEL(MODEL)<NELEMENTS(at5vxi_models))



/*
 * AT5VXI_INIT
 *
 * initialize all at5vxi cards
 *
 */
long
at5vxi_init()
{
	int 	r0;

	/*
	 * do nothing on crates without VXI
	 */
	if(!epvxiResourceMangerOK){
		return OK;
	}

	r0 = rebootHookAdd(at5vxi_shutdown);
	if(r0<0){
		return ERROR;
	}

	at5vxiDriverID = epvxiUniqueDriverID();

	{
		epvxiDeviceSearchPattern  dsp;

		dsp.flags = VXI_DSP_make;
		dsp.make = VXI_MAKE_AT5;
		r0 = epvxiLookupLA(&dsp, at5vxi_init_card, (void *)NULL);
		if(r0<0){
			return ERROR;
		} 
	}

	return OK;

}



/*
 * AT5VXI_SHUTDOWN
 *
 * disable interrupts on at5vxi cards
 *
 */
LOCAL void	
at5vxi_shutdown()
{
	epvxiDeviceSearchPattern  dsp;
	int			s;

        dsp.flags = VXI_DSP_make;
        dsp.make = VXI_MAKE_AT5;
        s = epvxiLookupLA(&dsp, at5vxi_shutdown_card, (void *)NULL);
        if(s<0){
		logMsg("AT5VXI module shutdown failed\n");
	}
}



/*
 * AT5VXI_SHUTDOWN_CARD
 *
 * disable interrupts on at5vxi cards
 *
 */
LOCAL void 
at5vxi_shutdown_card(la)
unsigned la;
{
#ifndef CONTINUOUS_OPERATION
	struct vxi_csr	*pcsr;

	pcsr = VXIBASE(la);

	pcsr->dir.w.control = INTDISABLE;
#endif
}



/*
 * AT5VXI_INIT_CARD
 *
 * initialize single at5vxi card
 *
 */
LOCAL void
at5vxi_init_card(addr)
unsigned addr;
{
	int			r0;
	struct at5vxi_config	*pc;
	struct time_val  	*ptv;
	unsigned		chan;
	int			i;
	int 			model;

	r0 = epvxiOpen(
		addr, 
		at5vxiDriverID, 
		(unsigned long) sizeof(*pc),
		at5vxi_stat);
	if(r0<0){
		logMsg("AT5VXI: device open failed %d\n", addr);
		return;
	}

	pc = AT5VXI_PCONFIG(addr);
	if(pc == NULL){
		epvxiClose(addr, at5vxiDriverID);
		return;
	}

	pc->pcsr = VXIBASE(addr);
	pc->pdd = (struct at5vxi_dd *) &pc->pcsr->dir.r.dd;

	FASTLOCKINIT(&pc->lock);

	/*
	 * revert to power up control 
	 * (temporarily disable the busy period)
	 */
	pc->pcsr->dir.w.control = 0;


#ifndef CONTINUOUS_OPERATION
	/*
	 * wait 5 sec for the end of current busy cycle as required
	 * (busy period is temporarily disabled
	 *
	 */
	for(i=0; i<5 && BUSY(pc->pcsr); i++)
		taskDelay(sysClkRateGet());
	if(BUSY(pc->pcsr)){
		epvxiClose(addr, at5vxiDriverID);
		return;
	}
#endif

#if defined(INIT_BINARY_OUTS)
	/*
	 * Set AD 664 default
	 */
	{
		struct at5vxi_setup su;

		su.gainA = TIMES2;
		su.gainB = TIMES2;
		su.gainC = TIMES2;
		su.gainD = TIMES2;

		su.modeA = BIPOLAR;
		su.modeB = BIPOLAR;
		su.modeC = BIPOLAR;
		su.modeD = BIPOLAR;

		su.ch2enbl = FALSE;
		su.ch1enbl = FALSE;

		(* (struct at5vxi_setup *) &pc->pdd->bio[1]) = su;
	}
#endif

	/*
	 * Init AMD 9513 for
	 * 
	 * binary division 
	 * data ptr seq enbl 
	 * 16 bit bus 
 	 * FOUT on 
	 * FOUT divide by one 
	 * FOUT source (F1) 
	 * Time of day disabled
	 */
#	define MASTER_MODE ((unsigned short)0x2000)
	*PCONTROL(pc->pcsr) = BANK0;
	r0 = stc_init(	&pc->pdd->tcmd, 
			&pc->pdd->tdata, 
			MASTER_MODE);
	if(r0 == ERROR){
		epvxiClose(addr, at5vxiDriverID);
		return;
	}

	*PCONTROL(pc->pcsr) = BANK1;
	r0 = stc_init(
		&pc->pdd->tcmd, 
		&pc->pdd->tdata, 
		MASTER_MODE);
	if(r0 == ERROR){	
		epvxiClose(addr, at5vxiDriverID);
		return;
	}

	for(chan=0, ptv = pc->tv; chan<NELEMENTS(pc->tv); chan++, ptv++){
		unsigned int_source;

		if(chan/AT5VXI_CHANNELS_PER_TIMER_BANK){
			*PCONTROL(pc->pcsr) = BANK1;
		}
		else{
			*PCONTROL(pc->pcsr) = BANK0;
		}

		r0 = stc_one_shot_read(
			&ptv->preset, 
			&ptv->iedge0_delay, 
			&ptv->iedge1_delay,	
			&pc->pdd->tcmd, 
			&pc->pdd->tdata, 
			chan,
			&int_source);
		if(r0 == OK && int_source == FALSE)
			ptv->valid = TRUE;
		else
			ptv->valid = FALSE;
	}

#	ifndef CONTINUOUS_OPERATION
  		r0 = intConnect(
			(unsigned char) INUM_TO_IVEC(addr), 
			at5vxi_int_service, 
			(void *) addr);
  		if(r0 == ERROR)
    			return;

		sysIntEnable(AT5VXI_INT_LEVEL);
#	endif

	/*
	 * init the csr
	 * (see at5vxi_init() for field definitions)
	 * interrupts enabled if not compiled for continuous operation
	 */
	*PCONTROL(pc->pcsr) = CSRINIT;

	model = VXIMODEL(pc->pcsr);	
	if(AT5VXI_VALID_MODEL(model)){
		r0 = epvxiRegisterModelName(
			VXIMAKE(pc->pcsr), 
			model, 
			at5vxi_models[AT5VXI_INDEX_FROM_MODEL(model)].name);
		if(r0<0){
			logMsg("%s: failed to register model at init: %x\n",
				__FILE__,
				model);
		}

		r0 = epvxiRegisterMakeName(VXI_MAKE_AT5, "LANL AT5");
		if(r0<0){
			logMsg(	"%s: unable reg make\n",
				__FILE__);
		}
	}

	return;
}


/*
 *
 *	AT5VXI_INT_SERVICE
 *
 *	update card busy writes and notify the IO interrupt scanner
 */
void 
at5vxi_int_service(addr)
int addr;
{
	register struct at5vxi_config *pconfig; 

	pconfig = AT5VXI_PCONFIG(addr);
	if(!pconfig){
		logMsg("AT5VXI: int before init\n");
		return;
	}

	/*
	 * wake up the I/O event scanner
	 */
	{
		io_scanner_wakeup(IO_AI, VXI_AT5_AI, addr);
		io_scanner_wakeup(IO_BI, VXI_AT5_BI, addr);
	}

	/*
	 * Update outputs while it is safe to do so
	 */
	if(pconfig->mdt){
		struct at5vxi_dd	*pdd;
		struct vxi_csr		*pcsr; 
		unsigned 		chan;
		int 			r0;

		pcsr = pconfig->pcsr;
		pdd = pconfig->pdd;

		if(!AT5VXI_CORRECT_MAKE(pcsr)){
			logMsg("AT5VXI: int from foreign card\n");
			return;
		}

		for(chan=0; chan<NELEMENTS(pconfig->tv); chan++){
			unsigned chip_chan;

			if(!pconfig->tv[chan].mdt)
				continue;


			if(chan/AT5VXI_CHANNELS_PER_TIMER_BANK){
				*PCONTROL(pcsr) = BANK1;
			}
			else{
				*PCONTROL(pcsr) = BANK0;
			}

 			chip_chan = chan%
				AT5VXI_CHANNELS_PER_TIMER_BANK;

 			r0 = stc_one_shot(
				pconfig->tv[chan].preset, 
				pconfig->tv[chan].iedge0_delay, 
				pconfig->tv[chan].iedge1_delay, 
				&pdd->tcmd, 
				&pdd->tdata, 
				chip_chan,
				FALSE);
			if(r0 == ERROR){
				logMsg("AT5 VXI- AMD9513 load fail\n");
			}
			else{
				pconfig->tv[chan].valid = TRUE;
			}

			/*
			 * reenable interrupts
			 */
			*PCONTROL(pcsr) = CSRINIT;

			pconfig->tv[chan].mdt = FALSE;
		}

		for(chan=0; chan<NELEMENTS(pconfig->av); chan++){
			if(!pconfig->av[chan].mdt)
				continue;
			pdd->ao[chan] = pconfig->av[chan].val<<4;
			pconfig->av[chan].mdt = FALSE;
		}

		if(pconfig->bv.mask){
			unsigned work;

			work = ((pdd->bio[1]<<(NBBY*sizeof(short))) | 
					pdd->bio[0]);

			/* alter specified bits */
			work = (work & ~pconfig->bv.mask) | 
				(pconfig->bv.val & pconfig->bv.mask);

			pdd->bio[0] =  work;
			pdd->bio[1] =  work>>(NBBY*sizeof(short));

			pconfig->bv.mask = 0;
		}

		if(BUSY(pcsr))
			logMsg("AT5 VXI INT- finished with card busy\n");

		pconfig->mdt = FALSE;
	}

}



/*
 *
 *	AT5VXI_ONE_SHOT
 *
 *	setup AMD 9513 STC for a repeated two edge timing signal 
 */
at5vxi_one_shot(	
		preset,
		edge0_delay,
		edge1_delay, 
		card, 
		channel,
		int_source,
		event_rtn,
		event_rtn_param)
int		preset;		/* TRUE or COMPLEMENT logic */
double		edge0_delay;	/* sec */
double		edge1_delay;	/* set */
unsigned	card;		/* 0 through ... */
unsigned	channel;	/* 0 through channels on a card */
int		int_source; 	/* (FALSE)External/(TRUE)Internal source */
void		*event_rtn;	/* subroutine to run on events */
int		event_rtn_param;/* parameter to pass to above routine */
{
  	int status;
  	register struct vxi_csr		*pcsr;
	register struct at5vxi_config 	*pconfig;

	pconfig = AT5VXI_PCONFIG(card);
	if(!pconfig)
		return ERROR;

	pcsr = pconfig->pcsr;

	if(!AT5VXI_CORRECT_MAKE(pcsr))
		return ERROR;

/*	AT5VXI does not support internal source for now
*/	if(int_source)
		return ERROR;

/*	AT5VXI does not support interrupts on timing channels for now
*/	if(event_rtn)
		return ERROR;

	if(channel>=AT5VXI_NTIMER_CHANNELS)
		return ERROR;

/* 	dont overflow unsigned short in STC
*/  	if(edge0_delay >= 0xffff/EXT_TICKS)
    		return ERROR;
  	if(edge1_delay >= 0xffff/EXT_TICKS)
    		return ERROR;
  	if(edge0_delay < 0.0)
    		return ERROR;
  	if(edge1_delay < 0.0)
    		return ERROR;

	FASTLOCK(&pconfig->lock);

#	ifdef CONTINUOUS_OPERATION
	{
		struct at5vxi_dd	*pdd;

		pdd = pconfig->pdd;

		if(channel/AT5VXI_CHANNELS_PER_TIMER_BANK){
			*PCONTROL(pcsr) = BANK1;
		}else{
			*PCONTROL(pcsr) = BANK0;
		}
		channel = channel%AT5VXI_CHANNELS_PER_TIMER_BANK;

  		status = stc_one_shot(
			preset, 
			(unsigned short) (edge0_delay * EXT_TICKS), 
			(unsigned short) (edge1_delay * EXT_TICKS), 
			&pdd->tcmd, 
			&pdd->tdata, 
			channel,
			FALSE);

		/*
		 * not required for now but safe
		 * against future mods
		 */
		*PCONTROL(pcsr) = CSRINIT;
	}
#	else
		*PCONTROL(pcsr) = INTDISABLE;
		pconfig->tv[channel].preset = preset;
		pconfig->tv[channel].iedge0_delay = 
				(edge0_delay * EXT_TICKS);
		pconfig->tv[channel].iedge1_delay = 
				(edge1_delay * EXT_TICKS);	
		pconfig->tv[channel].mdt = TRUE;		
		pconfig->mdt = TRUE;	
		*PCONTROL(pcsr) = CSRINIT;
	
		status = OK;
#	endif

	FASTUNLOCK(&pconfig->lock);

  	return status;
}


/*
 *
 *	AT5VXI_ONE_SHOT_READ
 *
 *	read back two edge timing from an AMD 9513 STC
 */
at5vxi_one_shot_read(	preset,
			edge0_delay,
			edge1_delay, 
			card, 
			channel,
			int_source)
int			*preset;	/* TRUE or COMPLEMENT logic */
double			*edge0_delay;	/* sec */
double			*edge1_delay;	/* sec */
unsigned		card;		/* 0 through ... */
unsigned		channel;	/* 0 through channels on a card */
int			*int_source; 	/* (FALSE)External/(TRUE)Internal src */
{	
#ifdef CONTINUOUS_OPERATION
  	unsigned short		iedge0;
  	unsigned short		iedge1;
  	unsigned int 		status;
#endif
  	register struct vxi_csr		*pcsr;
	register struct at5vxi_config 	*pconfig;

	pconfig = AT5VXI_PCONFIG(card);
	if(!pconfig)
		return ERROR;

	pcsr = pconfig->pcsr;

	if(!AT5VXI_CORRECT_MAKE(pcsr))
		return ERROR;

	if(channel>=AT5VXI_NTIMER_CHANNELS)
		return ERROR;


#	ifdef CONTINUOUS_OPERATION
	{
		struct at5vxi_dd	*pdd;

		pdd = pconfig->pdd;

		FASTLOCK(&pconfig->lock);

		if(channel/AT5VXI_CHANNELS_PER_TIMER_BANK){
			*PCONTROL(pcsr) = BANK1;
		}else{
			*PCONTROL(pcsr) = BANK0;
		}
		channel = channel%AT5VXI_CHANNELS_PER_TIMER_BANK;
		status = stc_one_shot_read(
				preset, 
				&iedge0, 
				&iedge1, 
				&pdd->tcmd, 
				&pdd->tdata, 
				channel,
				int_source);
		/*
		 * not required for noe but safe
		 * against future mods
		 */
		*PCONTROL(pcsr) = CSRINIT;
		FASTUNLOCK(&pconfig->lock);

  		if(status==OK){
			/*
			 * AT5VXI does not support external 
			 * source for now
			 */	
			if(int_source)
				return ERROR;

    			*edge0_delay = iedge0 / EXT_TICKS;
    			*edge1_delay = iedge1 / EXT_TICKS;
  		}


  		return status;
	}
#	else
		if(!pconfig->tv[channel].valid)
			return ERROR;

		FASTLOCK(&pconfig->lock);

		*preset = pconfig->tv[channel].preset;
		*edge0_delay = pconfig->tv[channel].iedge0_delay
					/EXT_TICKS;
		*edge1_delay = pconfig->tv[channel].iedge1_delay
					/EXT_TICKS;	
		*int_source = FALSE;

		FASTUNLOCK(&pconfig->lock);

		return OK;
#	endif
}



/*
 *
 *	AT5VXI_STAT
 *
 *	print status for a single at5 vxi card
 *	
 *
 */
void
at5vxi_stat(card, level)
unsigned	card;
int		level;
{
	struct vxi_csr			*pcsr; 
	register struct at5vxi_dd	*pdd;
	struct at5vxi_status		status;
	unsigned			channel;
	int				r0;
	struct at5vxi_config 		*pconfig;

	static char  	*busy_status[] = 	{"","busy"};
	static char  	*modid_status[] =	{"modid-on",""};
	static char  	*ipend_status[] =	{"","int-pending"};
	static char  	*ienable_status[] =	{"int-disabled","int-enabled"};
	static char  	*ext_st_status[] =	{"extended-self-testing",""};
	static char  	*st_status[] =		{"self-testing",""};
	static char  	*sfinh_status[] =	{"","sys-fail-inhibit"};
	static char  	*sreset_status[] =	{"","sys-reset"};
	static char  	*addr_mode_status[] =	{"SC","DC"};

	if(level==0)
		return;

	pconfig = AT5VXI_PCONFIG(card);

	pcsr = VXIBASE(card);
	pdd = pconfig->pdd;

  	r0 = vxMemProbe(	&pcsr->dir.r.status,
				READ,
				sizeof(status),
				&status);
  	if(r0 != OK)
    	  	return;

	if(VXIMAKE(pcsr) != VXI_MAKE_AT5)
		return;

	if(AT5VXI_VALID_MODEL(VXIMODEL(pcsr))){
		printf( "\tDrawing: %s\n",
			at5vxi_models[AT5VXI_INDEX_FROM_MODEL(VXIMODEL(pcsr))].drawing);
	}

  	printf(	
		"\tcard=%d address-mode=%s %s %s %s %s ilevel=%d %s %s %s %s\n",
		card,
		addr_mode_status[status.dev255],
		busy_status[ status.busy ],
		modid_status[ status.modid_cmpl ],
		ipend_status[ status.ipend ],
		ienable_status[ status.ienable ], 
		status.ilevel,
		ext_st_status[ status.ready ],
		st_status[ status.passed ],
		sfinh_status[ status.sfinh ],
		sreset_status[ status.sreset ]);

	if(pconfig){
		if(pconfig->mdt){
			printf("\toutput update is pending for interrupt\n");
		}
	}
 
	if(level <= 1)
		return;

	for(channel=0; channel<NELEMENTS(pdd->ai); channel++){
		printf(
			"\tAI:  channel %d value %x\n",
			channel,
			pdd->ai[channel]);
	}

	for(channel=0; channel<NELEMENTS(pdd->ao); channel++){
		printf(
			"\tAO:  channel %d value %x\n",
			channel,
			pdd->ao[channel]);
	}

	{
		long work;

		work = ((long)pdd->bio[1]) << (sizeof(short)*NBBY);	
		work |= pdd->bio[0];
		printf("\tBIO: value %x\n", work);
	}

	for(channel=0; channel<NELEMENTS(pconfig->tv); channel++){
		at5vxi_report_timing(card, channel);
	}

	return;
}


/*
 *
 *
 *	AT5VXI_REPORT_TIMING
 *
 *	diagnostic
 */
LOCAL int
at5vxi_report_timing(card,channel)
unsigned card;
unsigned channel;
{
  int 		preset;
  double 	edge0_delay;
  double 	edge1_delay;
  int 		int_source;
  int 		status;
  char		*clk_src[] = {"external-clk", "internal-clk"};

  status = 
    at5vxi_one_shot_read(	
			&preset,
			&edge0_delay,
			&edge1_delay, 
			card, 
			channel,
			&int_source);
  if(status == OK)
    printf(
	"\tTI:  channel %d preset %d delay %lf width %lf %s\n",
		channel,
		preset,
		edge0_delay,
		edge1_delay, 
		clk_src[int_source?1:0]);
		
  return status;
}


/*
 *
 *
 *	AT5VXI_AI_DRIVER
 *
 *	analog input driver
 */
at5vxi_ai_driver(card,chan,prval)
register unsigned short	card;
unsigned short		chan;
register unsigned short *prval;
{
	register struct at5vxi_dd	*pdd;
  	register struct vxi_csr		*pcsr;
	register struct at5vxi_config 	*pconfig;

	pconfig = AT5VXI_PCONFIG(card);
	if(!pconfig)
		return ERROR;

	pcsr = pconfig->pcsr;
	pdd = pconfig->pdd;

	if(!AT5VXI_CORRECT_MAKE(pcsr))
		return ERROR;

	if(chan >= NELEMENTS(pdd->ai))
		return ERROR;

	*prval = pdd->ai[chan]>>(unsigned)4;

	return OK;
}


/*
 *
 *
 *	AT5VXI_AO_DRIVER
 *
 *	analog output driver
 */
at5vxi_ao_driver(card,chan,prval,prbval)
register unsigned short card;
register unsigned short chan;
unsigned short 		*prval;
unsigned short 		*prbval;
{
	struct at5vxi_dd		*pdd;
  	register struct vxi_csr		*pcsr;
	register struct at5vxi_config 	*pconfig;

	pconfig = AT5VXI_PCONFIG(card);
	if(!pconfig)
		return ERROR;

	pcsr = pconfig->pcsr;
	pdd = pconfig->pdd;

	if(!AT5VXI_CORRECT_MAKE(pcsr))
		return ERROR;

	if(chan >= NELEMENTS(pdd->ao))
		return ERROR;

#ifdef CONTINUOUS_OPERATION
	pdd->ao[chan] = *prval<<4;
	*prbval = pdd->ao[chan]>>(unsigned)4;
#else
	*PCONTROL(pcsr) = INTDISABLE;
	pconfig->av[chan].val = *prval;
	pconfig->av[chan].mdt = TRUE;		
	pconfig->mdt = TRUE;	
	*PCONTROL(pcsr) = CSRINIT;

	*prbval = *prval & 0xfff;
#endif
	return OK;
}


/*
 *
 *
 *	AT5VXI_AO_READ
 *
 *	analog output read back
 */
at5vxi_ao_read(card,chan,pval)
register unsigned short card;
register unsigned short chan;
unsigned short 		*pval;
{
	register struct at5vxi_dd	*pdd;
  	register struct vxi_csr		*pcsr;
	register struct at5vxi_config 	*pconfig;

	pconfig = AT5VXI_PCONFIG(card);
	if(!pconfig)
		return ERROR;

	pcsr = pconfig->pcsr;
	pdd = pconfig->pdd;

	if(!AT5VXI_CORRECT_MAKE(pcsr))
		return ERROR;

	if(chan >= NELEMENTS(pdd->ao))
		return ERROR;

	*pval = pdd->ao[chan] >> 4;

	return OK;
}


/*
 *
 *
 *	AT5VXI_BI_DRIVER
 *
 *	binary input driver
 */
at5vxi_bi_driver(card, mask, prval)
register unsigned short card;
unsigned long		mask;
register unsigned long	*prval;
{
	register unsigned long		work;
	register struct at5vxi_dd	*pdd;
  	register struct vxi_csr		*pcsr;
	register struct at5vxi_config 	*pconfig;

	pconfig = AT5VXI_PCONFIG(card);
	if(!pconfig)
		return ERROR;

	pcsr = pconfig->pcsr;
	pdd = pconfig->pdd;

	if(!AT5VXI_CORRECT_MAKE(pcsr))
		return ERROR;

	FASTLOCK(&pconfig->lock);
	work = ((pdd->bio[1]<<(NBBY*sizeof(short))) | pdd->bio[0]);
	*prval = mask & work;
	FASTUNLOCK(&pconfig->lock);

	return OK;
}


/*
 *
 *
 *	AT5VXI_BO_DRIVER
 *
 *	binary output driver
 */
at5vxi_bo_driver(card,val,mask)
register unsigned short	card;
register unsigned long	val;
unsigned long		mask;
{
#ifdef CONTINUOUS_OPERATION
	register unsigned long		work;
#endif
  	register struct vxi_csr		*pcsr;
	register struct at5vxi_config 	*pconfig;

	pconfig = AT5VXI_PCONFIG(card);
	if(!pconfig)
		return ERROR;

	pcsr = pconfig->pcsr;

	if(!AT5VXI_CORRECT_MAKE(pcsr))
		return ERROR;

	FASTLOCK(&pconfig->lock);

#ifdef CONTINUOUS_OPERATION
	{
		struct at5vxi_dd	*pdd;

		pdd = pconfig->pdd;

		work = ((pdd->bio[1]<<(NBBY*sizeof(short))) | pdd->bio[0]);

		/* alter specified bits */
		work = (work & ~mask) | (val & mask);

		pdd->bio[0] =  work;
		pdd->bio[1] =  work>>(NBBY*sizeof(short));
	}
#else
	*PCONTROL(pcsr) = INTDISABLE;
	pconfig->bv.val = (pconfig->bv.val & ~mask) | (val & mask);
	pconfig->bv.mask |= mask;
	pconfig->mdt = TRUE;	
	*PCONTROL(pcsr) = CSRINIT;
#endif

	FASTUNLOCK(&pconfig->lock);
	
	return OK;
}

