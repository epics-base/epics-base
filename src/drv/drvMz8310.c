
/* drvMz8310.c */
/* share/src/drv @(#)drvMz8310.c	1.9     8/27/92 */
/* 
 * Routines specific to the MZ8310 Low level routines for the AMD STC in
 * stc_driver.c
 *	Author:	Jeff Hill
 *	Date:	Feb 1989
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
 * Modification History 
 * joh	02-20-89	Init Release 
 * joh 	04-28-89	Added read back
 * joh	11-17-89	added readback to io report
 * joh	12-10-89	DB defaults the internal/external clock
 * 			parameter to 0 or external clock.  This was the opposite 
 *			of what this driver expected. Fix was made here.
 * joh	07-06-90	print channel number with channel value in IO report
 * joh	02-25-91	made ext/int clk IO report more readable
 * joh	09-05-91	converted to v5 vxWorks
 *  bg	09-15-91	added sysBustoLocalAdrs() for addressing
 *  bg	03-10-92	added the argument, level, to mz310_io_report().
 *  bg	04-27-92	added rebootHookAdd and mz8310_reset so ioc will
 *                        not hang on ctl X reboot.
 * joh	04-28-92	added arguments to MACROS which had hidden
 *			parameters
 *  bg	06-25-92	combined drvMz8310.c and mz8310_driver.c
 *  bg	06-26-92	Added level to mz8310_io_report.
 * joh  08-05-92	callable interface now conforms with epics standard
 */

/* drvMz8310.c -  Driver Support Routines for Mz8310 */
#include	<vxWorks.h>
#include	<stdioLib.h>
#include 	<vme.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include 	<fast_lock.h>

#ifdef V5_vxWorks
#	include <iv.h>
#else
#	include <iv68k.h>
#endif


/* If any of the following does not exist replace it with #define <> NULL */
long mz8310_io_report();
long mz8310_init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvMz8310={
	2,
	mz8310_io_report,
	mz8310_init};

#define MZ8310CHIPSIZE		0x20
#define MZ8310SIZE		0x00000100
#define MZ8310BASE(CARD)	(shortaddr+tm_addrs[MZ8310]+(CARD)*MZ8310SIZE)

#define MZ8310DATA		0
#define MZ8310CMD		3
#define MZ8310CHANONCHIP	5
#define MZ8310CHIPCOUNT		2
#define MZ8310CHANCNT		(MZ8310CHANONCHIP*MZ8310CHIPCOUNT)

/*	
	NOTE: The mizar draftsman has labeled the chip at the 
	highest address as one and the chip at the lowest address
	2 so I am reversing the chip number below.
*/
#define CHIP_REVERSE(CHIP)	(MZ8310CHIPCOUNT-1-(CHIP))
#define CHIP_ADDR(CARD,CHIP)	(MZ8310BASE(CARD)+\
				(CHIP_REVERSE(CHIP)*MZ8310CHIPSIZE))

#define MZ8310_CMD_ADDR(CARD,CHIP)\
((unsigned char *)  CHIP_ADDR(CARD,CHIP) + MZ8310CMD)
#define MZ8310_DATA_ADDR(CARD,CHIP)\
((unsigned short *) CHIP_ADDR(CARD,CHIP) + MZ8310DATA)
#if  0
#define MZ8310VECBASE(CARD,CHIP)\
((unsigned char *)  CHIP_ADDR(CARD,CHIP) + 0x41)
#endif

#define MZ8310VECSIZE		(0x20)
#define MZ8310INTCNT		4
#define MZ8310FIRSTINTCHAN	0
#define MZ8310INTVEC(CARD,CHAN)\
(MZ8310_INT_VEC_BASE + (CARD*MZ8310INTCNT) + mz8310_strap[CHAN].vec_num)

#define MZ8310_INTERUPTABLE(CHAN)  (mz8310_strap[CHAN].vec_addr)

# define INT_TICKS 4.0e06	/* speed of F1 in Hz  			*/
# define EXT_TICKS 5.0e06	/* GTA std speed of SRC1 in Hz	 	*/


