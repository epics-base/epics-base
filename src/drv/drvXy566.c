
/* drvXy566.c */

/* share/src/drv $Id$ */
/*
 * subroutines that are used to interface to the vme analog input cards
 *
 * 	Author:      Bob Dalesio
 * 	Date:        6-13-88
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
 * .01	9-27-88		lrd	removed test code
 * .02	11-10-88	lrd	change addresses per HW Tech Note #2
 *				added external latched AI and made
 *				others scan continuously 
 * .03	02-08-89	lrd	moved module addresses into a table in
 *				module_types.h from ai_driver.h
 * .04	02-24-89	lrd	modify for vxWorks 4.0
 *				changed sysSetBCL to sysIntEnable
 * .05	05-10-89	lrd	increased performance for xycom 566 interface
 *				by keeping the address of the memory buffers
 *				thus removing the need to calculate on each read
 * .06	11-20-89	joh	added call to the at5vxi ai driver
 * .07	08-27-91	 bg	broke the 566 driver out of ai_driver.c
 *                              and moved it to this file. Moved io_report code
 *                              to this file. 
 *                              added arguments and raw value read out for analog
 *                              in cards.
 * .08  11-30-91         bg	Added sysBusToLocalAdrs to both ai and waveform 
 *                              sections.                  
 * .09  02-05-92         bg	Changed io_report so it has an argument level and
 *                              so if the level > 1, the raw value from the card 
 *                              will be printed out for analog ins only.
 */

#include	<vxWorks.h>
#include	<stdioLib.h>
#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include	<vme.h>
#include 	<wf_driver.h>
#include 	<task_params.h>
#include	<ai_driver.h>
#include	<drvsubs.h>

/* drvXy566.c -  Driver Support Routines for xy566 */

static char SccsId[] = "@(#)ai_xy566_driver.c	$Id$ ";

/* If any of the following does not exist replace it with #define <> NULL */

static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvXy566={
	2,
	report,
	init};

static long report()
{
    xy566_io_report();
    return(0);
}

static long init()
{

    return(0);
}


/* arrays which keep track of which cards are found at initialization */
struct ai566	*pai_xy566se[MAX_AI_XY_CARDS];
struct ai566	*pai_xy566di[MAX_AI_XY_CARDS];
struct ai566	*pai_xy566dil[MAX_AI_XY_CARDS]; 
unsigned short	*pai_xy566se_mem[MAX_AI_XY_CARDS];
unsigned short	*pai_xy566di_mem[MAX_AI_XY_CARDS];
unsigned short	*pai_xy566dil_mem[MAX_AI_XY_CARDS];

/* VME memory Short Address Space is set up in gta_init */
static char *shortaddr;
static char *stdaddr;


/* reset the counter interrupt                              0x8000 */
/* reset the sequence interrupt                             0x2000 */
/* enable the sequence interrupt                            0x1000 */
/* reset the trigger clock interrupt                        0x0800 */
/* enable the sequence controller			    0x0100 */
/* read values into first 32 words on each read             0x0040 */
/* read in sequential mode  (bit 0x0020 == 0)               0x0000 */
/* interrupt enable                                         0x0008 */
/* leds green-on red-off                                    0x0003 */

#define	XY566L_CSR	0xb94b
#define XY566_INT_LEVEL 6

/* forward references */
void 	senw();
VOID 	xy566_reset(); 
int	ai_xy566_init();
int	ai_xy566l_init();


static acro_intr(ap)
register struct acroregs *ap;
{
}

/*      The following two subroutines introduce a delay between
 *      successive writes to the 566. This is necessary for some
 *      parts of the card (i.e. the AM9513). It also insures
 *      that a value is actually written, instead of compiler
 *      generated bset or bclr instructions.
 */
static void senw (addr, val)
unsigned short *addr;
unsigned short val;
{
        *addr = val;
}
 
senb (addr, val)
unsigned char *addr;
unsigned char val;
{
        *addr = val;
}

ai566_intr(ap)
register struct ai566 *ap;
{
	/* wake up the I/O event scanner */
	io_scanner_wakeup(IO_AI,XY566DIL,ap->card_number);

	/* reset the CSR - needed to allow next interrupt */
	senw(&ap->a566_csr,XY566L_CSR);

}

