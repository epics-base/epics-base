/* $Id$
 *
 *	VXI standard defines
 *
 * 	Author:      Jeff Hill
 * 	Date:        11-89
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
 * .01 joh 021490	changed NVXIADDR to 32 before formal release
 * .02 joh 120591	reorganized to support nivxi
 * .03 joh 010892	added message based device registers and
 *			commands
 * .04 joh 013091	moved some of the slot 0 stuff here
 * .05 joh 070692	added KSC manufacturer id
 * .06 joh 070692	added address space defines
 * .07 joh 081992	added csr typedef	
 * .08 joh 071593	typedef for device registers
 * .09 joh 051995	fixed incorrect MBC_TOP_LEVEL_CMDR def	
 *
 */

#ifndef INCepvxih
#define INCepvxih

#include <sys/types.h>

/*
 * offset from the bottom of VME short addr of
 * VXI logical address space
 */
#define VXIBASEADDR	0xc000

typedef volatile uint16_t vxi16_t;
typedef volatile uint8_t vxi8_t;

/*
 * set NVXIADDR to less than 0x100 since other VME modules 
 * currently live in VXI address space
 */
#define NVXIADDR	(0x100)
#define NVXISLOTS	13
#define VXIDYNAMICADDR	(0xff)
#define NVXIADDRBITS	8
#define VXIADDRMASK	((1<<NVXIADDRBITS)-1)
#define VXI_RESMAN_LA	0
#define VXI_NBBY	8
#define VXIDEVBITS	6
#define VXIDEVSIZE	(1<<VXIDEVBITS)

#define VXI_LA_TO_PA(LA, VXIBASEADDR) \
( (VXICSR *) (((char *)VXIBASEADDR)+(((unsigned)LA)<<VXIDEVBITS)) )

#define VXI_PA_TO_LA(PA)	( (((unsigned)PA)>>VXIDEVBITS)&VXIADDRMASK )

struct vxi_msg_dev_w{
        vxi16_t          signal;
        vxi16_t          dextended;
        vxi16_t          dhigh;
        vxi16_t          dlow;
};

struct vxi_mxi_dev_w{
        vxi16_t         modid;
	vxi16_t		la_window;
	vxi8_t		a16_window_high;
	vxi8_t		a16_window_low;
	vxi8_t		a24_window_high;
	vxi8_t		a24_window_low;
	vxi8_t		a32_window_high;
	vxi8_t		a32_window_low;
        vxi16_t         INTX_interrupt;
        vxi16_t         INTX_trigger;
        vxi16_t         dd7;
        vxi16_t         INTX_utility;
        vxi16_t		dd9;
        vxi16_t         dd10;
	vxi16_t		subclass;
	vxi16_t		control;
	vxi16_t		lock;
	vxi16_t		irq;
	vxi16_t		trigger_drive;
	vxi16_t		trigger_mode;
	vxi16_t		interrupt;
	vxi16_t		status_id;
	vxi16_t		trigger_config;
	vxi16_t		trigger_sync_ack;
	vxi16_t		trigger_async_ack;
	vxi16_t		irq_ack;
};

struct vxi_reg_dev_w{
        vxi16_t         ddx08;
        vxi16_t         ddxoa;
        vxi16_t         ddx0c;
        vxi16_t         ddx0e;
        vxi16_t         ddx10;
        vxi16_t         ddx12;
        vxi16_t         ddx14;
        vxi16_t         ddx16;
        vxi16_t         ddx18;
        vxi16_t         ddx1a;
        vxi16_t         ddx1c;
        vxi16_t         ddx1e;
        vxi16_t         ddx20;
        vxi16_t         ddx22;
        vxi16_t         ddx24;
        vxi16_t         ddx26;
        vxi16_t         ddx28;
        vxi16_t         ddx2a;
        vxi16_t         ddx2c;
        vxi16_t         ddx2e;
};