struct mz8310_int_conf{
  void			(*user_service)();
  int			user_param;
  unsigned int		cnt;
};

struct	mz8310_conf{
  char				init;
  FAST_LOCK			lock;
  struct mz8310_int_conf 	icf[MZ8310CHANCNT];
};

struct mz8310_strap_info{
  unsigned char		irq;		/* the level at which the chan gen ints */
  unsigned char		vec_num;	/* really a vec offset-see MZ8310INTVEC */
  unsigned char		vec_addr;	/* offset from card base address */
};

static char 			*shortaddr;

static struct mz8310_conf 	*mzconf;
static unsigned int 		mz8310_card_count;

/* 
  only 4 unique interrupts per card but any channel can potentially 
  generate an interrupt depending on board strapping.

  NOTE:  existence of vec addr tells the driver that that channel is 
  strapped for interrupts since the card can't be polled for this info.

  	In the MIZAR 8310 Documentation:

  	Designation	vector reg offset
	IRQA		0x41	
	IRQB		0x61
	IRQC		0x81
	IRQD		0xa1
*/

static struct mz8310_strap_info mz8310_strap[MZ8310CHANCNT] =
{ 
  {	NULL,	NULL,	NULL	}, 	/* channel 0  */
  {	NULL,	NULL,	NULL	}, 	/* channel 1  */
  {	NULL,	NULL,	NULL	}, 	/* channel 2  */
  {	NULL,	NULL,	NULL	}, 	/* channel 3  */
  {	NULL,	NULL,	NULL	}, 	/* channel 4  */
  {	NULL,	NULL,	NULL	},	/* channel 5  */
  {	1,	0,	0x41	}, 	/* channel 6  */
  {	3,	1,	0x61	}, 	/* channel 7  */
  {	5,	2,	0x81	}, 	/* channel 8  */
  {	6,	3,	0xa1	} 	/* channel 9  */
};

/* forward reference. */

void mz8310_reset();
int mz8310_io_report_card(int card, int level);


long mz8310_io_report(level)
int 	level;
{
	unsigned	card;

    	for(card=0; card<tm_num_cards[MZ8310]; card++) 
		mz8310_io_report_card(card,level);

	return OK;
}



LOCAL
int mz8310_io_report_card(int card, int level)
{
  unsigned int channel, chip;
  int status;

  for(chip=0; chip<MZ8310CHIPCOUNT; chip++){
      status = stc_io_report(
			MZ8310_CMD_ADDR(card,chip), 
			MZ8310_DATA_ADDR(card,chip));
      if(status==ERROR){
        return status;
      }
  }

  printf("TM: MZ8310:\tcard %d\n", card);


  if (mzconf && card<mz8310_card_count && level){
      for(channel=0;channel<MZ8310CHANCNT;channel++){
         status =  mz8310_read_test(card, channel);
         if(status==ERROR){
         return status;
       }

      }
   }
  return OK;
}


long mz8310_init()
{
  register unsigned int 	card;
  register int 			status;
  struct mz8310_conf 		*temp_mzconf;
  register unsigned int 	card_count = tm_num_cards[MZ8310];

  if ((status = sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,0,&shortaddr)) != OK){ 
  	printf("Addressing error for short address in mz8310 driver\n");
        return ERROR;
  } 
  
  temp_mzconf = (struct mz8310_conf *) malloc(sizeof(*mzconf)*card_count);
  if(!temp_mzconf)
    return ERROR;

  for(card=0; card<card_count; card++){
    FASTLOCKINIT(&temp_mzconf[card].lock);
  }

  if(mzconf){
    for(card=0; card<card_count; card++){
      if(FASTLOCKFREE(&mzconf[card].lock)<0)
        logMsg("mz8310_init: error freeing sem\n");
    }
    if(free(mzconf)<0)
      logMsg("mz8310_init: error freeing device memory\n");
  }

  mzconf = temp_mzconf;

  for(card=0; card<card_count; card++){
    status = mz8310_init_card(card);
    if(status<0){
      break;
    }
  }

  mz8310_card_count = card;
  
  rebootHookAdd(mz8310_reset); 

  return OK;
}


