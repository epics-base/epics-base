/* drvJgvtr1.c */
/* share/src/drv $Id$ */
/*
 *	 Author:      Jeff Hill
 * 	Date:        5-89
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
 *	110689	joh	print mem not full message only once
 *	120789	joh	temporary removal of memory full check
 *	050190	joh	clear ptr to callback prior to calling 
 *			it so they can rearm from inside the callback
 *	071190  joh	check STD address after cycle complete detected
 *			to avoid erroneous card misaddressed messages
 *	071190	joh	internal sample rate status is bit reversed on the
 *			card- I added a lookup table to untwist it.
 *	020491  ges	Change taskDelay from 20 to 2 in "jgvtr1DoneTask".
 *			To allow rearm and data reads from succesive
 *				waveform scans up thru 10Hz rates. 
 * 	031491	lrd	move data into a local memory area for each card
 * 	090591	joh	converted to V5 vxWorks
 *	110591	lrd	initialization of cards other than 0 not 
 *			allocating data buffer correctly
 *      013092   bg     added sysBusToLocalAdrs.  Added levels to io_report 
 *			and the ability to read out the Joerger's raw values 
 *			in io_report if level is > 1.
 *	031992	joh	Took the vxMemProbe out of each arm and checked
 *			the card present bit instead.
 *	062592	 bg	Combined drvJgvtr1.c and jgvtr_driver.c
 *	062992	joh	removed file pointer argument added to io
 *			report by bg
 */

static char *sccsID = "$Id$\t$Date$";

/*
 * 	Code Portions
 *
 *	jgvtr1_init()
 *	jgvtr1_driver(card, pcbroutine, parg)
 *	jgvtr1_int_service()
 *	jgvtr1DoneTask()
 *	jgvtr1_io_report()
 *	jgvtr1_stat(card)
 *
 */
#include <vxWorks.h>

/* drvJgvtr1.c -  Driver Support Routines for Jgvtr1 */
#include	<vxWorks.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include        <task_params.h>
#include 	<fast_lock.h>
#include	<types.h>
#include	<vme.h>

#	ifdef V5_vxWorks
#		include <iv.h>
#	else
#		include <iv68k.h>
#	endif


/* If any of the following does not exist replace it with #define <> NULL */
static long jgvtr1_io_report();
static long jgvtr1_init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvJgvtr1={
	2,
	jgvtr1_io_report,
	jgvtr1_init};

static char SccsId[] = "@(#)jgvtr1_driver.c	1.4\t11/14/88";

static char	*stdaddr;
static char	*shortaddr;


#define JGVTR1MAXFREQ 		25.0e6
/* NBBY - the number of bits per byte */
#define JGVTR1SHORTSIZE		(1<<(NBBY*sizeof(char))) 
#define JGVTR1STDSIZE		(1<<(NBBY*sizeof(short)))
#define JGVTR1_INT_LEVEL 5
#define JGVTR1BASE(CARD)\
(shortaddr+wf_addrs[JGVTR1]+(CARD)*JGVTR1SHORTSIZE)
#define JGVTR1DATA(CARD)\
(stdaddr+wf_memaddrs[JGVTR1]+(CARD)*JGVTR1STDSIZE)


/*
Joerger fixed hardware bug by switching to an inverting tristate buffer
where these commands are read from the VME bus.  As a result these commands
are complemented.
*/
#define JGVTR1ARM		(~1)	
#define JGVTR1START		(~2)
#define JGVTR1STOP		(~4)



/* 
!! our compiler allocates bit fields starting from the ms bit !! 
*/
struct jgvtr1_status{
	unsigned	pad:8;
 	unsigned	internal_frequency:3;
  	unsigned	internal_clock:1;
  	unsigned	cycle_complete:1;
  	unsigned	interrupt:1;
  	unsigned	active:1;
  	unsigned	memory_full:1;
};

struct jgvtr1_config{
	char		present;	/* card present			*/
	char		std_ok;		/* std addr ok on first read	*/
	void		(*psub)();	/* call back routine		*/
	void		*pprm;		/* call back parameter		*/
	FAST_LOCK	lock;		/* mutual exclusion		*/
	unsigned short	*pdata;		/* pointer to the data buffer	*/
};

/* amount of data to make available from the waveform */
#define	JRG_MEM_SIZE	2048

static
struct jgvtr1_config	*pjgvtr1_config;
static
int			jgvtr1_max_card_count;

#ifdef INTERRUPT_HARDWARE_FIXED
	static
	SEM_ID		jgvtr1_interrupt;	/* interrupt event */
#endif



/*
 * JGVTR1_INIT
 *
 * intialize the driver for the joerger vtr1
 *
 */
