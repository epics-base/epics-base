/* drvVmi2534.c */
/* share/src/drv @(#)drvVmic2534.c	1.1     003/22/94 */

#ifndef lint
static char rcsid[] =
    "@(#) /babar/CVSROOT/epics/base/src/drv/old/drvVmi2534.c,v 1.1 1996/01/09 19:54:35 lewis Exp(LBL)";
#endif

/*
 *	routines used to test and interface with V.M.I.C. VMIVME-2534
 *	digital i/o module
 *
 * 	Author:      Bill Brown
 * 	Date:        03/22/94
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1994, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(DE-AC03-76SF00) at Lawrence Berkeley Laboratory.
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		Control System Group
 *		Advanced Light Source
 *		Lawrence Berkeley Laboratory
 *
 *	Co-developed with
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 *	NOTES:
 *	    This code is/was cloned from "drvXy240.c" of EPICS R3.11
 *
 *	    Data direction is dynamically assigned at ioInit() time.
 *	    The definition of the first bit(s) in a given byte will
 *	    determine the data direction of all bits within that byte.
 *
 * Modification Log:
 *
 *
    @(#) drvVmi2534.c,v
    @(#) Revision 1.1  1996/01/09 19:54:35  lewis
    @(#) Add PAL record.  Remove APS eg/er records.
    @(#) Add 4 STAR drivers, LBL 2534, and preliminaries for LBL/SLAC 9210/9325 drivers.
    @(#)
 * Revision 1.1.1.1  1995/07/25  21:49:57  lewis
 * Baseline import R3.12 w/LBL drivers and mods.
 *
 * Revision 1.5  1994/09/28  09:05:51  wbrown
 * at beginning of init function, ctrl reg is set to match
 *   SYS_RESET* condition.
 *
 * Revision 1.4  94/09/27  09:44:31  wbrown
 * changed order of "writes to '2534 hardware during initialization
 * to prevent undesired output transistions when calculating
 * "output correction factor."
 * 
 * Revision 1.3  94/08/19  15:16:14  wbrown
 * added debug #ifdef, printf statements
 * fixed initialization of record definition structure
 * 
 * Revision 1.2  94/07/20  09:25:26  wbrown
 * modified to dynamically assign data direction at
 *   record init time.
 * 
 * Revision 1.1  94/05/27  09:57:51  wbrown
 * Initial revision
 * 
 * 
 */

#include <vxWorks.h>
#include <taskLib.h>
#include <types.h>
#include <vme.h>

#include  "dbDefs.h"
#include  "errlog.h"
#include  "errlog.h"
#include "module_types.h"
#include "task_params.h"
#include "drvSup.h"
#include "dbDefs.h"
#include "dbScan.h"
#include "taskwd.h"

#define VMIC_2534_ADDR0     (bi_addrs[VMIC_2534_BI])
#define VMIC_2534_MAX_CARDS (bi_num_cards[VMIC_2534_BI])
#define VMIC_2534_MAX_CHANS (bi_num_channels[VMIC_2534_BI])

/*  control-status register
	bit definiitions						 */

#define BYTE_0_OUT_ENA	1<<0
#define BYTE_1_OUT_ENA	1<<1
#define BYTE_2_OUT_ENA	1<<2
#define BYTE_3_OUT_ENA	1<<3

#define FAIL_LED_OFF	1<<5
#define TM1_ENABLE	1<<7
#define TM2_ENABLE	1<<6

/*  Refer to VMIVME-2534 manual for hardware options			 */
/*	These boards are available in two configurations;		 */
/*	"POSITIVE-TRUE Option" - writing a "1" to the output register	 */
/*	    "TURNS ON the output driver, ie pulls the output LOW	 */
/*	"NEGATIVE-TRUE Option" - writing a "1" to the output register	 */
/*	    "TURNS OFF" the output driver, allowing the load to pull	 */
/*	    the output HIGH.						 */

/*	The initialization routine recognizes which type of board is	 */
/*	installed and generates a mask which is used by the device-	 */
/*	driver to cause BOTH board types to appear to have the		 */
/*	POSITIVE-TRUE Option installed.					 */

#define FAIL_FLAG	1<<5
				/*  Test Mode				 */
