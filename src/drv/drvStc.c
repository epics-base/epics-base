
/* drvStc.c */
/* share/src/drv $Id$ */
/*
 * The following are specific driver routines for the AMD STC
 *
 * NOTE: if multiple threads use these routines at once you must provide locking
 * so command/data sequences are gauranteed. See mz8310_driver.c for examples.
 *
 *
 *	Author: Jeff Hill
 *	Date:	Feb 89
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
 *
 * joh	022089	Init Release
 * joh	042889	Added read back
 * joh	111789	Fixed reset goes to byte mode bug
 * joh	121090	Fixed confusion about the polarity of internal/external
 *			clock between DB and the drivers.
 * joh	110791	Prevent the stc from generating tc prior to the trigger 
 *		in delayed pulse mode by forcing edge 0 delays of zero to be 
 *		a delay of one instead.
 * joh  010491  force all edge 0 delays less than two to two
 *  bg  062692  force all edge 0 delays less than two to two
 */

/* drvStc.c -  Driver Support Routines for Stc */
#include	<vxWorks.h>
#include	<stdioLib.h>

#include	<dbDefs.h>
#include	<drvSup.h>
#include	<module_types.h>


/* If any of the following does not exist replace it with #define <> NULL */
static long report();
static long init();

struct {
	long	number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvStc={
	2,
	report,
	init};

static long report(fp)
    FILE	*fp;
{

    return(0);
}

static long init()
{
    int status;

    return(0);
}

#define CHANONCHIP		5
#define CHIPCHAN		(channel%CHANONCHIP)
#define CHIPNUM			(channel/CHANONCHIP)

#define STC_RESET		*pcmd = 0xff
#define STC_BUS16		*pcmd = 0xef
#define STC_SET_MASTER_MODE(D)	{*pcmd = 0x17; *pdata=(D);}
#define STC_MASTER_MODE		(*pcmd = 0x17, *pdata)

#define STC_CTR_INIT(MODE,LOAD,HOLD)\
{*pcmd = CHIPCHAN+1; *pdata = (MODE); *pdata = (LOAD); *pdata= (HOLD);}

#define STC_CTR_READ(MODE,LOAD,HOLD)\
{*pcmd = CHIPCHAN+1; (MODE) = *pdata; (LOAD) = *pdata; (HOLD) = *pdata;}

#define STC_SET_TC(D)		*pcmd = 0xe0 | ((D)?8:0)|(CHIPCHAN+1)

#define STC_LOAD		*pcmd = 0x40 | 1<<(CHIPCHAN)
#define STC_STEP		*pcmd = 0xf0 | (CHIPCHAN+1)
#define STC_ARM			*pcmd = 0x20 | 1<<CHIPCHAN
#define STC_DISARM		*pcmd = 0xc0 | 1<<CHIPCHAN

stc_io_report(pcmd,pdata)
register unsigned char 	*pcmd;
register unsigned short *pdata;
{
  int	cmd, data;

  if(vxMemProbe(pcmd, READ, sizeof(*pcmd), &cmd) != OK)
    return ERROR;
  if(vxMemProbe(pdata, READ, sizeof(*pdata), &data) != OK)
    return ERROR;

  return OK;
}


stc_init(pcmd, pdata, master_mode)
register unsigned char 	*pcmd;
register unsigned short *pdata;
unsigned short		master_mode;
{
  int	dummy;
  int 	channel;

  if(vxMemProbe(pcmd, READ, sizeof(*pcmd), &dummy) != OK)
    return ERROR;
  if(vxMemProbe(pdata, READ, sizeof(*pdata), &dummy) != OK)
    return ERROR;
/*  
    go to 16 bit mode in order to test the master mode register
*/
  STC_BUS16;
  if(master_mode != STC_MASTER_MODE){
/*  
    start in a known state 
*/
    STC_RESET;
/*  
    required since the reset puts it in byte mode 
*/
    STC_BUS16; 
    STC_SET_MASTER_MODE(master_mode);
    for(channel=0; channel<CHANONCHIP; channel++)
      STC_LOAD;
  }

  return OK;
}


stc_one_shot_read(preset, edge0_count, edge1_count, pcmd, pdata, channel,int_source)
unsigned int	*preset;
unsigned short	*edge0_count;
unsigned short	*edge1_count;
unsigned char	*pcmd;
unsigned short	*pdata;
unsigned int 	channel;
unsigned int	*int_source;
{
  int			dummy;
  unsigned short	mode;
  unsigned short	edge0;
  unsigned short	edge1;

  if(vxMemProbe(pcmd, READ, sizeof(*pcmd), &dummy) != OK)
    return ERROR;
  if(vxMemProbe(pdata, READ, sizeof(*pdata), &dummy) != OK)
    return ERROR;

  if(channel>=CHANONCHIP)
    return ERROR;

  STC_CTR_READ(mode, edge0, edge1);
 
  /*
  Only return values if the counter is in the proper mode
  see stc_one_shot() for info on conversions and functions selected
  by these bit fields
  */
  if(mode == 0xc16a){
    *int_source = 	FALSE;
    *preset = 		TRUE;
    *edge0_count = 	~edge0; 
    *edge1_count = 	~edge1+1;
  }
  else if(mode == 0xc162){
    *int_source = 	FALSE;
    *preset = 		FALSE;
    *edge0_count = 	edge0-1; 
    *edge1_count = 	edge1;
  }
  else if(mode == 0xcb6a){
    *int_source = 	TRUE;
    *preset = 		TRUE;
    *edge0_count = 	~edge0; 
    *edge1_count = 	~edge1+1;
  }
  else if(mode == 0xcb62){
    *int_source = 	TRUE;
    *preset = 		FALSE;
    *edge0_count = 	edge0-1; 
    *edge1_count = 	edge1;
  }
  else
    return ERROR;

  return OK;
}


stc_one_shot(preset, edge0_count, edge1_count, pcmd, pdata, channel,int_source)
unsigned int	preset;
unsigned short	edge0_count;
unsigned short	edge1_count;
unsigned char	*pcmd;
unsigned short	*pdata;
unsigned int 	channel;
int		int_source;
{
  int		dummy;

  if(vxMemProbe(pcmd, READ, sizeof(*pcmd), &dummy) != OK)
    return ERROR;
  if(vxMemProbe(pdata, READ, sizeof(*pdata), &dummy) != OK)
    return ERROR;
  if(channel>=CHANONCHIP)
    return ERROR;

  /*
   * joh	110791	
   *		Prevent the stc from generating tc prior to the trigger 
   *		in delayed pulse mode by forcing edge 0 delays of zero to be 
   *		a delay of one instead.
   *
   *		010492
   *		Strange extra edges occur when the delay is 0 or 1
   *		and the counter is reinitialized to a width of
   *		zero so I have disabled a delay of one also
   *
   *		These extra edges occur when TC is set
   */

  if(edge0_count < 2)
    edge0_count = 2;

  STC_DISARM;

  /*
  active positive going edge (gate input)
  count on the rising edge of source
  ctr source: (F1- internal) (SRC1- external)
  mode L - Hardware triggered delayed pulse one-shot
  binary count
  count down (count up if preset is TRUE)
  TC toggled output

  see chapter 7 of the Am9513 STC tech man concerning count + 1

  */

  /*
  NOTE: I must be able to read back the state of the preset later
  so I encode this information in the count down/up bit.
  count up on TRUE preset 
  count down on FALSE preset

  see stc_one_shot_read() above
  */
  if(int_source){
    if(preset)
      STC_CTR_INIT(0xcb6a, ~edge0_count, ~edge1_count+1)
    else
      STC_CTR_INIT(0xcb62, edge0_count+1, edge1_count);
  }else{
    if(preset)
      STC_CTR_INIT(0xc16a, ~edge0_count, ~edge1_count+1)
    else
      STC_CTR_INIT(0xc162, edge0_count+1, edge1_count);
  }

  STC_LOAD;
  /* 
  see chapter 7 of the Am9513 STC tech man concerning this step
  */

  STC_STEP;

  STC_SET_TC(preset);

  /*
  Only arm counter if the pulse has a finite duration
  */
  if(edge1_count != 0){
    STC_ARM;
  }

  return OK;
}
   


