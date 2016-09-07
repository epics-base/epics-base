/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* module_types.h */
/*
 * 	Author:      Bob Dalesio
 * 	Date:        12-07-88
 */

#ifndef INCLmodule_typesh
#define INCLmodule_typesh

/* Device module types */
/*
 * all devices have corresponding entries in ~operator/db/src/menus.c
 * changes must be made in both areas to keep the database and drivers in sync
 */
/* & in comment indicates tested with card  0 */
/* % in comment indicates tested with card other than card 0 */
/* # in comment indicates that the Nth card has been tested */
/* !! never been tested */

/*
 *	@#  If any changes are made to this file, check the procedures 
 *	ab_card, and vme_card in signallist.c, and get_address in sigmenu.c.
 */

#ifdef MODULE_TYPES_INIT
#define MODULE_TYPES_DEF(MT_DEF_PARM)  MT_DEF_PARM
#else
#define MODULE_TYPES_DEF(MT_DEF_PARM) extern MT_DEF_PARM;
#endif

/* Number of columns used in io_report. */
#define IOR_MAX_COLS 4

/* I/O types */
#define IO_AI		0
#define IO_AO		1
#define IO_BI		2
#define IO_BO		3
#define IO_SM   	4
#define IO_WF  		5
#define	IO_TIMER	6
#define MAX_IO_TYPE	IO_TIMER

/* bus types */
/* must correspond to the values in link types */
/* these defines are in ~gta/dbcon/h/link.h */


/* equates for the Allen-Bradley cards. */
#define	AB_BASE_ADDR	0xc00000	/* base addr of first AB6008SV */
#define AB_MAX_LINKS    2       /* number of serial links from VME */
#define AB_MAX_ADAPTERS 8       /* number of physical adapters on a link */
#define AB_MAX_CARDS    16      /* max number of IO cards per adapter */
#define AB_CARD_ADAPTER 16      /* cards per logical adapter */
#define AB_CHAN_CARD    16      /* max channels per card */

