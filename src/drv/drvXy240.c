/* xy240_driver.c */
/* share/src/drv @(#)xy240_driver.c	1.1     6/25/92 */
/*
 *	routines used to test and interface with Xycom240
 *	digital i/o module
 *
 * 	Author:      B. Kornke
 * 	Date:        11/20/91
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
 * .01	06-25-92	bg	Added driver to code.  Added xy240_io_report
 *				to it. Added copyright disclaimer.
 * .02	08-10-92	joh	merged xy240_driver.h into this source
 * .03	08-11-92	joh	fixed use of XY240 where XY240_BI or XY240_BO
 *				should have been used
 * .04  08-11-92	joh	now allows for runtime reconfiguration of
 *				the addr map
 * .05	08-25-92        mrk     added DSET; made masks a macro
 */

#include "vxWorks.h"
#include "taskLib.h"
#include "vme.h"
#include "module_types.h"
#include <drvSup.h>

static long report();
static long init();

struct {
        long    number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvXy240={
        2,
        report,
        init};
static long report(level)
    int level;
{
    xy240_io_report(level);
    return(0);
}

static long init()
{

    xy240_init();
    return(0);
}



#define XY240_ADDR0	(bi_addrs[XY240_BI])
#define XY240_MAX_CARDS	(bi_num_cards[XY240_BI])
#define XY240_MAX_CHANS	(bi_num_channels[XY240_BI])

/*xy240 memory structure*/
struct dio_xy240{
        char            begin_pad[0x80];        /*go to interface block*/
        unsigned short  csr;                    /*control status register*/
        unsigned short  isr;                    /*interrupt service routine*/
        unsigned short  iclr_vec;               /*interrupt clear/vector*/
        unsigned short  flg_dir;                /*flag&port direction*/
        unsigned short  port0_1;                /*port0&1 16 bits value*/
        unsigned short  port2_3;                /*por2&3 16 bits value*/
        unsigned short  port4_5;                /*port4&5 16 bits value*/
        unsigned short  port6_7;                /*port6&7 16 bits value*/
        char            end_pad[0x400-0x80-16]; /*pad til next card*/
};

/*create dio control structure record*/
struct dio_rec
        {
        struct dio_xy240 *dptr;                 /*pointer to registers*/
        short num;                              /*device number*/
        short mode;                             /*operating mode*/
        unsigned short sport0_1;                /*saved inputs*/
        unsigned short sport2_3;                /*saved inputs*/
        /*short dio_vec;*/                      /*interrupt vector*/
        /*unsigned int intr_num;*/              /*interrupt count*/
        };


LOCAL
struct dio_rec *dio;	/*define array of control structures*/

/*dio_int
 *
 *interrupt service routine
 *
 */
#if 0
dio_int(ptr)
 register struct dio_rec *ptr;
{
 register struct dio_xy240 *regptr;

 regptr = ptr->dptr;

}
#endif

/*
 *
 *dio_scan
 *
 *task to check for change of state
 *
 */
 extern int	wakeup_init;
dio_scan()
 
{	
	int i;
	int	first_scan,first_scan_complete;

 first_scan = first_scan_complete = 0;
 for (;;) 
 {
   if (wakeup_init & !first_scan_complete) first_scan = 1;

   for (i = 0; i < XY240_MAX_CARDS; i++)
    {
    if (dio[i].dptr)
     if (((dio[i].dptr->port0_1) ^ (dio[i].sport0_1)) || 
		((dio[i].dptr->port2_3) ^ (dio[i].sport2_3))
		|| first_scan)
      {
	 /* printf("io_scanner_wakeup for card no %d\n",i); */
	  io_scanner_wakeup(IO_BI,XY240_BI,i);	  
	  dio[i].sport0_1 = dio[i].dptr->port0_1;
	  dio[i].sport2_3 = dio[i].dptr->port2_3;	  
	  }
    }
    if (first_scan){
	first_scan = 0;
	first_scan_complete = 1;
    }
    taskDelay(1);	
  }
}


/*DIO DRIVER INIT
 *
 *initialize xy240 dig i/o card
 */
xy240_init()
{
	short junk;
	register short i;
	struct dio_xy240	*pdio_xy240;
	static char *name = "scan";
	int tid,status;

	/*
	 * allow for runtime reconfiguration of the
	 * addr map
	 */
	dio = (struct dio_rec *) calloc(XY240_MAX_CARDS, sizeof(*dio));
	if(!dio){
		return ERROR;
	}

	status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,XY240_ADDR0,&pdio_xy240);
        if (status != OK){
           printf("Addressing error in xy240 driver\n");
           return ERROR;
        }