/*
 *	ai_566_init ()
 *
 *	Initialize all VME analog input cards
 */

ai_566_init()
{
	/* intialize the Xycom 566 Unipolar Single Ended Analog Inputs */
	ai_xy566_init(pai_xy566se,ai_addrs[XY566SE],ai_num_channels[XY566SE],
        	ai_num_cards[XY566SE],ai_memaddrs[XY566SE],pai_xy566se_mem); 

	/* intialize the Xycom 566 Unipolar Differential Analog Inputs */
	ai_xy566_init(pai_xy566di,ai_addrs[XY566DI],ai_num_channels[XY566DI],
		ai_num_cards[XY566DI],ai_memaddrs[XY566DI],pai_xy566di_mem); 


	/* intialize the Xycom 566 Unipolar Differential Analog Inputs Latched */
        ai_xy566l_init(pai_xy566dil,ai_addrs[XY566DIL],ai_num_channels[XY566DIL],
		ai_num_cards[XY566DIL],ai_memaddrs[XY566DIL],pai_xy566dil_mem); 

	return (0);
}

/*
 *  Driver Support Routines for Ai
 *

/*
 * ai_XY566_INIT
 *
 * intialize the xycom 566 analog input card
 */
int ai_xy566_init(pcards_present,base_addr,num_channels,num_cards,paimem,pmem_present)
register struct ai566	**pcards_present;
register unsigned short	*base_addr;
register short		num_channels;
short			num_cards;
register char		*paimem;	/* mem loc of I/O buffer */
register short		**pmem_present;
{
	short		shval;
	register short	i,n;
	struct ai566	*pai566;	/* memory location of cards */
        char		*pai566io;	/* mem loc of I/O buffer */
        short status;

    /* map the io card into the VRTX short address space */
    if((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,base_addr, &pai566)) != OK){
    	logMsg("Addressing error in 566 analog input driver\n");
   	return ERROR;
    }

    /* map the io card into the standard address space */
    if((status = sysBusToLocalAdrs(VME_AM_STD_SUP_DATA,paimem, &pai566io)) != OK){
    	logMsg("Addressing error in 566 analog input driver\n");
   	return ERROR;
    }

    /* mark each card present into the card present array */
    for (i = 0; i < num_cards;
      i++, pai566++, pcards_present+=1, pai566io+=XY566_MEM_INCR,pmem_present+=1) {
	if (vxMemProbe(pai566,READ,sizeof(short),&shval) != OK){
		*pcards_present = 0;
		continue;
	}
	if (vxMemProbe(pai566io,READ,sizeof(short),&shval) != OK){
		*pcards_present = 0;
		continue;
	}

	*pcards_present = pai566;
	*pmem_present = (short *)pai566io;

	/* reset the Xycom 566 board */
	senw(&pai566->a566_csr,0x00);		/* off seq control */
	senw(&pai566->a566_csr,XY_SOFTRESET);	/* reset */
	senw(&pai566->a566_csr,XY_LED);		/* reset off,red off,green on */

	/* Am9513 commands */
	/* initialize the Am9513 chip on the XY566 */
 	senw(&pai566->stc_control,0xffff);	/* master reset */
	senw(&pai566->stc_control,0xff5f);	/* disarm all counters */
	senw(&pai566->stc_control,0xffef);	/* 16 bit mode */

	/* master mode register */
	senw(&pai566->stc_control,0xff17);	/* select master mode reg */
	senw(&pai566->stc_data,0x2200);		/* 16 bit, divide by 4 */

	/* counter two is used to set the time between sequences */
	senw(&pai566->stc_control,0xff02);	/*sel counter 2 */
	senw(&pai566->stc_data,0x0b02);		/* TC toggle mode */
	senw(&pai566->stc_control,0xffea);	/* TC output high */

	/* counter four is used as time between sequence elements */
	senw(&pai566->stc_control,0xff04);	/* sel counter 4 */
	senw(&pai566->stc_data,0x0b02);		/* TC toggle mode */
	senw(&pai566->stc_control,0xffec);	/* TC output high */

	/* counter five is used as an event counter */
	senw(&pai566->stc_control,0xff05);	/* sel counter 5 */
	senw(&pai566->stc_data,0x0b02);		/* TC toggle mode */
	senw(&pai566->stc_control,0xffed);	/* TC output high */

	/* set time between sequences */
	senw(&pai566->stc_control,0xff04);	/* sel counter 4 */
	senw(&pai566->stc_data,0x9525);		/* see Am9513A manual */
	senw(&pai566->stc_data,0x0014);		/* downcount value */
	senw(&pai566->stc_control,0xff68);	/* load & arm cntr 4 */

	senw(&pai566->stc_control,0xff05);	/* sel counter 4 */
	senw(&pai566->stc_data,0x97ad);		/* see Am9513A manual */
	senw(&pai566->stc_data,0x0100);		/* downcount value */
	senw(&pai566->stc_control,0xff70);	/* load & arm cntr 4 */
	/* end of the Am9513 commands */

	/* for each channel set gain and place into the scan list */
	for (n=0; n < num_channels; n++) {
		senb((&pai566->gram_base + n*2),0); /* gain == 1 */
		/* end of sequnce = 0x80 | channel */
		/* stop           = 0x40 | channel */
		senb((&pai566->sram_base+n*2),(n==num_channels-1)? n|0x80:n);
	}
	senw(&pai566->dram_ptr, 0);		/* data ram at 0 */
	senb(&pai566->sram_ptr, 0);		/* seq ram also at 0 */

        /* set the Xycom 566 board */
        /* reset the counter interrupt                              0x8000 */
        /* reset the sequence interrupt                             0x2000 */
        /* reset the trigger clock interrupt                        0x0800 */
        /* enable the sequence controller                           0x0100 */
        /* read values into first 32 words on each read             0x0040 */
        /* read in sequential mode  (bit 0x0020 == 0)               0x0000 */
        /* leds green-on red-off                                    0x0003 */
	senw(&pai566->a566_csr,0xa943 );	/* init csr */

	/* latch in the first bunch of data and start continuous scan */
	senb(&pai566->soft_start,0);
    } 
} 

