/* 	drvEpvxi.h */
/* 	$Id$ */
/*
 *	parameter file supporting the VXI library
 *
 *      Author:      Jeff Hill
 *      Date:        8-91
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
 * .01 070792 joh 	Added MACROS to return the A24 and A32 base addr
 * .02 072992 joh 	Added  sccs id
 * .03 081992 joh 	ANSI C func proto
 * .04 082592 joh	added arg to epvxiRead() and epvxiWrite()	
 * .05 090392 joh	Now runtime links to NI trigger routing	
 * .06 092392 joh	New status from epvxiRead() - VXI_BUFFER_FULL
 * .07 033193 joh     	error codes converted to EPICS standard format
 * .08 071293 joh	record task id when opening a device	
 * .09 082793 joh	-Wall cleanup and added drvEpvxiFetchPConfig()
 *
 *
 */


#ifndef INCepvxiLibh
#define INCepvxiLibh

static char *epvxiLibhSccId = "$Id$";

#include <ellLib.h>
#include <epvxi.h>
#include <errMdef.h>

/*
 * Structure used to specify search patterns for epvxiLookupLA()
 *
 * Set a bit in the flags member for each item that is to be a
 * constraint on the search. All other items are ignored during
 * the search. The callback routine will be called for all devices
 * which exactly match all items specified in the flags field
 */
#define VXI_DSP_make          (1<<0)
#define VXI_DSP_model         (1<<1)
#define VXI_DSP_class         (1<<2)
#define VXI_DSP_slot          (1<<3)
#define VXI_DSP_slot_zero_la  (1<<4)
#define VXI_DSP_commander_la  (1<<5)
#define VXI_DSP_extender_la   (1<<6)	/* id crates that have one */

typedef struct {
 	long      	flags;         /* one bit enabling each field */
	unsigned short 	make;          /* manufacture identification */
	unsigned short 	model;         /* model code for the device  */
	unsigned short 	class;         /* vxi device class      */
	unsigned char  	slot;          /* slot where the device resides */
	unsigned char  	slot_zero_la;  /* logical address of slot 0 dev */
	unsigned char  	commander_la;  /* logical address of commander */
	unsigned char  	extender_la;   /* logical address of bus repeater */
}epvxiDeviceSearchPattern;

typedef long	EPVXISTAT;

/*
 * functions from epvxiLib.c
 */
EPVXISTAT	epvxiResman(
	void
);

EPVXISTAT 	epvxiIOReport(
	unsigned 		level
);

EPVXISTAT	epvxiDeviceList(
	void
);

EPVXISTAT	epvxiCrateList(
	void
);

EPVXISTAT	epvxiExtenderList(
	void
);

EPVXISTAT	epvxiUniqueDriverID(
	void
);

EPVXISTAT 	epvxiDeviceVerify(
	unsigned		la
);

EPVXISTAT	epvxiOpen(
	unsigned		la,
	int			vxiDriverID,
	unsigned long		driverConfigSize,
	void			(*pio_report_func)()
);

EPVXISTAT	epvxiClose(
	unsigned		la,
	int			vxiDriverID
);

EPVXISTAT	epvxiLookupLA(
	epvxiDeviceSearchPattern
			        *pdsp,
	void			(*pfunc)(),
	void			*parg 
);

EPVXISTAT	epvxiRouteTriggerECL(
	unsigned	la,		/* slot zero device logical address     */
	unsigned       	enable_map,	/* bits 0-5  correspond to trig 0-5     */
					/* a 1 enables a trigger                */
					/* a 0 disables a trigger               */
	unsigned       	io_map		/* bits 0-5  correspond to trig 0-5     */
					/* a 1 sources the front panel          */
					/* a 0 sources the back plane		*/
);

EPVXISTAT	epvxiRouteTriggerTTL(
	unsigned        la,		/* slot zero device logical address     */
	unsigned        enable_map,	/* bits 0-5  correspond to trig 0-5     */
					/* a 1 enables a trigger                */
					/* a 0 disables a trigger               */
	unsigned        io_map		/* bits 0-5  correspond to trig 0-5     */
					/* a 1 sources the front panel          */
					/* a 0 sources the back plane		*/
);