#define TM2		1<<6	/*  TM2 - "1" = disable test buffers	 */
#define TM1		1<<7	/*  TM1 - "1" = enable output drivers	 */

#define masks( K ) ((1<<K))


/*  VMIVME-2534 memory structure					 */

/*	    Data direction is dynamically assigned at ioInit() time.	 */
/*	    The definition of the first bit(s) in a given byte will	 */
/*	    determine the data direction of all bits within that byte.	 */


typedef struct
    {
    u_long	pioData;
    u_char	brdIdReg;	
    u_char	ctlStatReg;
    u_char	endPad[2];
    } VMIC_2534 ;


/*  VMIVME-2534 control structure record				 */

typedef struct
    {
    VMIC_2534 *vmicDevice;	/*  pointer to the hardware		 */
    short	deviceNr;	/*  device number			 */
    u_long	savedInputs;	/*  previous input value		 */
    u_long	outputCorrector;	/*  polarity correction factor	 */
    u_long	inputMask;	/*  and-mask for input bytes		 */
    u_long	outputMask;	/*  and-mask for output bytes		 */
    IOSCANPVT	ioscanpvt;
    } VMIC_2534_REC ;


VMIC_2534_REC *vmic2534Rec;	/*  points to control structures	 */

static long report( );
static long init( );


struct 
    {
    long	number;
    DRVSUPFUN	report;
    DRVSUPFUN	init;
    } drvVmi2534 =
	{
	2,
	report,
	init
	} ;

static
long report( int level)
    {
    vmic2534_io_report( level );
    return( 0 );
    }


void vmic2534BiReport( int card );
void vmic2534BoReport( int card );

static long init( )
    {
    vmic2534Init( );
    return( 0 );
    }

/*
 *
 *  This hardware does not support interrupts
 *
 *  NO interrupt service routine is applicable
 */


/*
 *
 *  vmic2534IOScan
 *
 *  task to check of a change of state
 *	(used to simulate "change-of-state" interrupts)
 *
 */

vmic2534IOScan( )
    {
    int	i;
    int	firstScan = TRUE;
    long temp;

    for ( ; ; )
	{
	if ( interruptAccept ) break;
	taskDelay( vxTicksPerSecond/30 );
	}

    for ( ; ; )
	{
	for ( i = 0; i < VMIC_2534_MAX_CARDS; i++ )
	    {
	    if ( vmic2534Rec[ i ].vmicDevice )
		{
		temp = ( vmic2534Rec[ i ].vmicDevice->pioData )
			& ( vmic2534Rec[ i ].inputMask );

		if ( (temp ^ vmic2534Rec[ i ].savedInputs) || firstScan )
		    {
		    scanIoRequest( vmic2534Rec[ i ].ioscanpvt );
		    vmic2534Rec[ i ].savedInputs = temp;
		    }
		}
	    }

	if ( firstScan )
	    firstScan = FALSE;

	taskDelay( vxTicksPerSecond/30 );
	}
    }


/*
 * DIO DRIVER INIT
 *
 */

