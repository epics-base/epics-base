/* drvGpib.h */
/* base/src/drv $Id$ */

/*
 *	Origional Author:  Unknown, probably National Instruments
 *      Author: John Winans
 *      Date:   10-27-91
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
 * .01  10-27-91	winans	moved into epics area
 */


/* GPIB-1014 engineering software package UNIX include file */

/*
 * The following structure dma_chan defines the memory map of a single
 * channel on the Hitachi 68450.
 */

struct  dma_chan {
	char    csr;   /* +0  channel status register */
	char    cer;   /* +1  channel error register */
	 char f0[2];
	char    dcr;   /* +4  device control register */
	char    ocr;   /* +5  operation control register */
	char    scr;   /* +6  sequence control register */
	char    ccr;   /* +7  channel control register */
	 char f1[2];
unsigned short  mtc;   /* +10 memory transfer counter */
	long    mar;   /* +12 memory address register */
	 char f2[4];
	long    dar;   /* +20 device address register */
	 char f3[2];
unsigned short  btc;   /* +26 base transfer counter */
	long    bar;   /* +28 base address register */
	 char f4[5];
	char    niv;   /* +37 normal interrupt vector */
	 char f5;
	char    eiv;   /* +39 error interrupt vector */
	 char f6;
	char    mfc;   /* +41 memory function codes */
	 char f7[3];
	char    cpr;   /* +45 channel priority register */
	 char f8[3];
	char    dfc;   /* +49 device function codes */
	 char f9[7];
	char    bfc;   /* +57 base function codes */
	 char fA[6];
};

/*
 * The structure ibregs defines the address space of the GPIB-1014.
 */
struct ibregs {

	struct dma_chan  ch0;   /* +0   dma channel 0 */
	struct dma_chan  ch1;   /* +64  dma channel 1 */
	struct dma_chan  ch2;   /* +128 dma channel 2 */
	struct dma_chan  ch3;   /* +192 dma channel 3 */
#define gcr     ch3.fA[5]       /* +255 general control register */
	char fB;
	char    cfg1;           /* +257 config reg 1 */
#define	gsr	cfg1		/* +257 GSR */
	char fC[3];
	char    cfg2;           /* +261 config reg 2 */
	char fD[10];
	char    fE, cdor;       /* +273 byte out register                */
	char    fF, imr1;       /* +275 interrupt mask register 1        */
	char    f10,imr2;       /* +277 interrupt mask register 2        */
	char    f11,spmr;       /* +279 serial poll mode register        */
	char    f12,admr;       /* +281 address mode register            */
	char    f13,auxmr;      /* +283 auxiliary mode register          */
	char    f14,adr;        /* +285 address register 0/1             */
	char    f15,eosr;       /* +287 end of string register           */
	char	f16[512-289];	/* +289 filler to rest of window         */
};
/*
* The structure srqTable defines the srq status of each of 31 possible devices
*/
struct srqTable {
	short	active;
	char	lad;
	char	tad;
	unsigned   char	status;
	SEM_ID  pSem;
} ;

/* 7210 readable registers */
#define dir     cdor
#define isr1    imr1
#define isr2    imr2
#define spsr    spmr
#define adsr    admr
#define cptr    auxmr
#define adr0    adr
#define adr1    eosr

/* GPIB status register */
#define	GSR_EOI	0x80
#define	GSR_ATN	0x40
#define	GSR_SRQ	0x20
#define	GSR_REN	0x10
#define	GSR_IFC	0x08
#define	GSR_NRFD 0x04
#define	GSR_NDAC 0x02
#define	GSR_DAV  0x01

/* 68450 DMAC register definitions */

/* Device Control Register (dcr) bits */
#define D_CS     0x80            /* cycle steal mode */
#define D_CSM    0x80            /* cycle steal mode,with bus Monitor*/
#define D_CSH    0xC0            /* cycle steal with hold */
#define D_CSHM   0xC0            /* cycle steal with hold,with bus Monitor */
#define D_IACK   0x20            /* device with DMAACK, implicitly addressed */
#define D_P16    0x08            /* 16 bit device port size */
#define D_IPCL   0x01            /* PCL set to status input with interrupt */

