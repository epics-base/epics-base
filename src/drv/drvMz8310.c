
/* drvMz8310.c */
/* share/src/drv$Id$*/
/* 
 * Routines specific to the MZ8310. Low level routines for the AMD STC in
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
 * mgb  08-04-93	Removed V5/V4 and EPICS_V2 conditionals
 * joh	08-24-93	Include drvStc.h and ANSI C upgrade
 */

/* drvMz8310.c -  Driver Support Routines for Mz8310 */
#include	<vxWorks.h>
#include	<stdioLib.h>
#include	<sysLib.h>
#include	<stdlib.h>
#include	<intLib.h>
#include	<rebootLib.h>
#include	<vxLib.h>
#include 	<vme.h>
#include	<iv.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>
#include 	<fast_lock.h>
#include 	<devLib.h>
#include 	<drvStc.h>

typedef long mz8310Stat;
#define	MZ8310_SUCCESS	0

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
((volatile unsigned char *)  CHIP_ADDR(CARD,CHIP) + MZ8310CMD)
#define MZ8310_DATA_ADDR(CARD,CHIP)\
((volatile unsigned short *) CHIP_ADDR(CARD,CHIP) + MZ8310DATA)
#if  0
#define MZ8310VECBASE(CARD,CHIP)\
((volatile unsigned char *)  CHIP_ADDR(CARD,CHIP) + 0x41)
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
  unsigned char		irq;	 /* the level at which the chan gen ints */
  unsigned char		vec_num; /* really a vec offset-see MZ8310INTVEC */
  unsigned char		vec_addr;/* offset from card base address */
};

static volatile char 		*shortaddr;

LOCAL struct mz8310_conf 	*mzconf;
LOCAL unsigned int 		mz8310_card_count;

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

LOCAL struct mz8310_strap_info mz8310_strap[MZ8310CHANCNT] =
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
LOCAL int		mz8310_reset(void);
LOCAL mz8310Stat	mz8310_io_report_card(unsigned card, int level);
LOCAL mz8310Stat	mz8310_init_card(unsigned card);
LOCAL mz8310Stat 	mz8310_setup_int(unsigned card, unsigned channel);
LOCAL mz8310Stat	mz8310_io_report(int level);
LOCAL mz8310Stat	mz8310_init(void);
LOCAL mz8310Stat 	mz8310_read_test(int card, int channel);
LOCAL void 		mz8310_int_service(struct mz8310_int_conf *icf);

/* externals */
mz8310Stat mz8310_one_shot_read(
int			*preset,	/* TRUE or COMPLEMENT logic */
double			*edge0_delay,	/* sec */
double			*edge1_delay,	/* sec */
int			card,		/* 0 through ... */
int			channel,	/* 0 through channels on a card */
int			*int_source 	/* (FALSE)External/(TRUE)Internal src */
);

mz8310Stat mz8310_one_shot(	
int	preset,		/* TRUE or COMPLEMENT logic */
double	edge0_delay,	/* sec */
double	edge1_delay,	/* set */
int	card,		/* 0 through ... */
int	channel,	/* 0 through channels on a card */
int	int_source, 	/* (FALSE)External/ (TRUE)Internal source */
void	*event_rtn,	/* subroutine to run on events */
int	event_rtn_param /* parameter to pass to above routine */
);

int	mz8310_cmd(
unsigned 	value,
unsigned 	card,
unsigned 	chip
);

int mz8310_rdata(int card, int chip);

int mz8310_wdata(
unsigned 	value,
int		card,
int		chip
);

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvMz8310={
	2,
	mz8310_io_report,
	mz8310_init};


/*
 * mz8310_io_report()
 */
LOCAL mz8310Stat mz8310_io_report(int level)
{
	unsigned	card;

    	for(card=0; card<tm_num_cards[MZ8310]; card++) 
		mz8310_io_report_card(card,level);

	return MZ8310_SUCCESS;
}




/*
 * mz8310_io_report_card()
 */