vmic2534Init( )
    {
    u_long		dummy;
    register short	i;
    VMIC_2534		*theHardware;
    int			tid;
    int			status;
    int			atLeastOnePresent = FALSE;

    /*
     *  allow for runtime reconfiguration of the addr map
     */

    vmic2534Rec = (VMIC_2534_REC *)calloc(
	    VMIC_2534_MAX_CARDS, sizeof( VMIC_2534_REC ) );
    if ( !vmic2534Rec )
	{
	logMsg("Attempt to calloc \"VMIC_2534_REC\" failed\n");
	return( ERROR );
	}
    status = sysBusToLocalAdrs( VME_AM_SUP_SHORT_IO, (VMIC_2534_ADDR0),
	    &theHardware );
    if ( status != OK )
	{
	logMsg("%s: Unable to map VMIC_2534 base addr\n", __FILE__);
	return( ERROR );
	}
    
    for ( i = 0; i < VMIC_2534_MAX_CARDS; i++, theHardware++ )
	{
	if ( (vxMemProbe( theHardware, READ, 2 ,&dummy )) != OK )
	    {
	    vmic2534Rec[i].vmicDevice = NULL;
	    continue;
	    }
	else
	    /*  initialize hardware					 */
	    {
	    /*  Outputs/Inputs are undetermined at this time		 */
	    /*  Disable output drivers in case of no SYS_RESET*		 */
	    theHardware->ctlStatReg = 0;

	    /*  determine "sense" of output and save correction factor	 */
	    theHardware->pioData = 0;
	    vmic2534Rec[i].outputCorrector = theHardware->pioData;

	    /*  clear input & output masks, (malloc'ed mem may != 0)	 */
	    vmic2534Rec[i].inputMask = 0;
	    vmic2534Rec[i].outputMask = 0;

	    /*  insure that outputs are initially "OFF", i.e. HIGH	 */
	    theHardware->pioData = vmic2534Rec[i].outputCorrector;
	    theHardware->ctlStatReg = TM1 | TM2 | FAIL_LED_OFF;
	
	    /*
	     *  record structure initalization 
	     */

	    vmic2534Rec[ i ].vmicDevice = theHardware;
	    atLeastOnePresent = TRUE;
	    scanIoInit( &(vmic2534Rec[ i ].ioscanpvt) );
	    }
	}

    if ( atLeastOnePresent)
	{
	if ( (tid = taskNameToId( VMIC_2534_NAME )) != ERROR )
	    {
	    taskwdRemove( tid );
	    taskDelete( tid );
	    }

	if ( (status = taskSpawn(
		VMIC_2534_NAME, VMIC_2534_PRI, VMIC_2534_OPT,
			VMIC_2534_STACK, vmic2534IOScan )) == ERROR )
	    logMsg("Unable to spawn \"vmic2534IOScan( )\" task\n");
	else
	    taskwdInsert( status, NULL, NULL );
	}
    return( OK );
    }

/*
 *  Set data direction control for specified data bits
 *  Note:
 *    Data direction is controlled on a "per-byte" basis.
 *    The first bit defined within a byte sets the direction for the entire
 *      byte.  Any attempt to define conflicting bits after the direction
 *      for a byte is set will return an error (-1) response.)
 *    Permmissable values for "direction" are:
 *      direction == 0 indicates input
 *      direction != 0 indicates output
 *
 */
vmic2534_setIOBitDirection( short card, u_int mask, u_int direction )
    {
    int		status = 0;
    u_long	work, j;

    static u_long maskTbl[ ] =
		    { 0x000000ff,
		      0x0000ff00,
		      0x00ff0000,
		      0xff000000
		    };

    if ( (card >= VMIC_2534_MAX_CARDS) || !(vmic2534Rec[ card ].vmicDevice) )
	status = -1 ;
    else
	{
	if ( direction == 0)
	    {
	    /*  INPUT	 */
#ifdef DEBUG_2534
		    printf("Mask = %08x, outPutMask =%08x\n",
			    mask, vmic2534Rec[ card ].outputMask);
#endif DEBUG_2534
	    if ( ((u_long)mask & vmic2534Rec[ card ].outputMask) == 0 )
		{
		for ( j = 0; j < 4; j++ )
		    {

		    if ( ( (u_long)mask & maskTbl[ j ] ) != 0 )
			{
			vmic2534Rec[ card ].inputMask |= maskTbl[ j ];
#ifdef DEBUG_2534
			printf("inputMask = %08x\n",
				vmic2534Rec[ card ].inputMask);
#endif DEBUG_2534
			vmic2534Rec[ card ].vmicDevice->ctlStatReg
				&= (u_char) ~(1 << j);
			}
		    }
		}
	    else
		{
		status = -1;
		}
	    }
	else
	    {
	    /*  OUTPUT	 */
#ifdef DEBUG_2534
		    printf("Mask = %08x, inputMask =%08x\n",
			    mask, vmic2534Rec[ card ].inputMask);
#endif DEBUG_2534
	    if ( ((u_long)mask & vmic2534Rec[ card ].inputMask) == 0 )
		{
		for ( j = 0; j < 4; j++ )
		    {
		    if ( ( (u_long)mask & maskTbl[ j ] ) != 0 )
			{
			vmic2534Rec[ card ].outputMask |= maskTbl[ j ];
#ifdef DEBUG_2534
			printf("outputMask = %08x\n",
				vmic2534Rec[ card ].outputMask);
#endif DEBUG_2534
			vmic2534Rec[ card ].vmicDevice->ctlStatReg
				|= (u_char)(1 << j);
			}
		    }
		}
	    else
		{
		status = -1;
		}
	    }
	}
    return( status );
    }