/* Operation Control Register (ocr) bits */
#define D_MTD    0x00            /* direction is from memory to device */
#define D_DTM    0x80            /* direction is from device to memory */
#define D_TW     0x10            /* transfer size is word */
#define D_TL     0x30            /* transfer size is long word */
#define D_ACH    0x08            /* array chaining */
#define D_LACH   0x0C            /* linked array chaining */
#define D_ARQ    0x03            /* auto request first transfer, then external*/
#define D_XRQ    0x02            /* external request mode */
#define D_ARM    0x01            /* auto request at maximum rate */


/* Sequence Control Register (scr) bits */
#define D_MCD    0x08            /* memory address counts down */
#define D_MCU    0x04            /* memory address counts up */
#define D_DCD    0x02            /* device address counts down */
#define D_DCU    0x01            /* device address counts up */

/* Channel Control Register (ccr) bits */
#define D_SRT    0x80            /* start channel operation */
#define D_CON    0x40            /* continue */
#define D_HLT    0x20            /* halt channel operation */
#define D_SAB    0x10            /* software abort */
#define D_EINT   0x08            /* enable channel interrupts */

/* Channel Status Register (csr) bits */
#define D_CLEAR  0xFF            /* clear all bits */
#define D_COC    0x80            /* channel operation complete */
#define D_BTC    0x40            /* block transfer complete */
#define D_NDT    0x20            /* normal device termination */
#define D_ERR    0x10            /* error as coded in cer */
#define D_ACT    0x08            /* channel active */
#define D_PCLT   0x02            /* PCL transition occurred */
#define D_PCLH   0x01            /* PCL line is high */
#define D_NSRQ   0x01            /* Not SRQ (gpib line) */

/* Channel Error Register (cer) bits */
#define D_ECF    0x01            /* configuration error */
#define D_ETM    0x02            /* operation timing error */
#define D_EMA    0x05            /* memory address error */
#define D_EDA    0x06            /* device address error */
#define D_EBA    0x07            /* base address error */
#define D_EBUS   0x08            /* bus error */
#define D_ECT    0x0C            /* transfer count error */
#define D_EEAB   0x01            /* external abort */
#define D_ESAB   0x11            /* software abort */

/* Channel Priority Register (cpr) bits */
#define D_PR1    0x01            /* priority 1 */
#define D_PR2    0x02            /* priority 2 */
#define D_PR3    0x03            /* priority 3 */

/* Function Code Register (fcr) bits */
#define D_SUP    0x04            /* supervisor access */
#define D_S24    0x02            /* standard 24 bit addressing */
#define D_PSA    0x01            /* program space access */

/* Configuration Register 1 (cfg1) bits */
#define D_OUT    0               /* direction memory to GPIB */
#define D_IN    (1<<0)           /* direction GPIB to memory */
#define D_DBM   (1<<1)           /* disarm Bus Monitor mode */
#define D_ECC   (1<<2)           /* arm automatic carry cycle feature */
#define D_BRG0  (00<<3)          /* select bus request/grant line 1 */
#define D_BRG1  (01<<3)          /* select bus request/grant line 1 */
#define D_BRG2  (02<<3)          /* select bus request/grant line 2 */
#define D_BRG3  (03<<3)          /* select bus request/grant line 3 */


/* Configuration Register 2 (cfg2) bits */
#define D_SC    (1<<0)          /* set system controller (SC) bit */
#define D_LMR   (1<<1)          /* set local master reset bit */
#define D_SPAC  (1<<2)          /* set supervisor only access to board */
#define D_SFL   (1<<3)          /* clear SYSFAIL line */


/* Control masks for hidden registers (auxmr) */

#define ICR     0040
#define PPR     0140
#define AUXRA   0200
#define AUXRB   0240
#define AUXRE   0300
#define CNT     0340    /* OR of all of above */

/* 7210 bits:           POSITION           7210 reg     */