EPVXISTAT	epvxiRegisterModelName(
	unsigned int 	make,
	unsigned int 	model,
	char 		*pmodel_name
); 

EPVXISTAT	epvxiRegisterMakeName(
	unsigned int 	make,
	char 		*pmake_name
);

EPVXISTAT	epuxiLookupModelName(
	unsigned int    make,           /* VXI manuf. */
	unsigned int    model,          /* VXI model code */
	char            *pbuffer,       /* model name return */
	unsigned int    bufsize,        /* size of supplied buf */
	unsigned int    *preadcount	/* n bytes written */
);

EPVXISTAT	epuxiLookupMakeName(
	unsigned int    make,           /* VXI manuf. */
	char            *pbuffer,       /* model name return */
	unsigned int    bufsize,        /* size of supplied buf */
	unsigned int    *preadcount	/* n bytes written */
);

EPVXISTAT	vxi_init(		/*  compatibility */
	void
);

EPVXISTAT	vxi_io_report(		/*  compatibility */
	unsigned	level
);

/* 
 * functions from epvxiMsgLib.c
 */
EPVXISTAT	epvxiCmd(
	unsigned        la,
	unsigned long   cmd
);
EPVXISTAT	epvxiQuery(
	unsigned        la,
	unsigned long   *presp
);
EPVXISTAT 	epvxiCmdQuery(
	unsigned        la,
	unsigned long   cmd,
	unsigned long   *presp
);
EPVXISTAT	epvxiRead(
	unsigned        la,
	char            *pbuf,
	unsigned long   count,
	unsigned long   *pread_count,
	unsigned long	option
);
#define epvxiReadOptNone	0

EPVXISTAT	epvxiWrite(
	unsigned        la,
	char            *pbuf,
	unsigned long   count,
	unsigned long   *pwrite_count,
	unsigned long	option
);
#define epvxiWriteOptNone	0
#define epvxiWriteOptPartialMsg	1  /* message continues after this transfer */

EPVXISTAT	epvxiSetTimeout(
	unsigned        la,
	unsigned long   timeout
);

/*
 * epvxiLib return codes (also used by epvxiMsgLib)
 *
 * These codes changed to the EPICS standrd format on 033193
 */