struct vxi_reg_slot0_dev_w{
        vxi16_t         modid;
        vxi16_t         ddxoa;
        vxi16_t         ddx0c;
        vxi16_t         ddx0e;
        vxi16_t         ddx10;
        vxi16_t         ddx12;
        vxi16_t         ddx14;
        vxi16_t         ddx16;
        vxi16_t         ddx18;
        vxi16_t         ddx1a;
        vxi16_t         ddx1c;
        vxi16_t         ddx1e;
        vxi16_t         ddx20;
        vxi16_t         ddx22;
        vxi16_t         ddx24;
        vxi16_t         ddx26;
        vxi16_t         ddx28;
        vxi16_t         ddx2a;
        vxi16_t         ddx2c;
        vxi16_t         ddx2e;
};

struct vxi_csr_w{
	vxi16_t		addr;
	vxi16_t		pad0;
	vxi16_t		control;
	vxi16_t		offset;
	union {
		struct vxi_msg_dev_w		msg;
		struct vxi_mxi_dev_w		mxi;
		struct vxi_reg_dev_w		reg;
		struct vxi_reg_slot0_dev_w	reg_s0;
	}dd;
};

struct vxi_msg_dev_r{
        vxi16_t         protocol;
        vxi16_t         response;
        vxi16_t         dhigh;
        vxi16_t         dlow;
};

struct vxi_mxi_dev_r{
        vxi16_t         modid;
        vxi16_t         la_window;
        vxi16_t         a16_window;
        vxi16_t         a24_window;
        vxi16_t         a32_window;
        vxi16_t         dd5;
        vxi16_t         dd6;
        vxi16_t         dd7;
        vxi16_t         dd8;
        vxi16_t         dd9;
        vxi16_t         dd10;
        vxi16_t         subclass;
        vxi16_t         status;
	vxi16_t		lock;
	vxi16_t		irq;
	vxi16_t		read_la;
	vxi16_t		trigger_mode;
	vxi16_t		interrupt;
	vxi16_t		status_id;
	vxi16_t		trigger_config;
	vxi16_t		trigger_sync_ack;
	vxi16_t		trigger_async_ack;
	vxi16_t		irq_ack;
};

struct vxi_reg_dev_r{
        vxi16_t          ddx08;
        vxi16_t          ddxoa;
        vxi16_t          ddx0c;
        vxi16_t          ddx0e;
        vxi16_t          ddx10;
        vxi16_t          ddx12;
        vxi16_t          ddx14;
        vxi16_t          ddx16;
        vxi16_t          ddx18;
        vxi16_t          ddx1a;
        vxi16_t          ddx1c;
        vxi16_t          ddx1e;
        vxi16_t          ddx20;
        vxi16_t          ddx22;
        vxi16_t          ddx24;
        vxi16_t          ddx26;
        vxi16_t          ddx28;
        vxi16_t          ddx2a;
        vxi16_t          ddx2c;
        vxi16_t          ddx2e;
};

struct vxi_reg_slot0_dev_r{
        vxi16_t          modid;
        vxi16_t          ddxoa;
        vxi16_t          ddx0c;
        vxi16_t          ddx0e;
        vxi16_t          ddx10;
        vxi16_t          ddx12;
        vxi16_t          ddx14;
        vxi16_t          ddx16;
        vxi16_t          ddx18;
        vxi16_t          ddx1a;
        vxi16_t          ddx1c;
        vxi16_t          ddx1e;
        vxi16_t          ddx20;
        vxi16_t          ddx22;
        vxi16_t          ddx24;
        vxi16_t          ddx26;
        vxi16_t          ddx28;
        vxi16_t          ddx2a;
        vxi16_t          ddx2c;
        vxi16_t          ddx2e;
};

struct vxi_csr_r{
	vxi16_t		make;
	vxi16_t		model;
	vxi16_t		status;
	vxi16_t		offset;
	union {
		struct vxi_msg_dev_r		msg;
		struct vxi_mxi_dev_r		mxi;
		struct vxi_reg_dev_r		reg;
		struct vxi_reg_slot0_dev_r	reg_s0;
	}dd;
};