/*
 * AI_XY566L_INIT
 *
 * intialize the xycom 566 latched analog input card
 */
int ai_xy566l_init(pcards_present,base_addr,num_channels,num_cards,paimem,pmem_present)
register struct ai566	**pcards_present;
register unsigned short	*base_addr;
register short		num_channels;
short			num_cards;
register char		*paimem;	/* mem loc of I/O buffer */
register short		**pmem_present;
{
    short			shval;
    register short		i,n;
    struct ai566	*pai566;	/* memory location of cards */
    char		*pai566io;	/* mem loc of I/O buffer */
    int status;

    /* map the io card into the VRTX short address space */
    if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,base_addr, &pai566)) != OK){
    	logMsg("Addressing error in 566 latched analog input driver\n");
   	return ERROR;
    }

    /* map the io card into the standard address space */
    if((status = sysBusToLocalAdrs(VME_AM_STD_SUP_DATA,paimem, &pai566io)) != OK){
    	logMsg("Addressing error in 566 analog input driver\n");
   	return ERROR;
    }

    rebootHookAdd(xy566_reset); 

    /* mark each card present into the card present array */
    for (i=0;i<num_cards;
      i++, pai566++, pcards_present+=1, pai566io+=XY566_MEM_INCR,pmem_present+=1){
	if (vxMemProbe(pai566,READ,sizeof(short),&shval) != OK){
		*pcards_present = 0;
		continue;
	}
	if (vxMemProbe(pai566io,READ,sizeof(short),&shval) != OK){
		*pcards_present = 0;
		continue;
	}
			
	*pcards_present = pai566;
	*pmem_present = (short *)pai566io;

	/* put the card number in the dual ported memory */
	senb(&pai566->card_number,i);

	/* set up the interrupt vector */
	/* taken from the XYCOM-566 Manual. Figure 4-6  Page 4-19 */
	pai566->int_vect = AI566_VNUM + i;
  
	intConnect((AI566_VNUM + i) * 4, ai566_intr, pai566);
        sysIntEnable(XY566_INT_LEVEL); 

	/* reset the Xycom 566 board */
	senw(&pai566->a566_csr,0x00);		/* off seq control */
	senw(&pai566->a566_csr,XY_SOFTRESET);	/* reset */
	senw(&pai566->a566_csr,XY_LED);		/* reset off,red off,green on */

	/* Am9513 commands */
	/* initialize the Am9513 chip on the XY566 */
 	senw(&pai566->stc_control,0xffff);	/* master reset */
	senw(&pai566->stc_control,0xff5f);	/* disarm all counters */
	senw(&pai566->stc_control,0xffef);	/* 16 bit mode */

	/* master mode register */
	senw(&pai566->stc_control,0xff17);	/* select master mode reg */
	senw(&pai566->stc_data,0x2200);		/* 16 bit, divide by 4 */

	/* counter two is used to set the time between sequences */
	senw(&pai566->stc_control,0xff02);	/*sel counter 2 */
	senw(&pai566->stc_data,0x0b02);		/* TC toggle mode */
	senw(&pai566->stc_control,0xffea);	/* TC output high */

	/* counter four is used as time between sequence elements */
	senw(&pai566->stc_control,0xff04);	/* sel counter 4 */
	senw(&pai566->stc_data,0x0b02);		/* TC toggle mode */
	senw(&pai566->stc_control,0xffec);	/* TC output high */

	/* counter five is used as an event counter */
	senw(&pai566->stc_control,0xff05);	/* sel counter 5 */
	senw(&pai566->stc_data,0x0b02);		/* TC toggle mode */
	senw(&pai566->stc_control,0xffed);	/* TC output high */

	/* set time between sequences */
	senw(&pai566->stc_control,0xff04);	/* sel counter 4 */
	senw(&pai566->stc_data,0x9525);		/* see Am9513A manual */
	senw(&pai566->stc_data,0x0014);		/* downcount value */
	senw(&pai566->stc_control,0xff68);	/* load & arm cntr 4 */

	senw(&pai566->stc_control,0xff05);	/* sel counter 4 */
	senw(&pai566->stc_data,0x97ad);		/* see Am9513A manual */
	senw(&pai566->stc_data,0x0100);		/* downcount value */
	senw(&pai566->stc_control,0xff70);	/* load & arm cntr 4 */
	/* end of the Am9513 commands */

	/* for each channel set gain and place into the scan list */
	for (n=0; n < num_channels; n++) {
		senb((&pai566->gram_base + n*2), 0); /* gain == 1 */

		/* end of sequnce = 0x80 | channel */
		/* stop           = 0x40 | channel */
		/* interrupt      = 0x20 | channel */
		senb((&pai566->sram_base+n*2),(n==num_channels-1)? n|0xe0:n);
	}
	senw(&pai566->dram_ptr,0);		/* data ram at 0 */
	senb(&pai566->sram_ptr,0);		/* seq ram also at 0 */

        /* initialize the control status register */
	/* reset the sequence interrupt                             0x2000 */
	/* enable the sequence interrupt                            0x1000 */
	/* reset the trigger clock interrupt                        0x0800 */
	/* enable the sequence controller			    0x0100 */
	/* read values into first 32 words on each read             0x0040 */
	/* read in sequential mode  (bit 0x0020 == 0)               0x0000 */
	/* interrupt enable                                         0x0008 */
	/* leds green-on red-off                                    0x0003 */
	senw(&pai566->a566_csr,XY566L_CSR);

    }
}