        for (i = 0; i < XY240_MAX_CARDS; i++, pdio_xy240++){
	  
	if (vxMemProbe(pdio_xy240,READ,2,&junk) == OK){
     

/*
register initialization
*/
		pdio_xy240->csr = 0x3;
		pdio_xy240->iclr_vec = 0x00;				/*clear interrupt input register latch*/
		pdio_xy240->flg_dir = 0xf0;					/*ports 0-3,input;ports 4-7,output*/
		dio[i].sport2_3 = pdio_xy240->port2_3;		/*read and save high values*/
                dio[i].dptr = pdio_xy240;
							
		}
	else{
	   dio[i].dptr = 0;
	    }
}
				
 if (dio[i].dptr) 
  {
   if ((tid = taskNameToId(name)) != ERROR)
   taskDelete(tid);
   if (taskSpawn(name,111,VX_SUPERVISOR_MODE|VX_STDIO,1000,dio_scan) == ERROR)
  printf("Unable to create scan task\n");
  }
  return(0);
} 	

/*
 * XY240_BI_DRIVER
 *
 *interface to binary inputs
 */

xy240_bi_driver(card,mask,prval)
    register unsigned short card;
	unsigned int			mask;
	register unsigned int	*prval;
{
	register unsigned int	work;
 		if ((card >= XY240_MAX_CARDS) || (!dio[card].dptr)) 
		 return -1;
/*		 printf("%d\n",dio[card].num);*/
		work = (dio[card].dptr->port0_1 << 16)
			 + dio[card].dptr->port2_3;
		*prval = work & mask;
    return(0);
}

/*
 *
 *XY240_BO_READ
 *
 *interface to binary outputs
 */

xy240_bo_read(card,mask,prval)
	register unsigned short card;
	unsigned int			mask;
	register unsigned int	*prval;
 {
	register unsigned int	work;
 		if ((card >= XY240_MAX_CARDS) || (!dio[card].dptr)){ 
			 return -1;
                }
              
		/* printf("%d\n",dio[card].num); */
	   	work = (dio[card].dptr->port4_5 << 16)
			 + dio[card].dptr->port6_7;
                 		
		*prval = work &= mask;
        return(0);
 }

/* XY240_DRIVER
 *
 *interface to binary outputs
 */

xy240_bo_driver(card,val,mask)
	register unsigned short card;
	unsigned int			mask;
	register unsigned int	val;
 {
	register unsigned int	work;
 		if ((card >= XY240_MAX_CARDS) || (!dio[card].dptr)) 
		 return -1;

 /* use structure to handle high and low short swap */
                /* get current output */

		work = (dio[card].dptr->port4_5 << 16)
			 + dio[card].dptr->port6_7;

		work = (work & ~mask) | (val & mask);

		dio[card].dptr->port6_7 = (unsigned short)(work >> 16);
		dio[card].dptr->port4_5 = (unsigned short)work;
		return(0);
 }


/*dio_out
 *
 *test routine for xy240 output 
 */
dio_out(card,port,val)
	unsigned short		card,port,val;
	
{

 if ((card > XY240_MAX_CARDS-1)) 		/*test to see if card# is allowable*/
  {
  printf("card # out of range\n");
  return -1;
  }
 else if (!dio[card].dptr)				/*see if card exists*/
  {
  printf("can't find card %d\n", card);
  return -2;
  }
 else if ((port >7) || (port <4))			/*make sure they're output ports*/
  {
  printf("use ports 4-7\n");
  return -3;
  }
 else if (port == 4)
  {
  dio[card].dptr->port4_5 = val<< 8;
  return -4;
  }
 else if (port == 5)
  {
  dio[card].dptr->port4_5 = val;
  return -5;
  }
 else if (port == 6)
  {
  dio[card].dptr->port6_7 = val<< 8;
  return -6;
  }
else if (port == 7)
  {
  dio[card].dptr->port6_7 = val;
  return -7;
  }
  return(0);
}
 