LOCAL mz8310Stat mz8310_io_report_card(unsigned card, int level)
{
  unsigned 	channel;
  unsigned	chip;
  mz8310Stat	status;

  for(chip=0; chip<MZ8310CHIPCOUNT; chip++){
      status = stc_io_report(
			MZ8310_CMD_ADDR(card,chip), 
			MZ8310_DATA_ADDR(card,chip));
      if(status){
        return status;
      }
  }

  printf("TM: MZ8310:\tcard %d\n", card);


  if (mzconf && card<mz8310_card_count && level){
      for(channel=0;channel<MZ8310CHANCNT;channel++){
         status =  mz8310_read_test(card, channel);
         if(status){
         	return status;
         }

      }
   }
  return MZ8310_SUCCESS;
}



/*
 * mz8310_init()
 */
LOCAL mz8310Stat	mz8310_init(void)
{
  unsigned 			card;
  mz8310Stat			status;
  struct mz8310_conf 		*temp_mzconf;
  unsigned 			card_count = tm_num_cards[MZ8310];

  status = sysBusToLocalAdrs(
		VME_AM_SUP_SHORT_IO,
		0,
		(char **)&shortaddr);
  if (status != OK){ 
	status = S_dev_badA16;
	errMessage(status, "A16 Address map error mz8310 driver");
        return status;
  } 
  
  temp_mzconf = (struct mz8310_conf *) malloc(sizeof(*mzconf)*card_count);
  if(!temp_mzconf)
    return S_dev_noMemory;

  for(card=0; card<card_count; card++){
    FASTLOCKINIT(&temp_mzconf[card].lock);
  }

  if(mzconf){
    for(card=0; card<card_count; card++){
      if(FASTLOCKFREE(&mzconf[card].lock)<0){
	status = S_dev_internal;
        errMessage(status, "error freeing sem");
      }
    }
    free(mzconf);
  }

  mzconf = temp_mzconf;

  for(card=0; card<card_count; card++){
    status = mz8310_init_card(card);
    if(status){
      break;
    }
  }

  mz8310_card_count = card;
  
  rebootHookAdd(mz8310_reset); 

  return MZ8310_SUCCESS;
}



/*
 *
 * mz8310_init_card()
 *
 * Locking for this provided by mz8310_init()
 */
LOCAL mz8310Stat mz8310_init_card(unsigned card)
{
  unsigned	chip;
  unsigned	chan;
  mz8310Stat	error;
  /*
   * binary division
   * data ptr seq enbl
   * 16 bit bus
   * FOUT on 
   * FOUT divide by one
   * FOUT source (F1)
   * Time of day disabled
   */
  unsigned short master_mode = 0x2100;

  for(chip=0; chip< MZ8310CHIPCOUNT; chip++){
    error = stc_init(
		MZ8310_CMD_ADDR(card,chip), 
		MZ8310_DATA_ADDR(card,chip), 
		master_mode);
    if(error){
      errMessage(error, NULL);
      return error;
    }
  }

  for(chan=0; chan<MZ8310CHANCNT; chan++)
    mz8310_setup_int(card,chan);

  mzconf[card].init = TRUE;

  return MZ8310_SUCCESS;
}



/*
 * mz8310_setup_int()
 *
 * (locked by calling routines)
 */
LOCAL mz8310Stat mz8310_setup_int(unsigned card, unsigned channel)
{
  unsigned char	vector;
  mz8310Stat	status;

  mzconf[card].icf[channel].user_service = NULL;
  mzconf[card].icf[channel].user_param = NULL;
  mzconf[card].icf[channel].cnt = 0;

  /*
   * Is this channel strapped for interrupts
   */
  if(!MZ8310_INTERUPTABLE(channel))
    return MZ8310_SUCCESS;

  vector = MZ8310INTVEC(card,channel);
  status = vxMemProbe(        
		(char *)MZ8310BASE(card) + mz8310_strap[channel].vec_addr,
		WRITE,
		sizeof(vector),
		&vector);
  if(status != OK){
    	status = S_dev_noDevice;
    	errMessage(status, NULL);
    	return S_dev_noDevice;
  }

  status = intConnect(	INUM_TO_IVEC(vector), 
			mz8310_int_service, 
			(int)&mzconf[card].icf[channel]);
  if(status != OK)
    return S_dev_vxWorksVecInstlFail;

  sysIntEnable(mz8310_strap[channel].irq);

  return MZ8310_SUCCESS;
}