ai_xy566_driver(card,chan,type,prval)
register unsigned short	card;
unsigned short		chan;
register unsigned int	type;
register unsigned short *prval;
{
	/* the register short i is used to calculate the delay for the	*/
	/* conversion of the XYCOM 566. Make sure it stays a register	*/
	register unsigned short *pcard;
	register unsigned short i;

	/* check on the card and channel number as kept in module_types.h */
	if (card >= ai_num_cards[type]) return(-1);
	if (chan >= ai_num_channels[type]) return(-2);

	switch (type){

	case (XY566SE):
	{
		register struct ai566 *pai566;

		/* check card specified exists */
		if (pai_xy566se[card] == 0) return(-1);

		/* read the value from the Xycom data RAM area */
		*prval = pai_xy566se_mem[card][chan];

		return (0);
	}

	case (XY566DI):
	{
		register struct ai566 *pai566;

		/* check card specified exists */
		if (pai_xy566di[card] == 0) return(-1);

		/* read the value form the Xycom data RAM area */

		*prval = pai_xy566di_mem[card][chan];

                rval_convert(prval);
		return (0);
	}

	case (XY566DIL):
	{
		register struct ai566 *pai566;

		/* check card specified exists */
		if (pai_xy566dil[card] == 0) return(-1);

		/* read the value form the Xycom data RAM area */
		*prval = pai_xy566dil_mem[card][chan];

                rval_convert(prval);
		return (0);
	}

	return (0);

        }

}