vmic2534_getioscanpvt( short card, IOSCANPVT *scanpvt )
    {
    if ( (card >= VMIC_2534_MAX_CARDS) || !(vmic2534Rec[ card ].vmicDevice) )
	return( -1 );
    *scanpvt = vmic2534Rec[ card ].ioscanpvt;
    return( 0 );
    }


/*
 *  VMEVMIC_BI_DRIVER
 *
 *  interface binary inputs
 */

vmic2534_bi_driver( register short card,
		u_int		mask,
		register u_int	*  prval )
    {
    if ( (card >= VMIC_2534_MAX_CARDS) || !(vmic2534Rec[ card ].vmicDevice) )
	return( -1 );

    *prval = (u_int)( (vmic2534Rec[ card ].vmicDevice->pioData)
	    & (vmic2534Rec[ card ].inputMask) )
		 & mask;
    return( 0 );
    }


/*
 *  vmic2534_bo_read
 *
 *  (read) interface to binary outputs
 *
 */

vmic2534_bo_read( register short card,
		u_int		mask,
		register u_int	* prval )
    {
    if ( (card >= VMIC_2534_MAX_CARDS) || !(vmic2534Rec[ card ].vmicDevice) )
	return( -1 );

    *prval = (u_int)(~( ( (vmic2534Rec[ card ].vmicDevice->pioData) 
	    & (vmic2534Rec[ card ].outputMask) )
		^ vmic2534Rec[card].outputCorrector) ) & mask;
    return( 0 );
    }


/*
 *  vmic2534_bo_driver
 *
 *  (write) interface to binary outputs
 */

vmic2534_bo_driver( register short card,
		register u_int	val,
		u_int		mask )
    {
    u_long work;

    if ( (card >= VMIC_2534_MAX_CARDS) || !(vmic2534Rec[ card ].vmicDevice) )
	return( ERROR );
    
#ifndef DEBUGTHIS


    vmic2534Rec[ card ].vmicDevice->pioData =
	    ( ( ( ~(vmic2534Rec[ card ].vmicDevice->pioData) & (u_long)~mask)
		|  (u_long)(val & mask) ) ^ vmic2534Rec[card].outputCorrector )
		    & vmic2534Rec[ card ].outputMask;
#else
    printf("Data = 0x%x,  mask = 0x%x,  corrector = 0x%x\n",
	val, mask, vmic2534Rec[card].outputCorrector);

    work = vmic2534Rec[ card ].vmicDevice->pioData;
    printf("Current raw i/o data = 0x%x, output mask = 0x%x\n",
	    work, vmic2534Rec[ card ].outputMask);
    work = ~work;
    printf("Corrected output data = 0x%x\n", work);
    work &= ( (u_long)~mask );
    printf("Masked data = 0x%x\n", work);
    work |= ( (u_long)(val & mask) );
    printf("New output data = 0x%x\n", work);
    work ^= vmic2534Rec[card].outputCorrector;
    printf("Polarity-corrected output data = 0x%x\n", work);
    work &= vmic2534Rec[ card ].outputMask;
    printf("Output data masked to output bits only = 0x%x\n", work);
    vmic2534Rec[ card ].vmicDevice->pioData = work;
    printf("*\n");
#undef DEBUGTHIS
#endif DEBUGTHIS
    return( OK );
    }


/*
 * vmiv2534Out
 *
 * test routine for vm1c2534 output
 */

vmiv2534Out( short	card,
	    u_long	val )
    {
    /*  check for valid card nr						 */
    if ( card >= VMIC_2534_MAX_CARDS )
	{
	logMsg("card # out of range/n");
	return( -1 );
	}

    /*  check for card actually installed				 */
    if ( !(vmic2534Rec[ card ].vmicDevice) )
	{
	logMsg("card #%d not found\n", card);
	return( -2 );
        }

    /*  set the physical output						 */
    vmic2534Rec[ card ].vmicDevice->pioData
	    = vmic2534Rec[ card ].outputCorrector ^ val;
    return( OK );
    }


