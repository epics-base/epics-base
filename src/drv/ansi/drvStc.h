/* drvStc.h */
/* base/src/drv $Id$ */
/*
 * The following are specific driver routines for the AMD STC
 *
 * NOTE: if multiple threads use these routines at once you must provide locking
 * so command/data sequences are gauranteed. See mz8310_driver.c for examples.
 *
 *
 *      Author: Jeff Hill
 *      Date:   Feb 89
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
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
 * Modification Log:
 * -----------------
 *
 * joh  022089  Init Release
 * joh  082493  ANSI C and EPICS return codes
 */

/*
 * AMD STC constants
 */
#define CHANONCHIP              5U
#define CHIPCHAN                (channel%CHANONCHIP)
#define CHIPNUM                 (channel/CHANONCHIP)

#define STC_RESET               stcWriteCmd(pcmd,0xffU);
#define STC_BUS16               stcWriteCmd(pcmd,0xefU);
#define STC_BUS16               stcWriteCmd(pcmd,0xefU);
#define STC_SET_MASTER_MODE(D)  {stcWriteCmd(pcmd,0x17U); \
				stcWriteData(pdata,(D));} 
#define STC_MASTER_MODE         (stcWriteCmd(pcmd,0x17U), stcReadData(pdata))

#define STC_CTR_INIT(MODE,LOAD,HOLD)\
{stcWriteCmd(pcmd,CHIPCHAN+1); stcWriteData(pdata,(MODE)); \
stcWriteData(pdata,(LOAD)); stcWriteData(pdata,(HOLD));}

#define STC_CTR_READ(MODE,LOAD,HOLD)\
{stcWriteCmd(pcmd,CHIPCHAN+1); (MODE) = stcReadData(pdata); \
(LOAD) = stcReadData(pdata); (HOLD) = stcReadData(pdata);}

#define STC_SET_TC(D)           stcWriteCmd(pcmd, \
				0xe0U | ((D)?8:0)|(CHIPCHAN+1U) )

#define STC_LOAD                stcWriteCmd(pcmd, 0x40U | 1<<(CHIPCHAN))
#define STC_STEP                stcWriteCmd(pcmd, 0xf0U | (CHIPCHAN+1U))
#define STC_ARM                 stcWriteCmd(pcmd, 0x20U | 1<<CHIPCHAN)
#define STC_DISARM              stcWriteCmd(pcmd, 0xc0U | 1<<CHIPCHAN)




/*
 * return type of the stc routines
 */
typedef long    stcStat;
#define STC_SUCCESS	0


stcStat stc_io_report(
volatile uint8_t	*pcmd,
volatile uint16_t	*pdata
);


stcStat stc_init(
volatile uint8_t	*pcmd,
volatile uint16_t	*pdata,
unsigned 		master_mode
);

stcStat stc_one_shot_read(
unsigned 		*preset,
uint16_t		*edge0_count,
uint16_t		*edge1_count,
volatile uint8_t	*pcmd,
volatile uint16_t	*pdata,
unsigned 		channel,
unsigned 		*int_source 
);

stcStat stc_one_shot(
unsigned 		preset,
unsigned 		edge0_count,
unsigned 		edge1_count,
volatile uint8_t	*pcmd,
volatile uint16_t	*pdata,
unsigned 		channel,
unsigned 		int_source 
);

void stcWriteData(volatile uint16_t *pdata, uint16_t data);
uint16_t stcReadData(volatile uint16_t *pdata);
void stcWriteCmd(volatile uint8_t *pcmd, uint8_t cmd);