long jgvtr1_init()
{
	register unsigned 	card;
	register unsigned 	card_count = 0;
	register struct jgvtr1_config	
				*pconfig;
	short			readback;
	int			status;

	void			jgvtr1_int_service();
	void			jgvtr1DoneTask();

        if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,0,&shortaddr)) != OK){ 
           printf("Addressing error for short address in jgvtr1 driver\n");
           return ERROR;
        } 

        if ((status = sysBusToLocalAdrs(VME_AM_STD_SUP_DATA,0,&stdaddr)) != OK){
           logMsg("Addressing error for standard address in jgvtr1 driver\n");
           return ERROR;
        }

	jgvtr1_max_card_count = wf_num_cards[JGVTR1];

	if(pjgvtr1_config){
		if(FASTLOCKFREE(&pjgvtr1_config->lock)<0)
			return ERROR;
		if(free(pjgvtr1_config)<0)
			return ERROR;
	}

	pjgvtr1_config = 
		(struct jgvtr1_config *) 
			calloc(wf_num_cards[JGVTR1], sizeof(*pjgvtr1_config));
  	if(!pjgvtr1_config)
    	  	return ERROR;
         

	for(	card=0, pconfig=pjgvtr1_config; 
		card < wf_num_cards[JGVTR1]; 
		pconfig++, card++){

		FASTLOCKINIT(&pconfig->lock);

  		status = vxMemProbe(	JGVTR1BASE(card),
					READ,
					sizeof(readback),
					&readback);
		if(status==ERROR)
			continue;
               

		pconfig->pdata = 
			(unsigned short *)malloc(JRG_MEM_SIZE);
		/*
		not easy to test for correct addressing in
		standard address space since the module does
		not respond if it has not clocked in data

		- so I check this the first time data is ready
		*/
		pconfig->std_ok = FALSE; /* presumed guilty before tested */
		pconfig->present = TRUE;
		card_count++;
	}

	if(!card_count)
		return OK;
       

#	ifdef INTERRUPT_HARDWARE_FIXED
		jgvtr1_interrupt = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
		if(!jgvtr1_interrupt)
			return ERROR;
#	endif

        /* start the waveform readback task */
	status = taskSpawn(	WFDONE_NAME,
				WFDONE_PRI,
				WFDONE_OPT,
				WFDONE_STACK,
				jgvtr1DoneTask);
	if(status == ERROR)
	  	return ERROR;
         

#	ifdef INTERRUPT_HARDWARE_FIXED
  		status = intConnect(	INUM_TO_IVEC(JGVTR1_INT_VEC), 
					jgvtr1_int_service, 
					NULL);
  		if(status != OK)
    			return ERROR;
        sysIntEnable(JGVTR_INT_LEVEL); 
#	endif

    	return OK;	
}



/*
 * JGVTR1_DRIVER
 *
 * initiate waveform read
 *
 */
jgvtr1_driver(card, pcbroutine, parg)
register unsigned short	card;
register unsigned int	*pcbroutine;
register unsigned int	*parg;	/* number of values read */
{
	if(card >= jgvtr1_max_card_count)
		return ERROR;
        
	if(!pjgvtr1_config[card].present)
		return ERROR;
       
	if(pjgvtr1_config[card].psub)
		return ERROR;

	FASTLOCK(&pjgvtr1_config[card].lock);

	*(short *)JGVTR1BASE(card) = JGVTR1ARM;

	pjgvtr1_config[card].pprm = parg;
 	pjgvtr1_config[card].psub = (void (*)()) pcbroutine;
 	
	FASTUNLOCK(&pjgvtr1_config[card].lock);

	return OK;
} 


/*
 * JGVTR1_INT_SERVICE
 *
 * signal via the RTK that an interrupt occured from the joerger vtr1
 *
 */
#ifdef INTERRUPT_HARDWARE_FIXED
void
jgvtr1_int_service()
{
  	semGive(jgvtr1_interrupt);
}
#endif


/*
 * JGVTR1DONETASK
 *
 * wait for joerger vtr1 waveform record cycle complete
 * and call back to the database with the waveform size and address
 *
 */
void
jgvtr1DoneTask()
{
	register unsigned		card;
	register struct jgvtr1_config	*pconfig;
 	struct jgvtr1_status 		stat;
	static char 			started = FALSE;
	register unsigned short		*pdata,*pjgdata;
	register long			i;

	/* dont allow two of this task */
	if(started)
	  exit(1);
	started = TRUE;

    	while(TRUE){

#		ifdef INTERRUPT_HARDWARE_FIXED
			semTake(jgvtr1_interrupt, WAIT_FOREVER);
#		else
			/* ges: changed from 20 ticks to 2 ticks 2/4/91 */
			taskDelay(2);
#		endif
		for(	card=0, pconfig = pjgvtr1_config;
			card < jgvtr1_max_card_count;
			card++, pconfig++){

			void (*psub)() = pconfig->psub;

			if(!pconfig->present)
				continue;

			if(!psub)
				continue;

			stat = *(struct jgvtr1_status *) JGVTR1BASE(card);
			/*		
		 	 * Wait for the module to finish filling its memory 
			 * or a stop trigger
			 */
			if(!stat.cycle_complete)
				continue;

			/*
			clear ptr to function here so they 
			can rearm in the callback
			*/
			pconfig->psub = NULL;
			/*
				check the first time for module
				correctly addressed

				card does not respond at STD address
				until it has data
			*/
			if(!pconfig->std_ok){
				short 	readback;	
				int	status;

  				status = vxMemProbe(	JGVTR1DATA(card),
							READ,
							sizeof(readback),
							&readback);
				if(status==ERROR){
					logMsg(	"jgvtr1 card %d incorrectly addressed- use std addr %x\n",
						card,
						JGVTR1DATA(card));
					pconfig->present = FALSE;
					continue;
				}
				pconfig->std_ok = TRUE;
			}
/*
			Test for full memory
			( card designer does not give a sample count register )
			( instead a bus error returned when on the last sample 
			  to test every location is a lot of overhead so a test
			  for memory full is used for now )
*/
			if(!stat.memory_full){
				logMsg("jgvtr1 driver: proceeding with partial mem\n");
				logMsg("jgvtr1 driver: beware of bus errors\n");
			}

			/* copy the data into a local memory buffer */
			/* this is to avoid any bus errors          */
			for(i = 0, 
			  pdata = pconfig->pdata, 
			  pjgdata = (unsigned short *)JGVTR1DATA(card);
			  i < JRG_MEM_SIZE/sizeof(unsigned short); 
			  i++, pdata++, pjgdata++){
				*pdata = *pjgdata;
			}

/*	
			Post waveform to the database
			perhaps the size must be the size below+1 ?
			(Joerger's documentation is not clear here)
*/
			(*psub)(pconfig->pprm,JRG_MEM_SIZE,pconfig->pdata);
    		} 
   	}
}