/*
 *  vmic2534Write
 *
 *  command line interface to test bo driver
 */

vmic2534Write( short	card,
	    u_int	val )
    {
    return( vmic2534_bo_driver( card, val, 0xffff ) );
    }



long
vmic2534_io_report( short level )
    {
    int card;

    for ( card = 0; card < VMIC_2534_MAX_CARDS; card++ )
	{
	if ( vmic2534Rec[ card ].vmicDevice )
	    {
	    printf("B*: vmic2534:\tcard%d\n", card);
	    if( level >= 1 )
		{
		vmic2534_bi_io_report( card );
		vmic2534_bo_io_report( card );
		}
	    }
	}
    }

void
vmic2534_bi_io_report( int card )
    {
    short int num_chans,j,k,l,m,status;
    int ival,jval,kval,lval,mval;
    unsigned int   *prval;

    num_chans = VMIC_2534_MAX_CHANS;

    if( !vmic2534Rec[card].vmicDevice )
	    return;

    printf("\tVMEVMI-2534 BINARY IN CHANNELS:\n");
    for( j=0, k=1, l=2, m=3;
	    j < num_chans, k < num_chans, l< num_chans, m < num_chans;
		j+=IOR_MAX_COLS, k+= IOR_MAX_COLS, l+= IOR_MAX_COLS,
		    m += IOR_MAX_COLS )
	{
	if( j < num_chans )
	    {
	    vmic2534_bi_driver( card, masks( j ), &jval );
	    if ( jval != 0 )
		    jval = 1;
	    printf("\tChan %d = %x\t ", j, jval);
	    }

	if( k < num_chans )
	    {
	    vmic2534_bi_driver( card, masks( k ), &kval );
	    if ( kval != 0) 
		    kval = 1;
	    printf("Chan %d = %x\t ", k, kval);
	    }

	if( l < num_chans )
	    {
	    vmic2534_bi_driver( card, masks( l ), &lval );
	    if ( lval != 0 )
		    lval = 1;
	    printf("Chan %d = %x \t", l, lval);
	    }
	if( m < num_chans) 
	    {
	    vmic2534_bi_driver( card, masks( m ), &mval );
	    if ( mval != 0 )
		    mval = 1;
	    printf("Chan %d = %x \n", m, mval);
	    }
	}
    }


void
vmic2534_bo_io_report( int card )
    {
    short int num_chans,j,k,l,m,status;
    int ival,jval,kval,lval,mval;
    unsigned int   *prval;

    num_chans = VMIC_2534_MAX_CHANS;

    if( !vmic2534Rec[card].vmicDevice )
	    return;

    printf("\tVMEVMI-2534 BINARY OUT CHANNELS:\n");
    for( j=0, k=1, l=2, m=3;
	    j < num_chans, k < num_chans, l< num_chans, m < num_chans;
		j+=IOR_MAX_COLS, k+= IOR_MAX_COLS, l+= IOR_MAX_COLS,
		    m += IOR_MAX_COLS )
	{
	if( j < num_chans )
	    {
	    vmic2534_bo_read( card, masks( j ), &jval );
	    if ( jval != 0 )
		    jval = 1;
	    printf("\tChan %d = %x\t ", j, jval);
	    }

	if( k < num_chans )
	    {
	    vmic2534_bo_read( card, masks( k ), &kval );
	    if ( kval != 0) 
		    kval = 1;
	    printf("Chan %d = %x\t ", k, kval);
	    }

	if( l < num_chans )
	    {
	    vmic2534_bo_read( card, masks( l ), &lval );
	    if ( lval != 0 )
		    lval = 1;
	    printf("Chan %d = %x \t", l, lval);
	    }
	if( m < num_chans) 
	    {
	    vmic2534_bo_read( card, masks( m ), &mval );
	    if ( mval != 0 )
		    mval = 1;
	    printf("Chan %d = %x \n", m, mval);
	    }
	}
    }