/*
 * rval_convert 
 *          For 566_card -  values for XY566DI and XY566DIL
 *     come from the card as signed hex numbers( -0x7ff to +0x7ff).
 *     This subrountine converts them to unsigned hex numbers (0x0 -
 *     0x7ff.  Then it strips off the sign bit.
 *
*/
VOID rval_convert(rval)
   unsigned short *rval;
{
   
	*rval = *rval + 0x800; 

	 /* remove the sign bits. */

	*rval &= 0xfff;
}

/*
 *
 *     xy566_reset- Called for ctl X reboot.  The idea is to disable
 *     bits 3 and 12 of the CSR.
 *
*/

VOID xy566_reset(){
	unsigned short csr_val,shval;
	short int i; 
	struct ai566	*pai566;	/* memory location of cards */
	short int status;

	if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,ai_addrs[XY566DIL], &pai566)) != OK){
		logMsg("Addressing error in 566 latched analog input driver\n");
		return ERROR;
	}

	for (i=0;i<ai_num_cards[XY566DIL]; i++, pai566++ ){
		if (vxMemProbe(pai566,READ,sizeof(short),&shval) != OK){
			continue;
		}
		csr_val =(unsigned short )XY566L_CSR;
		csr_val &= 0xeff7;
		pai566->a566_csr = csr_val;
        }
}




/*
 *  io_report (or dbior) subroutine for 566 card.
 *
 * 
 *
 * .c -  Driver Support Routines for Ai
*/

static long report();
static long init();

int ai_xy566_io_report(level)
  char level;
{
    short i,j;
    unsigned short rval;
   
    
    for (i = 0; i < MAX_AI_XY_CARDS; i++){
	if (pai_xy566se[i]){	
            printf("AI: XY566-SE:   card %d\n",i);
            if (level == 1){
               xy566_rval_report(i,XY566SE); 
            }    
            
        }
    }
    for (i = 0; i < MAX_AI_XY_CARDS; i++){
	if (pai_xy566di[i]){	
            printf("AI: XY566-DI:   card %d\n",i);
            if (level == 1){
               xy566_rval_report(i,XY566DI); 
            } 
        }

     }
       
    for (i = 0; i < MAX_AI_XY_CARDS; i++){
	if (pai_xy566dil[i]){
            printf("AI: XY566-DIL:   card %d\n",i); 
            if (level == 1){
               xy566_rval_report(i,XY566DIL); 
            } 
        }
    }
    return OK;
}

/*
 *  xy566_rval_report -reports the raw value for every channel on the card  
 * 
 *  called by ai_xy566_io_report if level is 1 
 *
 */
VOID  xy566_rval_report(card,type)
   short int card,type;
 { 
	short	i,j,k,l,m,num_chans;
        unsigned short jrval,krval,lrval,mrval;

        printf("\n");

	num_chans = ai_num_channels[type];

        for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m < num_chans;
            j+=IOR_MAX_COLS,k+=IOR_MAX_COLS,l+= IOR_MAX_COLS,m +=IOR_MAX_COLS){
        	if(j < num_chans){
                     if( ai_driver(card,j,type,&jrval) == 0){       
                	printf("Chan %d = %x \t",j,jrval);
                     }else 
                        printf("READ_ALARM\n");  
                }
                  
         	if(k < num_chans){
                     if(ai_driver(card,k,type,&krval) == 0){       
                	printf("Chan %d = %x \t",k,krval);
                     } else 
                        printf("READ_ALARM\n");  
                }
                if(l < num_chans){
                     if( ai_driver(card,l,type,&lrval) == 0){       
                	printf("Chan %d = %x \t",l,lrval);
                     } else 
                        printf("READ_ALARM\n");  
                  }
                   if(m < num_chans){
                     if( ai_driver(card,m,type,&mrval) == 0){      
                 	printf("Chan %d = %x \n",m,mrval);
                     }
                     else 
                        printf("READ_ALARM\n");  
                  }
     }
 }