struct vxi_csr{
	union {
		struct vxi_csr_w	w;
		struct vxi_csr_r	r;
	}dir;
};

typedef struct vxi_csr	VXICSR;

#define CSRRMEM(PSR,MEMBER)  	( ((VXICSR *)(PSR))->MEMBER )

/*
 * Reserved device types
 */
#define VXIMODELMASK(WD)	( (WD)&0xfff )
#define	VXIMODEL(PSR)	 	( (unsigned) VXIMODELMASK(CSRRMEM(PSR,dir.r.model)) )
#define	VXIMAKE(PSR)  		( (unsigned) 0xfff&CSRRMEM(PSR,dir.r.make) )
#define VXISLOT0MODELTEST(WD)	((WD)<0x100)
#define VXISLOT0MODEL(PCSR)	(VXISLOT0MODELTEST(VXIMODEL(PCSR)))

/*
 * vxi device classes
 */
#define VXI_MEMORY_DEVICE	0
#define VXI_EXTENDED_DEVICE	1
#define VXI_MESSAGE_DEVICE	2
#define VXI_REGISTER_DEVICE	3
#define	VXICLASS(PSR)		((unsigned)(0xc000&CSRRMEM(PSR,dir.r.make))>>14)
#define VXISUBCLASS(PSR)	((unsigned)CSRRMEM(PSR,dir.r.dd.mxi.subclass) )
#define VXIMXI(PSR)\
(VXICLASS(PSR)==VXI_EXTENDED_DEVICE ? VXISUBCLASS(PSR)==VXI_MXI_DEVICE : 0)
#define VXI_MXI_DEVICE		0xfffc
#define MXI_BASE_DEVICE		0xf
#define MXI_INTX_DEVICE		0xe
#define MXI_EXTENDED_TYPE(PSR)	((((unsigned)CSRRMEM(PSR,dir.r.status))>>10)&0xf)
#define MXIINTX(PSR)		(MXI_EXTENDED_TYPE(PSR) == MXI_INTX_DEVICE)	
#ifndef SRCepvxiLib
extern 
#endif
char	*vxi_device_class_names[]
#ifdef SRCepvxiLib
 	= {
		"memory",
		"extended",
		"message",
		"register"}
#endif
;

/*
 * A32/A24 address space 
 */
#define VXIADDRSPACE(PSR)	( (unsigned long) 3&(CSRRMEM(PSR,dir.r.make)>>12) )
#define VXIREQMEM(PSR)		( (unsigned long) 0xf&(CSRRMEM(PSR,dir.r.model)>>12) )
#define VXIA24MEMSIZE(M)	( (long) 1<<(23-(M)) )
#define VXIA32MEMSIZE(M)	( (long) 1<<(31-(M)) )
#define VXI_ADDR_EXT_A24	0
#define VXI_ADDR_EXT_A32	1
#define VXI_ADDR_EXT_NONE	3

/*
 *
 * VXI vendor codes
 *
 */
#define VXI_MAKE_HP	0xfff	/* Hewlett Packard */
#define VXI_MAKE_RD	0xffb	/* Racal Dana */
#define VXI_MAKE_NI	0xff6	/* National Instruments */
#define VXI_MAKE_AT5	0xfa0	/* Los Alamos Natl Lab AT */
#define VXI_MAKE_KSC	0xf29	/* Kinetic Systems */
struct vxi_vendor_info{
	uint16_t	make;
	char		*pvendor_name;
};

/*
 * In decreasing order of vendor id
 */
#ifndef SRCepvxiLib 
        extern
#endif
struct vxi_vendor_info vxi_vi[]
#ifdef SRCepvxiLib
= {
	{VXI_MAKE_HP, 	"Hewlett-Packard"},
	{VXI_MAKE_RD, 	"Racal-Dana"},
	{VXI_MAKE_NI, 	"National Instruments"},
	{VXI_MAKE_AT5, 	"LANL AT5"},
	{VXI_MAKE_KSC, 	"Kinetic Systems"},
}
#endif
;