/*
 * mz8310_one_shot_read()
 */
mz8310Stat mz8310_one_shot_read(
int			*preset,	/* TRUE or COMPLEMENT logic */
double			*edge0_delay,	/* sec */
double			*edge1_delay,	/* sec */
int			card,		/* 0 through ... */
int			channel,	/* 0 through channels on a card */
int			*int_source 	/* (FALSE)External/(TRUE)Internal src */
)
{	
  int 			chip = channel/MZ8310CHANONCHIP;
  double 		ticks;
  unsigned short	iedge0;
  unsigned short	iedge1;
  mz8310Stat		status;

  if(channel >= MZ8310CHANONCHIP * MZ8310CHIPCOUNT)
    return S_dev_badSignalNumber;

  if(card>=mz8310_card_count)
    return S_dev_badA16;

  if(!mzconf)
    return S_dev_noDevice;

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
  if(status==STC_SUCCESS){
    ticks = *int_source ? INT_TICKS : EXT_TICKS;
    *edge0_delay = iedge0 / ticks;
    *edge1_delay = iedge1 / ticks;
  }

  FASTUNLOCK(&mzconf[card].lock);

  return status;
}


/*
 * mz8310_read_test()
 */
LOCAL mz8310Stat mz8310_read_test(int card, int channel)
{
  int 		preset;
  double 	edge0_delay;
  double 	edge1_delay;
  int 		int_source;
  mz8310Stat	status;
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
  if(status==MZ8310_SUCCESS){
    printf(	"\tChannel %d %s delay=%f width=%f %s\n",
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


/*
 * mz8310_one_shot()
 */
mz8310Stat mz8310_one_shot(	
int	preset,		/* TRUE or COMPLEMENT logic */
double	edge0_delay,	/* sec */
double	edge1_delay,	/* set */
int	card,		/* 0 through ... */
int	channel,	/* 0 through channels on a card */
int	int_source, 	/* (FALSE)External/ (TRUE)Internal source */
void	*event_rtn,	/* subroutine to run on events */
int	event_rtn_param /* parameter to pass to above routine */
)
{
  int 		chip = channel/MZ8310CHANONCHIP;
  double 	ticks = int_source?INT_TICKS:EXT_TICKS;
  mz8310Stat	status;

  if(channel >= MZ8310CHANONCHIP * MZ8310CHIPCOUNT)
    return S_dev_badSignalNumber;

  if(card>=mz8310_card_count)
    return S_dev_badA16;

  if(!mzconf)
    return S_dev_noDevice;

  /* dont overflow unsigned short in STC */
  if(edge0_delay >= 0xffff/ticks)
    return S_dev_highValue;
  if(edge1_delay >= 0xffff/ticks)
    return S_dev_highValue;
  if(edge0_delay < 0.0)
    return S_dev_lowValue;
  if(edge1_delay < 0.0)
    return S_dev_lowValue;

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



/*
 * mz8310_int_service()
 */
LOCAL void mz8310_int_service(struct mz8310_int_conf *icf)
{
  icf->cnt++;

  if(icf->user_service)
    (*icf->user_service)(icf->user_param);

  return;
}

/*
 *	The following are provided for mz8310 access from the shell
 */


/*
 * mz8310_cmd()
 */
int	mz8310_cmd(
unsigned 	value,
unsigned 	card,
unsigned 	chip
)
{
  volatile unsigned char *cmd = MZ8310_CMD_ADDR(card,chip);
  
  *cmd = value;

  return *cmd;
}


/*
 * mz8310_rdata()
 */
int mz8310_rdata(int card, int chip)
{
  volatile unsigned short *data = MZ8310_DATA_ADDR(card,chip);

  return *data;
}


/*
 * mz8310_wdata()
 */
int mz8310_wdata(
unsigned 	value,
int		card,
int		chip
)
{
  volatile unsigned short *data = MZ8310_DATA_ADDR(card,chip);

  *data = value;

  return value;

}



/*
 * mz8310_reset
 */
LOCAL int mz8310_reset(void)
{
    	short card,channel,chip;
      
	for (card = 0; card < mz8310_card_count; card++){
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
       
	return OK;
}
 
    	
       