/* Driver for 566 card as wave form card. 
 *
 * Modification Log
 * -----------------
 * .00	09-14-88	lrd	check for IGEN card present before initing
 * .01	09-27-88	lrd	removed call to xy_mem_init
 * .02	10-04-88	lrd	remove IGEN support
 * .03	10-07-88	lrd	add support for the Xycom 085 arm mechanism
 * .04	04-17-89	lrd	add callback mechanism for data take complete
 * .05	02-15-90	lrd	modified for new 085 card
 * .05	07-27-89	lrd	modified to use channel 0 not channel 1
 *	02/04/91	ges	Change taskDelay from 6 to 2 in "xy566DoneTask".
 *				Allows rearm and data reads for successive
 *					waveform scans up thru 10 Hz rates.
 * .07	10/30/91	bg	Combined the xy566 waveform driver with the
 *                               xy566 analog in drivers. Changed addressing to
 *                               use sysBusToLocalAddrs and VME_AM_STD_SUP or
 *                               VME_AM_SUP_SHORT_IO
 * .08	03/20/92	bg	Added the argument level to io_report, but so far
 *                              it doesn't do anything. Will add ability to report
 *                              ability to read out raw value if there is a demand.
 */

/*
 * Code Portions
 *
 * xy566DoneTask	Task to handle data take completion for the 566 waveform
 * xy566_init		Initializes the 566 waveform cards
 * senb/senw		Writes to the 566 where the call provides a req'd delay
 */


/* arrays which keep track of which cards are found at initialization */
struct ai566	*pwf_xy566[MAX_AI_XY_CARDS];
struct wf085	*pwf_xy085[MAX_AI_XY_CARDS];
char		*pwf_mem[MAX_AI_XY_CARDS];

/* the routine to call when the data is read and the argument to send */
unsigned int	*pargument[MAX_AI_XY_CARDS];
unsigned int	*proutine[MAX_AI_XY_CARDS];

/* VME memory Short Address Space is set up in gta_init */

int		wfDoneId;	/* waveform done task ID */

/* forward references */


/*
 * xy566_driver
 *
 */
xy566_driver(slot,pcbroutine,parg)
register unsigned short	slot;
register unsigned int	*pcbroutine;
register unsigned int	*parg;	/* number of values read */
{
	register struct ai566 *pwf566;
	register struct wf085 *pwf085;

	/* slot range checking occurs in wf_driver.c */

	/* is the Xycom 566 card present */
	if ((pwf566 = pwf_xy566[slot]) == 0)
			return(-1);
	if ((pwf085 = pwf_xy085[slot]) == 0)
			return(-1);

	/* armed already by someone else */
	if (proutine[slot] != 0)
		return(-2);	/* by someone else */

	/* set the Xycom 566 board */
	senw(&pwf566->dram_ptr,0); /* RAM pointer to 0 */
	senw(&pwf566->sram_ptr,0); /* sequence pointer to 0 */

	/* keep the pointer to the value field */
	proutine[slot] = pcbroutine;
	pargument[slot] = parg;

	/* arm the encoder pulse mechanism */
	senw(&pwf085->csr,XY_ARM | XY_LED | 0x20);	/* set high */
	senw(&pwf085->csr,XY_LED | 0x20);		/* set low */

	return(0);
}

/*
 * xy566DoneTask
 *
 * polls the busy bit on the Xycom 566 latched waveform records
 * The busy bit is set externally when data collection is completed
 */