/*
Locking for this provided by mz8310_init()
*/
mz8310_init_card(card)
register int 	card;		/* 0 through ... 	*/
{
  register int 	chip;
  register int	error;
  register int	chan;
  /*
  binary division
  data ptr seq enbl
  16 bit bus
  FOUT on 
  FOUT divide by one
  FOUT source (F1)
  Time of day disabled
  */
  register unsigned short master_mode = 0x2100;

  for(chip=0; chip< MZ8310CHIPCOUNT; chip++){
    error = stc_init(
		MZ8310_CMD_ADDR(card,chip), 
		MZ8310_DATA_ADDR(card,chip), 
		master_mode);
    if(error!=OK)
      return ERROR;
  }

  for(chan=0; chan<MZ8310CHANCNT; chan++)
    mz8310_setup_int(card,chan);

  mzconf[card].init = TRUE;

  return OK;
}


/*
  Use probe for init safe writes
  (locked by calling routines)
*/
mz8310_setup_int(card, channel)
register unsigned int card;
register unsigned int channel;
{
  unsigned char	vector;
  register int	status;
  register int 	chip = channel/MZ8310CHANONCHIP;
  void		mz8310_int_service();

  mzconf[card].icf[channel].user_service = NULL;
  mzconf[card].icf[channel].user_param = NULL;
  mzconf[card].icf[channel].cnt = 0;

  /*
  Is this channel strapped for interrupts
  */
  if(!MZ8310_INTERUPTABLE(channel))
    return OK;

  vector = MZ8310INTVEC(card,channel);
  if(vxMemProbe(	MZ8310BASE(card) + mz8310_strap[channel].vec_addr,
			WRITE,
			sizeof(char),
			&vector) != OK){
    logMsg("mz8310_driver: int not setup for card %d chan %d\n",card,channel);
    return ERROR;
  }

  status = intConnect(	INUM_TO_IVEC(vector), 
			mz8310_int_service, 
			&mzconf[card].icf[channel]);
  if(status != OK)
    return ERROR;

  sysIntEnable(mz8310_strap[channel].irq);

  return OK;

}


mz8310_one_shot_read(	preset,
			edge0_delay,
			edge1_delay, 
			card, 
			channel,
			int_source)
int			*preset;	/* TRUE or COMPLEMENT logic */
double			*edge0_delay;	/* sec */
double			*edge1_delay;	/* sec */
int			card;		/* 0 through ... */
int			channel;	/* 0 through channels on a card */
int			*int_source; 	/* (FALSE)External/(TRUE)Internal src */
{	
  int 			chip = channel/MZ8310CHANONCHIP;
  double 		ticks;
  unsigned short	iedge0;
  unsigned short	iedge1;
  unsigned int 		status;

  if(channel >= MZ8310CHANONCHIP * MZ8310CHIPCOUNT)
    return ERROR;

  if(card>=mz8310_card_count)
    return ERROR;

  if(!mzconf)
    return ERROR;

  FASTLOCK(&mzconf[card].lock);

  status =
    stc_one_shot_read(
			preset, 
			&iedge0, 
			&iedge1, 
			MZ8310_CMD_ADDR(card,chip), 
			MZ8310_DATA_ADDR(card,chip), 
			channel % MZ8310CHANONCHIP,
			int_source);
  if(status==OK){
    ticks = *int_source ? INT_TICKS : EXT_TICKS;
    *edge0_delay = iedge0 / ticks;
    *edge1_delay = iedge1 / ticks;
  }

  FASTUNLOCK(&mzconf[card].lock);

  return status;
}

mz8310_read_test(card,channel)
int card;
int channel;
{
  int 		preset;
  double 	edge0_delay;
  double 	edge1_delay;
  int 		int_source;
  int 		status;
  static char	*pclktype[] = {"external-clk", "internal-clk"};
  static char	*ppresettype[] = {"preset-FALSE", "preset-TRUE "};

  status = 
    mz8310_one_shot_read(	
			&preset,
			&edge0_delay,
			&edge1_delay, 
			card, 
			channel,
			&int_source);
  if(status == OK){
    printf(	"\tChannel %d %s delay=%lf width=%lf %s\n",
		channel,
		ppresettype[preset&1],
		edge0_delay,
		edge1_delay, 
		pclktype[int_source&1]);
    if(mzconf[card].icf[channel].cnt)
	printf("\tChannel %d Interrupt count=%u\n",
		channel,
		mzconf[card].icf[channel].cnt);
  }
		
  return status;
}