/*
 * MXI commands
 * (for the MXI control register)
 */
#define MXI_UPPER_LOWER_BOUNDS	0x4000
#define MXI_LA_WINDOW_SIZE_MASK	0x3
#define MXIA24MASK      (0xffff)
#define MXIA24MASKSIZE	16
#define MXIA32MASK      (0xffffff)
#define MXIA32MASKSIZE	24
#define MXIA24ALIGN(A)  (((unsigned long)(A)+MXIA24MASK)&~MXIA24MASK)
#define MXIA32ALIGN(A)  (((unsigned long)(A)+MXIA32MASK)&~MXIA32MASK)

/*
 * applies to all vxi devices
 */
#define VXIMEMENBLMASK		(0x8000)
#define VXIMEMENBL(PSR)		(VXIMEMENBLMASK&CSRRMEM(PSR,dir.r.status))
#define VXINDCDEVICESMASK	(0xff)
#define VXINDCDEVICES(PSR)	(VXINDCDEVICESMASK&CSRRMEM(PSR,dir.r.offset))

/*
 * Register based slot zero 
 */
#define VXIMODIDSTATUS(WD)	((WD)&0x4000?FALSE:TRUE)
#define VXIPASS		3
#define VXIPASSEDSTATUS(WD)	(((WD)>>2)&VXIPASS)


/*
 * RULE C.4.4
 *
 * The resource manager shall write all ones to the
 * device dependent bits when writing to the control register 
 *
 */
#define VXISAFECONTROL		0x7fff  /* sys fail inhibit, reset, no mem enbl  */
#define VXIMEMENBLCONTROL	0xfffc  /* mem enbl, no sys fail inhibit, no reset */

#define VXI_SET_REG_MODID(PCSR, SLOT) \
( ((VXICSR *)(PCSR))->dir.w.dd.reg_s0.modid =  \
	0x2000|1<<(SLOT) )
#define VXI_CLR_ALL_REG_MODID(PCSR) \
( ((VXICSR *)(PCSR))->dir.w.dd.reg_s0.modid =  0 )

/*
 * vxi msg based functions
 */
#define VXIWRITEREADYMASK	(0x0200)
#define VXIREADREADYMASK	(0x0400)
#define VXIDIRMASK		(0x1000)
#define VXIDORMASK		(0x2000)
#define VXIFHSMMASK		(0x0100)
#define VXIERRNOTMASK		(0x0800)

#define VXICMDRMASK		(0x8000)
#define VXICMDR(PSR)		(!(VXICMDRMASK&CSRRMEM(PSR,dir.r.dd.msg.protocol)))
#define VXIFHSMASK		(0x0800)
#define VXIFHS(PSR)		(!(VXIFHSMASK&CSRRMEM(PSR,dir.r.dd.msg.protocol)))
#define VXISHMMASK		(0x0400)
#define VXISHM(PSR)		(!(VXISHMMASK&CSRRMEM(PSR,dir.r.dd.msg.protocol)))
#define VXIMBINTMASK		(0x1000)
#define VXIMBINT(PSR)		(VXIMBINTMASK&CSRRMEM(PSR,dir.r.dd.msg.protocol))
#define VXIVMEBMMASK		(0x2000)
#define VXIVMEBM(PSR)		(!(VXIVMEBMMASK&CSRRMEM(PSR,dir.r.dd.msg.protocol)))
#define VXISIGREGMASK		(0x4000)
#define VXISIGREG(PSR)		(!(VXISIGREGMASK&CSRRMEM(PSR,dir.r.dd.msg.protocol)))

/*
 *	serial protocol errors
 */
#define MBE_NO_ERROR		(0xff)
#define MBE_MULTIPLE_QUERIES	(0xfd)
#define MBE_UNSUPPORTED_CMD	(0xfc)
#define MBE_DIR_VIOLATION	(0xfb)
#define MBE_DOR_VIOLATION	(0xfa)
#define MBE_RR_VIOLATION	(0xf9)
#define MBE_WR_VIOLATION	(0xf8)