xy566DoneTask(){
    register unsigned int	**pproutines;
    register unsigned int	(*pcbroutine)();
    register short		i;

    while(TRUE){
	pproutines = proutine;
 	for (i=0;  i<MAX_AI_XY_CARDS; i++,pproutines++){

		/* check if the waveform is armed */
		if (*pproutines == 0) continue;

		/* check if the data is taken */
		if ((pwf_xy085[i]->csr & XY_BUSY) == 0){
			/* callback routine when data take completed */
			(unsigned int *)pcbroutine = *pproutines;
			(*pcbroutine)
			    (pargument[i],pwf_xy566[i]->dram_ptr,pwf_mem[i]);

			/* reset for someelse to claim */
			*pproutines = 0;
			pargument[i] = 0;
		}
	}

	/* sleep for .1 second - system degrade will slow this task */
	/* that's OK						    */
	taskDelay(2);	/* ges: changed from 6 ticks to 2 ticks 2/4/91 */
    }
}

/*
 * XY566_INIT
 *
 * intialize the xycom 566 waveform input card
 */
xy566_init()
{
	register struct ai566	**pcards_present = pwf_xy566;
	register struct wf085	**parms_present = pwf_xy085;
	register char		**pmem_present = pwf_mem;

	short			shval,status;
	register short		i,got_one;
	register struct ai566	*pwf566;	/* VME location of cards */
	struct wf085		*pwf085;	/* VME location of arm module */
	char			*pwfMem;	/* VME 566 memory buffer */

    /* map the io card into the VRTX short address space */
    if(( status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,0, &shortaddr)) != OK){
    	logMsg("Addressing error in 566 wf driver\n");
   	return ERROR;
    }
    pwf085 = (struct wf085 *)((int)shortaddr + (int)wf_armaddrs[XY566WF]);
    pwf566 = (struct ai566 *)((int)shortaddr + (int)wf_addrs[XY566WF]);

    if ((status = sysBusToLocalAdrs(VME_AM_STD_SUP_DATA,wf_memaddrs[XY566WF],&pwfMem)) != OK){
    	logMsg("Addressing error in 566 wf ram\n");
   	return ERROR;
    }
 


    /* mark each card present into the card present array */
    got_one = 0;
    for (i = 0;
      i < wf_num_cards[XY566WF];
      i++, pwf566++,pwf085++,pwfMem += XY566_MEM_INCR, pcards_present += 1) {

	/* is the Xycom 566 here */
	if (vxMemProbe(pwf566,READ,sizeof(short),&shval) != OK){
		*pcards_present = 0;
		continue;
	}
	/* is the Xycom 566 memory here */
	if (vxMemProbe(pwfMem,READ,sizeof(short),&shval) != OK){
		*pcards_present = 0;
		continue;
	}
	/* is the Xycom 085 used as the arming mechanism present */
	if (vxMemProbe(pwf085,READ,sizeof(short),&shval) != OK){
		*pcards_present = 0;
		continue;
	}

	got_one = 1;
	*pcards_present = pwf566;
	*parms_present = pwf085;
	*pmem_present = pwfMem;

	/* taken from the XYCOM-566 Manual. Figure 4-6  Page 4-19 */
	/* reset the Xycom 566 board */
	senw (&pwf566->a566_csr,0x00);		/* off seq control */
	senw (&pwf566->a566_csr,XY_SOFTRESET);	/* reset */
	senw (&pwf566->a566_csr,XY_LED);	/* reset off,red off,green on */

	/* Am9513 commands */
	/* initialize the Am9513 chip on the XY566 */
 	senw (&pwf566->stc_control, 0xffff);	/* master reset */
	senw(&pwf566->stc_control, 0xff5f);	/* disarm all counters */
	senw(&pwf566->stc_control, 0xffef);	/* 16 bit mode */

	/* master mode register */
	senw(&pwf566->stc_control, 0xff17);	/* select master mode reg */
	senw(&pwf566->stc_data, 0x2200);	/* 16 bit, divide by 4 */

	/* counter two is used to set the time between sequences */
	senw(&pwf566->stc_control, 0xff02);	/*sel counter 2 */
	senw(&pwf566->stc_data, 0x0b02);	/* TC toggle mode */
	senw(&pwf566->stc_control, 0xffea);	/* TC output high */

	/* counter four is used as time between sequence elements */
	senw(&pwf566->stc_control, 0xff04);	/* sel counter 4 */
	senw(&pwf566->stc_data, 0x0b02);	/* TC toggle mode */
	senw(&pwf566->stc_control, 0xffec);	/* TC output high */

	/* counter five is used as an event counter */
	senw(&pwf566->stc_control, 0xff05);	/* sel counter 5 */
	senw(&pwf566->stc_data, 0x0b02);	/* TC toggle mode */
	senw(&pwf566->stc_control, 0xffed);	/* TC output high */

	/* set counter 4 */
	/* active high level gate N			0x8000		*/
	/* count on the falling edge			0x1000		*/
	/* SRC 5					0x0500		*/
	/* disable special gate				0x0080 = 0	*/
	/* reload from load				0x0040 = 0	*/
	/* count repetitively				0x0020 = 1	*/
	/* binary count					0x0010 = 0	*/
	/* count down					0x0008 = 0	*/
	/* active low terminal count pulse		0x0007 = 5	*/
	senw(&pwf566->stc_control,0xff04);	/* sel counter 4 */
	senw(&pwf566->stc_data,0x9525);		/* see comment above*/
	senw(&pwf566->stc_data,0x0014);		/* downcount value */
	senw(&pwf566->stc_control,0xff68);	/* load & arm cntr 4 */

	/* set counter 5 */
	/* active high level gate N			0x8000		*/
	/* count on the falling edge			0x1000		*/
	/* GATE 2					0x0700		*/
	/* enable special gate				0x0080 = 1	*/
	/* reload from load				0x0040 = 0	*/
	/* count repetitively				0x0020 = 1	*/
	/* binary count					0x0010 = 0	*/
	/* count up					0x0008 = 1	*/
	/* active low terminal count pulse		0x0007 = 5	*/
	senw(&pwf566->stc_control,0xff05);	/* sel counter 5 */
	senw(&pwf566->stc_data,0x97ad);	/* see comment above */
	senw(&pwf566->stc_data,0x0100);	/* count value */
	senw(&pwf566->stc_control,0xff70);	/* load & arm cntr 5*/
	/* end of the Am9513 commands */

	/* Xycom gain RAM */
	senb(&pwf566->gram_base,0);		/* set gain to 1 ch0*/

	/* Xycom sequence RAM */
	senb(&pwf566->sram_base,0xc0);	/* read only the 0th channel */

	/* Xycom data RAM index */
	senw(&pwf566->dram_ptr,0);		/* data ram at 0 */

	/* Xycom sequential RAM index */
	senb(&pwf566->sram_ptr,0);		/* seq ram also at 0 */

	/* set the Xycom 566 board */
	/* reset the counter interrupt                              0x8000 */
	/* reset the sequence interrupt                             0x2000 */
	/* reset the trigger clock interrupt                        0x0800 */
	/* enable the sequence controller                           0x0100 */
	/* read values into data RAM contiguously (bit 0x0040 == 0) 0x0000 */
	/* read in sequential mode  (bit 0x0020 == 0)               0x0000 */
	/* leds green-on red-off                                    0x0003 */
	senw(&pwf566->a566_csr,0xa903);	/* init csr */

	/* initialize the xycom 085 used as the arming mechanism */
	/* leds green-on red-off          0x0003 */
	senw(&pwf085->csr,XY_LED | 0x20);	/* init csr */
    }

    /* start the 566 waveform readback task */
    if (got_one)
	wfDoneId = 
	   taskSpawn(WFDONE_NAME,WFDONE_PRI,WFDONE_OPT,WFDONE_STACK,xy566DoneTask);

    return 0;
}


/*      
 * XY566_IO_REPORT
 *
 *
 *
 *
 */
xy566_io_report(level)
 short int level;
{
	int i,xy566_bytes;

	/* report all of the xy566 waveform inputs present */
	for (i = 0; i < wf_num_cards[XY566WF]; i++)
		if (pwf_xy566[i]){
         	  	printf("WF: XY566:      card %d\n",i);
                }
                           
}