/*
 *
 *	JGVTR1_IO_REPORT
 *
 *	print status for all cards in the specified joerger 
 *	vtr1 address range
 *
 *
 */
long jgvtr1_io_report(level)
short int level;
{
	unsigned 	card;
	unsigned	nelements;
	int		status;
	
	for(card=0; card < wf_num_cards[JGVTR1]; card++){
		status = jgvtr1_stat(card,level);
		if(status<0){
			continue;
		}
		if (level >= 2){
			printf("enter the number of elements to dump:");
			status = scanf("%d",&nelements);
			if(status == 1){
				jgvtr1_dump(card, nelements);
			}
		}
	}
	return OK;
}



/*
 *
 *	JGVTR1_STAT
 *
 *	print status for a single card in the joerger vtr1 address range
 *	
 *
 */
jgvtr1_stat(card,level)
unsigned	card;
short int 	level;
{
 	struct jgvtr1_status 		stat;
	int				status;

	/* 
	internal freq status is bit reversed so I
	use a lookup table
	*/

	static float 		sample_rate[8] = {
				JGVTR1MAXFREQ/(1<<7),
				JGVTR1MAXFREQ/(1<<3),
				JGVTR1MAXFREQ/(1<<5),
				JGVTR1MAXFREQ/(1<<1),
				JGVTR1MAXFREQ/(1<<6),
				JGVTR1MAXFREQ/(1<<2),
				JGVTR1MAXFREQ/(1<<4),
				JGVTR1MAXFREQ/(1<<0)};

	static char  		*clock_status[] = 
		{"ext-clock",	"internal-clk"};
	static char  		*cycle_status[] = 
		{"cycling",		"done"};
	static char  		*interrupt_status[] = 
		{"",			"int-pending"};
	static char  		*activity_status[] = 
		{"",			"active"};
	static char  		*memory_status[] = 
		{"",			"mem-full"};

  	status = vxMemProbe(	JGVTR1BASE(card),
				READ,
				sizeof(stat),
				&stat);
  	if(status != OK)
    	  return ERROR;
        if (level == 0)
  	   printf("WF: JGVTR1:\tcard=%d \n",card);
        else if (level >  0)
        	printf(	"WF: JGVTR1:\tcard=%d Sample rate=%g %s %s %s %s %s \n",
	 		card,
			sample_rate[stat.internal_frequency],
			clock_status[ stat.internal_clock ],
			cycle_status[ stat.cycle_complete ],
			interrupt_status[ stat.interrupt ],
			activity_status[ stat.active ],
			memory_status[ stat.memory_full ]);

	return OK;
}


/*
 * jgvtr1_dump
 *
 */
LOCAL
int jgvtr1_dump(card, n)
unsigned	card;
unsigned	n;
{
	register unsigned short		*pjgdata;
        unsigned short                  *pread;
        unsigned short                  *pdata;
	unsigned			nread;
      	int				status;
 
        /* Print out the data if user requests it. */

	n = min(JRG_MEM_SIZE,n);
	
	pdata = (unsigned short *)malloc(n * (sizeof(*pdata)));
	if(!pdata){
		return ERROR;
	}
	
	pread = pdata;
	nread = 0;
	pjgdata = (unsigned short *)JGVTR1DATA(card);
	while(nread <= (n>>1)){
		status = vxMemProbe(
				pjgdata,
				READ,
				sizeof(*pread),
				pread);
		if(status<0){
			break;
		}
		nread++;
		pread++;
		pjgdata++;
 	} 

	for(pread=pdata; pread<&pdata[nread]; pread++){
                if ((pread-pdata)%8 == 0){
	           printf("\n\t"); 
		}
		printf(	"%02X %02X ",
			(unsigned char) ((*pread)>>8),
			(unsigned char) *pread);
	}

	printf("\n");

	free(pdata);        

	return OK;
}