#define VXI_SUCCESS 0 /* normal successful completion*/
#define S_epvxi_noDevice (M_epvxi|1) /*device does not exist*/
#define S_epvxi_notSlotZero (M_epvxi|2) /*not a slot zero devic*/
#define S_epvxi_uknDevice (M_epvxi|3) /*device not supported*/
#define S_epvxi_badTrigger (M_epvxi|4) /*specified trigger does not exist*/
#define S_epvxi_badTrigIO (M_epvxi|5) /*specified trigger io does not exist*/
#define S_epvxi_deviceOpen (M_epvxi|6) /*device already open*/
#define S_epvxi_notOwner (M_epvxi|7) /*device in use by a different driver*/
#define S_epvxi_noMemory (M_epvxi|8) /*memory allocation failed*/
#define S_epvxi_notOpen (M_epvxi|9) /*device not open*/
#define S_epvxi_notMsgDevice (M_epvxi|10) /*operation requires a message based device*/
#define S_epvxi_deviceTMO (M_epvxi|11) /*message based dev timed out*/
#define S_epvxi_msgDeviceFailure (M_epvxi|12) /*message based dev failed*/
#define S_epvxi_badLA (M_epvxi|13) /*logical addr out of range*/
#define S_epvxi_multipleQueries (M_epvxi|14) /*multiple queries serial protocol error*/
#define S_epvxi_unsupportedCmd (M_epvxi|15) /*unsupported cmd serial protocol error*/
#define S_epvxi_dirViolation (M_epvxi|16) /*DIR violation serial protocol error*/
#define S_epvxi_dorViolation (M_epvxi|17) /*DOR violation serial protocol error*/
#define S_epvxi_rrViolation (M_epvxi|18) /*RR violation serial protocol error*/
#define S_epvxi_wrViolation (M_epvxi|19) /*WR violation serial protocol error*/
#define S_epvxi_errFetchFailed (M_epvxi|20) /*unknown serial protocol error*/
#define S_epvxi_selfTestFailed (M_epvxi|21) /*self test failed*/
#define S_epvxi_timeoutToLarge (M_epvxi|22) /*specified timeout to long*/
#define S_epvxi_protocolError (M_epvxi|23) /*protocol error*/
#define S_epvxi_unreadData (M_epvxi|24) /*attempt to write when unread data from a previous command is present (RULE C.3.3)*/
#define S_epvxi_nameMismatch (M_epvxi|25) /*make or model name already registered does not matchi supplied name*/
#define S_epvxi_noMatch (M_epvxi|26) /*no name registered for the supplied make and or model*/
#define S_epvxi_bufferFull (M_epvxi|27) /*read terminated with unread data remaining because the end of the supplied  buffer was reached*/
#define S_epvxi_noResman (M_epvxi|28) /*the VXI resource manager must run first*/
#define S_epvxi_internal (M_epvxi|29) /*VXI internal failure*/
#define S_epvxi_badConfig (M_epvxi|30) /*Incorrect system configuration*/
#define S_epvxi_noCmdr (M_epvxi|31) /*No commander hardware support for message based comm - continuing*/
#define S_epvxi_msgDeviceStatus (M_epvxi|32) /*VXI Message based device reporting error condition*/
#define S_epvxi_slotNotFound (M_epvxi|33) /*VXI device's slot not found- MODID failure?*/
#define S_epvxi_noMODID (M_epvxi|34) /*VXI device does not have MODID capability*/


enum ext_type { ext_local_cpu, /* bus master constrained by module_types.h */
                ext_export_vxi_onto_mxi,  /* VXI mapped into MXI addr space */
                ext_import_mxi_into_vxi  /* MXI mapped into VXI addr space */
                /*
                .
                .
                other bus extender types  could be inserted here
                .
                .
                */
                };

#ifndef SRCepvxiLib 
extern
#endif
char    *ext_type_name[]
#ifdef SRCepvxiLib
	 = {
                "VXI hosted VME bus master",
                "VXI mapped onto MXI addr space",
                "MXI mapped into VXI addr space"
         }
#endif
;

typedef struct extender_device{
        ELLNODE            	node;
        ELLLIST            	extenders;      /* sub extenders */
	struct extender_device	*pParent;
        enum ext_type   	type;
        int             	la;
        int             	la_low;         /* inclusive */
        int             	la_high;        /* inclusive */
        unsigned long		A24_base;
        unsigned long   	A24_size;
        unsigned long		A32_base;
        unsigned long   	A32_size;
        unsigned        	la_mapped:1;	/* device present */
        unsigned        	A24_mapped:1;
        unsigned        	A32_mapped:1;
	unsigned		A24_ok:1;
	unsigned		A32_ok:1;
}VXIE;

/*
 * bits for the device type and a pointer
 * to its configuration registers if
 * available 
 */
typedef struct slot_zero_device{
	ELLNODE		node;
	void            (*set_modid)(
				struct slot_zero_device *pvxisz, 
				unsigned slot);
	void            (*clear_modid)(
				struct slot_zero_device *pvxisz);
	VXIE            *pvxie;
	struct vxi_csr  *pcsr;
	unsigned char	la;
	unsigned	reg:1;
	unsigned	msg:1;
	unsigned	nicpu030:1;
}VXISZ;