mz8310_one_shot(	preset,
			edge0_delay,
			edge1_delay, 
			card, 
			channel,
			int_source,
			event_rtn,
			event_rtn_param)
int	preset;		/* TRUE or COMPLEMENT logic */
double	edge0_delay;	/* sec */
double	edge1_delay;	/* set */
int	card;		/* 0 through ... */
int	channel;	/* 0 through channels on a card */
int	int_source; 	/* (FALSE)External/ (TRUE)Internal source */
void	*event_rtn;	/* subroutine to run on events */
int	event_rtn_param;/* parameter to pass to above routine */
{
  int chip = channel/MZ8310CHANONCHIP;
  double ticks = int_source?INT_TICKS:EXT_TICKS;
  void	*old_handler();
  int status;

  if(channel >= MZ8310CHANONCHIP * MZ8310CHIPCOUNT)
    return ERROR;

  if(card>=mz8310_card_count)
    return ERROR;

  if(!mzconf)
    return ERROR;

  /* dont overflow unsigned short in STC */
  if(edge0_delay >= 0xffff/ticks)
    return ERROR;
  if(edge1_delay >= 0xffff/ticks)
    return ERROR;
  if(edge0_delay < 0.0)
    return ERROR;
  if(edge1_delay < 0.0)
    return ERROR;

  FASTLOCK(&mzconf[card].lock);

  /* Enable calling of user routine */
  if(MZ8310_INTERUPTABLE(channel)){
    mzconf[card].icf[channel].user_service = event_rtn;
    mzconf[card].icf[channel].user_param = event_rtn_param;
  }

  status =
    stc_one_shot(
			preset, 
			(unsigned short) (edge0_delay * ticks), 
			(unsigned short) (edge1_delay * ticks), 
			MZ8310_CMD_ADDR(card,chip), 
			MZ8310_DATA_ADDR(card,chip), 
			channel % MZ8310CHANONCHIP,
			int_source);

  FASTUNLOCK(&mzconf[card].lock);

  return status;

}


void mz8310_int_service(icf)
register struct mz8310_int_conf *icf;
{
  icf->cnt++;

  if(icf->user_service)
    (*icf->user_service)(icf->user_param);

/*  logMsg("MZINT: level %d cnt %d\n",icf->irq,icf->cnt); */

  return;
}

/*
	The following are provided for mz8310 access from the shell
*/

unsigned char	mz8310_cmd(value, card, chip)
unsigned char	value;
int		card;
int		chip;
{
  register unsigned char *cmd = MZ8310_CMD_ADDR(card,chip);
  
  *cmd = value;

  return *cmd;
}

unsigned short	mz8310_rdata(card, chip)
int		card;
int		chip;
{
  register unsigned short *data = MZ8310_DATA_ADDR(card,chip);

  return *data;
}

unsigned short	mz8310_wdata(value, card, chip)
unsigned short	value;
int		card;
int		chip;
{
  register unsigned short *data = MZ8310_DATA_ADDR(card,chip);

  *data = value;

  return value;

}


void mz8310_reset()
{
    short card,channel,chip;
      
	for (card = 0;card < mz8310_card_count; card++){
		FASTLOCK(&mzconf[card].lock); 
		for ( channel = 0; channel < tm_num_channels[MZ8310]; channel++){
                   if (mzconf[card].icf[channel].cnt){
                        chip = channel/MZ8310CHANONCHIP;                      
                       
			stc_one_shot(
					0,
					10,
					0,
                                        MZ8310_CMD_ADDR(card,chip),
                                        MZ8310_DATA_ADDR(card,chip),
                                        channel % MZ8310CHANONCHIP,
                                        0);	                        
		   } 
		}
		FASTUNLOCK(&mzconf[card].lock); 
	}
       
}
 
    	
       