#define HR_DI           (1<<0)          /* ISR1         */
#define HR_DO           (1<<1)          /*  ,           */
#define HR_ERR          (1<<2)          /*  ,           */
#define HR_DEC          (1<<3)          /*  ,           */
#define HR_END          (1<<4)          /*  ,           */
#define HR_DET          (1<<5)          /*  ,           */
#define HR_APT          (1<<6)          /*  ,           */
#define HR_CPT          (1<<7)          /*  ,           */
#define HR_DIIE         (1<<0)          /* IMR1         */
#define HR_DOIE         (1<<1)          /*  ,           */
#define HR_ERRIE        (1<<2)          /*  ,           */
#define HR_DECIE        (1<<3)          /*  ,           */
#define HR_ENDIE        (1<<4)          /*  ,           */
#define HR_DETIE        (1<<5)          /*  ,           */
#define HR_ADSC         (1<<0)          /* ISR2         */
#define HR_REMC         (1<<1)          /*  ,           */
#define HR_LOKC         (1<<2)          /*  ,           */
#define HR_CO           (1<<3)          /*  ,           */
#define HR_REM          (1<<4)          /*  ,           */
#define HR_LOK          (1<<5)          /*  ,           */
#define HR_SRQI         (1<<6)          /*  ,           */
#define HR_INT          (1<<7)          /*  ,           */
#define HR_ACIE         (1<<0)          /* IMR2         */
#define HR_REMIE        (1<<1)          /*  ,           */
#define HR_LOKIE        (1<<2)          /*  ,           */
#define HR_COIE         (1<<3)          /*  ,           */
#define HR_DMAI         (1<<4)          /*  ,           */
#define HR_DMAO         (1<<5)          /*  ,           */
#define HR_SRQIE        (1<<6)          /*  ,           */
#define HR_PEND         (1<<6)          /* SPSR         */
#define HR_RSV          (1<<6)          /* SPMR         */
#define HR_MJMN         (1<<0)          /* ADSR         */
#define HR_TA           (1<<1)          /*  ,           */
#define HR_LA           (1<<2)          /*  ,           */
#define HR_TPAS         (1<<3)          /*  ,           */
#define HR_LPAS         (1<<4)          /*  ,           */
#define HR_SPMS         (1<<5)          /*  ,           */
#define HR_NATN         (1<<6)          /*  ,           */
#define HR_CIC          (1<<7)          /*  ,           */
#define HR_ADM0         (1<<0)          /* ADMR         */
#define HR_ADM1         (1<<1)          /*  ,           */
#define HR_TRM0         (1<<4)          /*  ,           */
#define HR_TRM1         (1<<5)          /*  ,           */
#define HR_LON          (1<<6)          /*  ,           */
#define HR_TON          (1<<7)          /*  ,           */
#define HR_DL           (1<<5)          /* ADR          */
#define HR_DT           (1<<6)          /*  ,           */
#define HR_ARS          (1<<7)          /*  ,           */

#define HR_HLDA         (1<<0)          /* auxra        */
#define HR_HLDE         (1<<1)          /*  ,           */
#define HR_REOS         (1<<2)          /*  ,           */
#define HR_XEOS         (1<<3)          /*  ,           */
#define HR_BIN          (1<<4)          /*  ,           */
#define HR_CPTE         (1<<0)          /* auxrb        */
#define HR_SPEOI        (1<<1)          /*  ,           */
#define HR_TRI          (1<<2)          /*  ,           */
#define HR_INV          (1<<3)          /*  ,           */
#define HR_ISS          (1<<4)          /*  ,           */
#define HR_PPS          (1<<3)          /* ppr          */
#define HR_PPU          (1<<4)          /*  ,           */

/* 7210 Auxiliary Commands */

#define AUX_PON         000     /* Immediate Execute pon                  */
#define AUX_CR          002     /* Chip Reset                             */
#define AUX_FH          003     /* Finish Handshake                       */
#define AUX_TRIG        004     /* Trigger                                */
#define AUX_RTL         005     /* Return to local                        */
#define AUX_SEOI        006     /* Send EOI                               */
#define AUX_NVAL        007     /* Non-Valid Secondary Command or Address */
#define AUX_VAL         017     /* Valid Secondary Command or Address     */
#define AUX_CPPF        001     /* Clear Parallel Poll Flag               */
#define AUX_SPPF        011     /* Set Parallel Poll Flag                 */
#define AUX_TCA         021     /* Take Control Asynchronously            */
#define AUX_TCS         022     /* Take Control Synchronously             */
#define AUX_TCSE        032     /* Take Control Synchronously on End      */
#define AUX_GTS         020     /* Go To Standby                          */
#define AUX_LTN         023     /* Listen                                 */
#define AUX_LTNC        033     /* Listen in Continuous Mode              */
#define AUX_LUN         034     /* Local Unlisten                         */
#define AUX_EPP         035     /* Execute Parallel Poll                  */
#define AUX_SIFC        036     /* Set IFC                                */
#define AUX_CIFC        026     /* Clear IFC                              */
#define AUX_SREN        037     /* Set REN                                */
#define AUX_CREN        027     /* Clear REN                              */
#define AUX_DSC         024     /* Disable System Control                 */