typedef struct epvxiLibDeviceConfig{
        void    	(*pio_report_func)();   /* ptr to io report func */
	void		*pDriverConfig;		/* ptr to driver config	*/
	void		*pMsgConfig;		/* msg driver config area */
	void		*pFatAddrBase;
	VXIE		*pvxie;			/* extender info */
	VXIE		*pvxieSelf;		/* extender info for self */
	VXISZ		*pvxisz;		/* ptr to slot zero info */
	unsigned long	driverID;		/* unique driver id	*/
	int		taskID;			/* opened by this id	*/	
        unsigned short  make;
        unsigned short  model;
        short		slot;
        short		commander_la;
	short		extender_la;   /* logical address of bus repeater */
	short		slot_zero_la; 
        unsigned char   class;
	unsigned	st_passed:1;		/* self test passed	*/
        unsigned        msg_dev_online:1;
        unsigned        slot0_dev:1;
	unsigned	A24_mapped:1;
	unsigned	A32_mapped:1;
}VXIDI;

/*
 *
 *
 *      functions used from the nivxi library
 *	(to support runtime linking)
 *
 */
enum    nivxi_func_index {
                e_SetMODID,
                e_VXIinLR,
                e_InitVXIlibrary,
                e_vxiinit,
                e_MapTrigToTrig,
                e_GetMyLA,
                e_EnableSignalInt,
                e_SetSignalHandler,
		e_RouteSignal
	};
#ifndef SRCepvxiLib 
extern
#endif
char    *nivxi_func_names[] 
#ifdef SRCepvxiLib 
	= {
                "_SetMODID",
                "_VXIinLR",
                "_InitVXIlibrary",
                "_vxiinit", /* WARNING this is different than vxiInit */
                "_MapTrigToTrig",
                "_GetMyLA",
                "_EnableSignalInt",
                "_SetSignalHandler",
		"_RouteSignal"
        }
#endif
;

#ifdef SRCepvxiLib 
int     (*pnivxi_func[NELEMENTS(nivxi_func_names)])();
#else
extern
int     (*pnivxi_func[])();
#endif


/*
 *
 * typical usage
 * 
 * struct freds_driver_info pfdi;
 *
 * pfdi = epvxiPConfig(14, driverID, (struct freds_driver_info *))
 *	
 * RETURNS ptr to device config area or NULL
 */
#define PVXIDI(LA) (epvxiLibDeviceList[LA])
#define epvxiPConfig(LA, ID, CAST) \
( \
PVXIDI(LA)? \
	PVXIDI(LA)->driverID==(ID)? \
		(CAST) PVXIDI(LA)->pDriverConfig \
	: \
		(CAST) NULL \
: \
	(CAST) NULL \
)

/*
 * epvxiFetchPConfig
 * (improved version of the above returns status)
 */
#define epvxiFetchPConfig(LA, ID, PTR) \
( \
PVXIDI(LA)!=NULL? \
	(PVXIDI(LA)->driverID==(ID)? \
		(((PTR) = PVXIDI(LA)->pDriverConfig), VXI_SUCCESS) \
	: \
		S_epvxi_notOwner) \
: \
	S_epvxi_noDevice \
)

/*
 * A24 and A32 base addressing
 */
#define epvxiA24Base(LA) (PVXIDI(LA)->pFatAddrBase)
#define epvxiA32Base(LA) (PVXIDI(LA)->pFatAddrBase)

#ifndef SRCepvxiLib 
extern
#endif
VXIDI   *epvxiLibDeviceList[NVXIADDR];

#define NO_DRIVER_ATTACHED_ID	(0)	
#define UNINITIALIZED_DRIVER_ID	(0xffff)	
#ifndef SRCepvxiLib 
extern
#endif
unsigned long	epvxiNextDriverID
#ifdef SRCepvxiLib
= (NO_DRIVER_ATTACHED_ID+1)
#endif
;

/*
 * set by the RM when it is done and VXI modules are present
 */
#ifndef SRCepvxiLib 
extern
#endif
int		epvxiResourceMangerOK;

#ifndef SRCepvxiLib 
extern
#endif
void    *epvxi_local_base;

#define VXIBASE(LA) VXI_LA_TO_PA(LA, epvxi_local_base)

#endif /* INCepvxiLibh */