/*XY240_WRITE
 *
 *command line interface to test bo driver
 *
 */
xy240_write(card,val)
        short card;
        unsigned int val;
 {
  return xy240_bo_driver(card,val,0xffffffff);
 }

#define masks(K) ((1<<K))
xy240_io_report(level)
short int level;
{
  	short int i;

        for (i = 0; i < XY240_MAX_CARDS; i++){
        
                if(dio[i].dptr){
                   printf("B*: XY240:\tcard %d\n",i);
                   if (level >= 1){
			xy240_bi_io_report();
			xy240_bo_io_report();
                   }
                }
        }
 
}

                          
xy240_bi_io_report(){
        short int num_chans,i,j,k,l,m,status;
        int ival,jval,kval,lval,mval;
        unsigned int   *prval;
        short int first_time = 0;

	printf("\tBinary In  Channels 0 - 31\n");

        num_chans = XY240_MAX_CHANS;

        for (i = 0; i < XY240_MAX_CARDS; i++){
                if(dio[i].dptr){
                        if(first_time < 1){
				printf("XY240 BINARY IN CHANNELS:\n");
                                first_time = 1;
                        }
			for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l< num_chans,
				m < num_chans; j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,
				m += IOR_MAX_COLS){
                         

				if(j < num_chans){
					xy240_bi_driver(i,masks(j),&jval);
					if (jval != 0) 
						jval = 1;
					printf("Chan %d = %x\t ",j,jval);
				}
				if(k < num_chans){
					xy240_bi_driver(i,masks(k),&kval);
					if (kval != 0) 
						kval = 1;
					printf("Chan %d = %x\t ",k,kval);
				}
				if(l < num_chans){
					xy240_bi_driver(i,masks(l),&lval);
					if (lval != 0) 
						lval = 1;
					printf("Chan %d = %x \t",l,lval);
				}
				if(m < num_chans){
					xy240_bi_driver(i,masks(m),&mval);
					if (mval != 0) 
						mval = 1;
	                               printf("Chan %d = %x \n",m,mval);
                                }
 
                        }
		}              
	}

}
xy240_bo_io_report(){
        short int num_chans,i,j,k,l,m,status;
        int ival,jval,kval,lval,mval;
        unsigned int   *prval;
        short int first_time = 0;

	printf("\tBinary Out Channels 0 - 31\n");

        num_chans = XY240_MAX_CHANS;

        for (i = 0; i < XY240_MAX_CARDS; i++){
                if(dio[i].dptr){
                        if(first_time < 1){
				printf("XY240 BINARY OUT CHANNELS:\n");
                                first_time = 1;
                        }
		for(j=0,k=1,l=2,m=3;j < num_chans,k < num_chans, l < num_chans,m
			< num_chans; j+=IOR_MAX_COLS,k+= IOR_MAX_COLS,l+= IOR_MAX_COLS,
			m += IOR_MAX_COLS){

			if(j < num_chans){
				xy240_bo_read(i,masks(j),&jval);
				if (jval != 0) 
					jval = 1; 
                                printf("Chan %d = %x\t ",j,jval);
                               }
                               if(k < num_chans){
				xy240_bo_read(i,masks(k),&kval);
				if (kval != 0) 
					kval = 1; 
 				printf("Chan %d = %x\t ",k,kval);
				}
                               if(l < num_chans){
                                       xy240_bo_read(i,masks(l),&lval);
                                       if (lval != 0) 
                                               lval = 1; 
                                       printf("Chan %d = %x \t",l,lval);
                               }
                               if(m < num_chans){
                                       xy240_bo_read(i,masks(m),&mval);
                                       if (mval != 0) 
                                               mval = 1; 
	                               printf("Chan %d = %x \n",m,mval);
                                }
 
                        }
		}
	}
}