/*
 * vxi message based commands
 */
#define MBC_BEGIN_NORMAL_OPERATION	0xfcff
#define MBC_TOP_LEVEL_CMDR		0x0100
#define MBC_READ_SERVANT_AREA		0xceff
#define MBC_BA				0xbc00
#define MBC_BR				0xdeff
#define MBC_END				0x0100
#define MBC_READ_PROTOCOL		0xdfff
#define MBC_ASSIGN_INTERRUPTER_LINE	0xaa00
#define MBC_READ_INTERRUPTERS		0xcaff
#define MBC_READ_PROTOCOL_ERROR		0xcdff
#define MBC_GRANT_DEVICE		0xbf00
#define MBC_IDENTIFY_COMMANDER		0xbe00
#define MBC_CLEAR			0xffff

/*
 * async mode control commands
 */
#define MBC_ASYNC_MODE_CONTROL		0xa800

#define MBC_AMC_RESP_ENABLE		0x0000
#define MBC_AMC_RESP_DISABLE		0x0008
#define MBC_AMC_EVENT_ENABLE		0x0000
#define MBC_AMC_EVENT_DISABLE		0x0004

#define MBC_AMC_RESP_INT_ENABLE		0x0000
#define MBC_AMC_RESP_SIGNAL_ENABLE	0x0002
#define MBC_AMC_EVENT_INT_ENABLE	0x0000
#define MBC_AMC_EVENT_SIGNAL_ENABLE	0x0001

/*
 * control resp command
 * (enable all)
 */
#define MBC_CONTROL_RESPONSE		0x8fc0

/*
 * msg based responses
 */
#define MBR_STATUS(RESP)		(((RESP)>>12) & 0xf)
#define MBR_STATUS_SUCCESS		0xf
#define MBR_READ_SERVANT_AREA_MASK	0x00ff
#define MBR_CR_CONFIRM_MASK		0x007f

/*
 * read interrupters response
 */
#define MBR_READ_INTERRUPTERS_MASK	0x7

/*
 * async mode control resp
 */
#define MBR_AMC_CONFIRM_MASK		0xf

/*
 * begin normal operation message based responses
 */
#define MBR_BNO_STATE(RESP)		(((RESP)>>8) & 0xf)
#define MBR_BNO_STATE_NO		0xf

/*
 * read protocol message based responses
 */
#define MBR_RP_LW(RESP)			(!((RESP)&0x1))
#define MBR_RP_ELW(RESP)		(!((RESP)&0x2))
#define MBR_RP_I(RESP)			(!((RESP)&0x4))
#define MBR_RP_I4(RESP)			(!((RESP)&0x8))
#define MBR_RP_TRG(RESP)		(!((RESP)&0x10))
#define MBR_RP_PH(RESP)			(!((RESP)&0x20))
#define MBR_RP_PI(RESP)			(!((RESP)&0x40))
#define MBR_RP_EG(RESP)			(!((RESP)&0x100))
#define MBR_RP_RG(RESP)			(!((RESP)&0x200))
#define MBR_REV_12(RESP)		(!((RESP)&0x8000))

/*
 * protocol events
 * vxi spec E.4
 */
#define MBE_EVENT_TEST(EVENT)		((EVENT)&0x1000)

/*
 * vxi trigger constants
 */
#define VXI_N_TTL_TRIGGERS 	8
#define VXI_N_ECL_TRIGGERS 	6
#define MXI_ECL0_ENABLE		0x0800
#define MXI_ECL0_FP_TO_BP	0x0400	
#define MXI_ECL0_BP_TO_FP	0
#define MXI_ECL1_ENABLE		0x2000
#define MXI_ECL1_FP_TO_BP	0x1000	
#define MXI_ECL1_BP_TO_FP	0

#endif /* INCvxih */