/* analog inputs */
#define AB1771IL	0	/* &% Allen-Bradley low level analog input */
#define AB1771IFE	1	/* &% Allen-Bradley low level analog input */
#define	AB1771IXE	2	/* &% Allen-Bradley millivolt input */
#define XY566SE		3	/* & Xycom 12-bit Single Ended Scanned*/
#define XY566DI		4	/* &% Xycom 12-bit Differential Scanned */
#define XY566DIL	5	/* &% Xycom 12-bit Differential Latched */
#define	VXI_AT5_AI	6	/* % AT-5 VXI module's Analog Inputs */
#define AB1771IFE_SE	7	/* % A-B IFE in 16 single-ended input mode */
#define AB1771IFE_4to20MA 8	/* % A-B IFE in 8 double-ended 4to20Ma	*/
#define DVX2502         9       /* &% DVX_2502 128 chan 16 bit differential */
#define AB1771IFE_0to5V 10	/* % A-B IFE in 8 double-ended 4to20Ma	*/
#define KSCV215		11	/* % KSC V215 VXI 16 bit differential */
#define AB1771IrPlatinum 12	/* % A-B RTD Platinum */
#define AB1771IrCopper 13	/* % A-B RTD Copper */
#define MAX_AI_TYPES	AB1771IrCopper	
MODULE_TYPES_DEF(short ai_num_cards[MAX_AI_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={12,12,12, 4, 4, 6,32,12,12,  1, 12, 32, 12,12};
#endif
MODULE_TYPES_DEF(short ai_num_channels[MAX_AI_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={ 8, 8, 8,32,16,16, 8,16, 8, 127,  8, 32,6,6};
#endif
MODULE_TYPES_DEF(short ai_interruptable[MAX_AI_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={0, 0, 0, 0, 0, 1, 1, 0, 0,  1,  0, 0,0,0};
#endif
MODULE_TYPES_DEF(short ai_bus[MAX_AI_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={ 4, 4, 4, 2, 2, 2, 2, 4, 4,  2,  4, 2,4,4};
#endif
MODULE_TYPES_DEF(unsigned short ai_addrs[MAX_AI_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={ 0,0,0,0x6000,0x7000,0xe000, 0xc014,0,0, 0xff00, 0, 0,0,0};
#endif
MODULE_TYPES_DEF(long ai_memaddrs[MAX_AI_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={0,0,0,0x000000,0x040000,0x0c0000, 0,0,0, 0x100000, 0, 0,0,0};
#endif

/* analog outputs */
#define AB1771OFE	0	/* &% Allen-Bradley 12 bit Analog Output */
#define VMI4100		1	/* & VMIC VMIVME 4100 */
#define ZIO085		2 	/* & Ziomek 085 */
#define	VXI_AT5_AO	3	/* !! AT-5 VXI modules analog outputs */
#define MAX_AO_TYPES	VXI_AT5_AO
MODULE_TYPES_DEF(short ao_num_cards[MAX_AO_TYPES+1])
#ifdef MODULE_TYPES_INIT
                = {12,  4,  1, 32};
#endif
MODULE_TYPES_DEF(short ao_num_channels[MAX_AO_TYPES+1])
#ifdef MODULE_TYPES_INIT
                =  { 4, 16, 32, 16};
#endif
MODULE_TYPES_DEF(short ao_interruptable[MAX_AO_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                = { 0,  0,  0,  1};
#endif
MODULE_TYPES_DEF(short ao_bus[MAX_AO_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={ 4,  2,  2,  2};
#endif
MODULE_TYPES_DEF(unsigned short ao_addrs[MAX_AO_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={ 0,0x4100,0x0800, 0xc000};
#endif

/* binary inputs */
#define	ABBI_08_BIT	0	/* &% Allen-Bradley generic Binary In  8 bit */
#define	ABBI_16_BIT	1	/* &% Allen-Bradley generic Binary In 16 bit */
#define BB910		2	/* & BURR BROWN MPV 910 (relay) */
#define XY210		3	/* &% XYcom 32 bit binary in */
#define	VXI_AT5_BI	4	/* !! AT-5 VXI modules binary inputs */
#define HPE1368A_BI	5	/* !! HP E1368A video switch */
#define AT8_FP10S_BI	6	/* !! AT8 FP10 slave fast protect */
#define XY240_BI	7	/* !! Xycom 32 bit binary in / 32 bit binary out */
#define	MAX_BI_TYPES	XY240_BI	
MODULE_TYPES_DEF(short bi_num_cards[MAX_BI_TYPES+1] )
#ifdef MODULE_TYPES_INIT
		={ 12, 12,  4,  4, 32, 32, 8, 2};
#endif
MODULE_TYPES_DEF(short bi_num_channels[MAX_BI_TYPES+1] )
#ifdef MODULE_TYPES_INIT
		={ 8, 16, 32, 32, 32, 16, 32, 32};
#endif
MODULE_TYPES_DEF(short bi_interruptable[MAX_BI_TYPES+1] )
#ifdef MODULE_TYPES_INIT
		={  1,  1,  0,  0,  1, 1, 1, 1};
#endif
MODULE_TYPES_DEF(short bi_bus[MAX_BI_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={  4,  4,  2,  2,  2, 2, 2, 2};
#endif
MODULE_TYPES_DEF(unsigned short bi_addrs[MAX_BI_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={ 0,0,0xb800,0xa000, 0xc000, 0xc000, 0x0e00, 0xd000};
#endif

/* binary outputs */
#define	ABBO_08_BIT	0	/* &% Allen-Bradley 8 bit binary out */
#define	ABBO_16_BIT	1	/* &% Allen-Bradley 16 bit binary out */
#define BB902		2	/* &% BURR BROWN MPV 902 (relay) */
#define XY220		3	/* &% XYcom 32 bit binary out */
#define	VXI_AT5_BO	4	/* !! AT-5 VXI modules binary outputs */
#define HPE1368A_BO	5	/* !! HP E1368A video switch */
#define AT8_FP10M_BO	6	/* !! AT8 FP10 master fast protect */
#define XY240_BO	7	/* !! Xycom 32 bit binary in / 32 bit binary out */
#define	MAX_BO_TYPES	XY240_BO	
MODULE_TYPES_DEF(short bo_num_cards[MAX_BO_TYPES+1] )
#ifdef MODULE_TYPES_INIT
		={12, 12,  4,  1, 32, 32, 2, 2};
#endif
MODULE_TYPES_DEF(short bo_num_channels[MAX_BO_TYPES+1] )
#ifdef MODULE_TYPES_INIT
		={ 8, 16, 32, 32, 32, 16, 32, 32};
#endif
MODULE_TYPES_DEF(short bo_interruptable[MAX_BO_TYPES+1] )
#ifdef MODULE_TYPES_INIT
		={ 0,  0,  0,  0,  1, 0, 0, 1 };
#endif
MODULE_TYPES_DEF(short bo_bus[MAX_BO_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={ 4,  4,  2,  2,  2, 2, 2, 2 };
#endif
MODULE_TYPES_DEF(unsigned short bo_addrs[MAX_BO_TYPES+1])
#ifdef MODULE_TYPES_INIT
		={ 0,0,0xd800,0xc800, 0xc000, 0xc000, 0x0c00, 0xd000};
#endif

/* stepper motor drivers */
#define	CM57_83E	0	/* & Compumotor 57-83E motor controller */
#define	OMS_6AXIS	1	/* & OMS six axis motor controller */
#define	MAX_SM_TYPES	OMS_6AXIS
MODULE_TYPES_DEF(short sm_num_cards[MAX_SM_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                ={ 8, 8 };
#endif
MODULE_TYPES_DEF(short sm_num_channels[MAX_SM_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                =  { 1, 8};
#endif
MODULE_TYPES_DEF(short sm_interruptable[MAX_SM_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                = { 0, 0 };
#endif
MODULE_TYPES_DEF(short sm_bus[MAX_SM_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={ 2, 2 };
#endif
MODULE_TYPES_DEF(unsigned short sm_addrs[MAX_SM_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={ 0x8000, 0xfc00 };
#endif

/* waveforms */
#define XY566WF		0	/* & Xycom 566 as a waveform */
#define CAMAC_THING	1	/* !! CAMAC waveform digitizer */
#define JGVTR1		2	/* & Joerger transient recorder */
#define	COMET		3	/* !! COMET transient recorder */
#define MAX_WF_TYPES	COMET
MODULE_TYPES_DEF(short wf_num_cards[MAX_WF_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                ={4, 4, 8, 4};
#endif
MODULE_TYPES_DEF(short wf_num_channels[MAX_WF_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                ={1, 1, 1, 4};
#endif
MODULE_TYPES_DEF(short wf_interruptable[MAX_WF_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                = {0, 0, 0, 0};
#endif
MODULE_TYPES_DEF(short wf_bus[MAX_WF_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={2, 3, 2, 2};
#endif
MODULE_TYPES_DEF(unsigned short wf_addrs[MAX_WF_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={0x9000, 0, 0xB000, 0xbc00};
#endif
MODULE_TYPES_DEF(unsigned short wf_armaddrs[MAX_WF_TYPES+1])
#ifdef MODULE_TYPES_INIT
                = {0x5400, 0, 0, 0};
#endif
MODULE_TYPES_DEF(long wf_memaddrs[MAX_WF_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={0x080000, 0, 0xb80000, 0xe0000000};
#endif


/* timing cards */
#define MZ8310		0	/* &% Mizar Timing Module */
#define DG535		1	/* !! GPIB timing instrument */
#define	VXI_AT5_TIME	2	/* !! AT-5 VXI modules timing channels */
#define MAX_TM_TYPES	VXI_AT5_TIME
MODULE_TYPES_DEF(short tm_num_cards[MAX_TM_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                ={ 4, 1,  32 };
#endif
MODULE_TYPES_DEF(short tm_num_channels[MAX_TM_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                ={10, 1, 10};
#endif
MODULE_TYPES_DEF(short tm_interruptable[MAX_TM_TYPES+1] )
#ifdef MODULE_TYPES_INIT
                = { 1, 0,  1 };
#endif
MODULE_TYPES_DEF(short tm_bus[MAX_TM_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={ 2, 5,  2 };
#endif
MODULE_TYPES_DEF(unsigned short tm_addrs[MAX_TM_TYPES+1])
#ifdef MODULE_TYPES_INIT
                ={0xf800, 0, 0xc000 };
#endif

/* AT830X clock cards */
MODULE_TYPES_DEF(long AT830X_1_addrs )
#ifdef MODULE_TYPES_INIT
                = 0x0400;
#endif
MODULE_TYPES_DEF(short AT830X_1_num_cards )
#ifdef MODULE_TYPES_INIT
                = 2;
#endif
MODULE_TYPES_DEF(long AT830X_addrs )
#ifdef MODULE_TYPES_INIT
                = 0xaa0000;
#endif
MODULE_TYPES_DEF(short AT830X_num_cards )
#ifdef MODULE_TYPES_INIT
                = 2;
#endif

/* 
 * system controller cards. 
 * (driver looks for only one card)
 */
MODULE_TYPES_DEF(long xy010ScA16Base)
#ifdef MODULE_TYPES_INIT
                = 0x0000;
#endif
/* 
 *	limit the size of the VXI logical address space
 *
 *	<VXI LA BASE> = <VME SHORT ADDR BASE> + 0xc000
 *	
 *	LA			VME address
 *	0	 		<VXI LA BASE>
 *	EPICS_VXI_LA_COUNT	<VXI LA BASE> + (EPICS_VXI_LA_COUNT-1)*64
 */
MODULE_TYPES_DEF(unsigned char EPICS_VXI_LA_COUNT)
#ifdef MODULE_TYPES_INIT
		= 32;
#endif

/*
 *
 *	address ranges for VXI A24 and A32 devices
 *
 */
MODULE_TYPES_DEF(char *EPICS_VXI_A24_BASE)
#ifdef MODULE_TYPES_INIT
		= (char *) 0x900000;
#endif
MODULE_TYPES_DEF(unsigned long EPICS_VXI_A24_SIZE)
#ifdef MODULE_TYPES_INIT
		= 0x100000;
#endif
MODULE_TYPES_DEF(char *EPICS_VXI_A32_BASE)
#ifdef MODULE_TYPES_INIT
		= (char *) 0x90000000;
#endif
MODULE_TYPES_DEF(unsigned long EPICS_VXI_A32_SIZE)
#ifdef MODULE_TYPES_INIT
		= 0x10000000;
#endif


/******************************************************************************
 *
 * Interrupt vector locations used by the MV167 CPU board. 
 * These are defined in mv167.h
 *
 *      PCC2_INT_VEC_BASE       0x40     PCC interrupt vector base number 
 *                                       any multiple of 0x10 
 *      UTIL_INT_VEC_BASE0      0x50     VMEchip2 utility interrupt 
 *                                       vector base number 
 *                                       any multiple of 0x10 
 *      UTIL_INT_VEC_BASE1      0x60     VMEchip2 utility interrupt 
 *                                       vector base number 
 *                                       any multiple of 0x10 
 *
 *      INT_VEC_CD2400_A        0x90     int vec for channel A 
 *      INT_VEC_CD2400_B        0x94     int vec for channel B 
 *      INT_VEC_CD2400_C        0x98     int vec for channel C 
 *      INT_VEC_CD2400_D        0x9c     int vec for channel D 
 *     
 *      LANC_IRQ_LEVEL          3        LNANC IRQ level 
 *      MPCC_IRQ_LEVEL          4        serial comm IRQ level
 *      SYS_CLK_LEVEL           6        interrupt level for sysClk
 *      AUX_CLK_LEVEL           5        interrupt level for auxClk
 *      SCSI_IRQ_LEVEL          2        scsi interrupt level
 *
 ******************************************************************************/

/* interrupt vector allocation - one for each XY566 DIL card */
MODULE_TYPES_DEF(int AI566_VNUM)
#ifdef MODULE_TYPES_INIT
		=0xf8;	/* Xycom 566 Differential Latched */
#endif

/* interrupt vector allocation - one for each DVX card */
MODULE_TYPES_DEF(int DVX_IVEC0)
#ifdef MODULE_TYPES_INIT
		=0xd0;
#endif

/* stepper motor interrupt vector - one for each motor */
MODULE_TYPES_DEF(int MD_INT_BASE)
#ifdef MODULE_TYPES_INIT
		=0xf0;	/* base of the motor int vector */
#endif

/* I reserve from here up to num_cards * 4 interrupting chans/card - joh */
MODULE_TYPES_DEF(int MZ8310_INT_VEC_BASE)
#ifdef MODULE_TYPES_INIT
		=0xe8;
#endif

/* Allen-Bradley Serial Driver - MAX_AB_LINKS number of vectors */
MODULE_TYPES_DEF(int	AB_VEC_BASE)
#ifdef MODULE_TYPES_INIT
		=0x60;
#endif

/* only one interrupt vector allocated for all Joerger VTR1 boards joh */
MODULE_TYPES_DEF(int JGVTR1_INT_VEC)
#ifdef MODULE_TYPES_INIT
		=0xe0;
#endif

/* AT830X_1 cards have 1 intr vector for each AT830X_1_num_cards (presently 2) */
MODULE_TYPES_DEF(int	AT830X_1_IVEC0)
#ifdef MODULE_TYPES_INIT
		=0xd4;
#endif

/* AT830X cards have 1 intr vector for each AT830X_num_cards (presently 2) */
MODULE_TYPES_DEF(int	AT830X_IVEC0)
#ifdef MODULE_TYPES_INIT
		=0xd6;
#endif

/* AT8 fast protect interrupt vector base */
MODULE_TYPES_DEF(int	AT8FP_IVEC_BASE)
#ifdef MODULE_TYPES_INIT
		=0xa2;
#endif


MODULE_TYPES_DEF(int	AT8FPM_IVEC_BASE	)
#ifdef MODULE_TYPES_INIT
		=0xaa;
#endif


/******************************************************************************
 *
 * Addresses and IRQ information used by the XVME402 bitbus cards.
 *
 ******************************************************************************/
MODULE_TYPES_DEF(unsigned short BB_SHORT_OFF )
#ifdef MODULE_TYPES_INIT
		= 0x1800;  /* the first address of link 0's region */
#endif
#define    BB_NUM_LINKS    4       /* max number of BB ports allowed */
MODULE_TYPES_DEF(int BB_IVEC_BASE  )
#ifdef MODULE_TYPES_INIT
		=  0xa0;    /* vectored interrupts (2 used for each link) */
#endif
MODULE_TYPES_DEF(int BB_IRQ_LEVEL   )
#ifdef MODULE_TYPES_INIT
		= 5;       /* IRQ level */
#endif

/******************************************************************************
 *
 * Information for the PEP modular Bitbus boards.
 *
 ******************************************************************************/
MODULE_TYPES_DEF(unsigned short PEP_BB_SHORT_OFF )
#ifdef MODULE_TYPES_INIT
		= 0x1c00;
#endif
MODULE_TYPES_DEF(int PEP_BB_IVEC_BASE )
#ifdef MODULE_TYPES_INIT
		= 0xe8;
#endif

/******************************************************************************
 *
 * Addresses and IRQ information used by the NI1014 and NI1014D bitbus cards.
 *
 ******************************************************************************/
MODULE_TYPES_DEF(unsigned short NIGPIB_SHORT_OFF)
#ifdef MODULE_TYPES_INIT
		=  0x5000;/* First address of link 0's region */
#endif
                                /* Each link uses 0x0200 bytes */
#define    NIGPIB_NUM_LINKS  4     /* Max number of NI GPIB ports allowed */
MODULE_TYPES_DEF(int NIGPIB_IVEC_BASE )
#ifdef MODULE_TYPES_INIT
		= 100;   /* Vectored interrupts (2 used for each link) */
#endif
MODULE_TYPES_DEF(int NIGPIB_IRQ_LEVEL  )
#ifdef MODULE_TYPES_INIT
		=5;     /* IRQ level */
#endif

#if 0	/* JRW */
#define NI1014_LINK_NUM_BASE    0
#endif

/*
 * nothing after this endif
 */
#endif /*INCLmodule_typesh*/
