/*
 *	drvEpvxi.c
 *
 *	base/src/drv $Id$
 *  	Routines for the VXI device support and resource management.
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
 *	.01 joh 02-14-90	formal release
 *	.02 joh 04-04-90	as requested KLUDGED dynamic address so they
 *				dont have to specify model number in DCT
 *	.03 joh 04-04-90	as requested KLUDGED dynamic address so they
 *				dont have to specify model number in DCT
 *	.04 joh 07-26-90	changed from ttl trig 7 to ecl trig 0
 *	.05 joh 07-27-90	added support for multiple slot0 cards
 *	.06 joh 08-08-91	delinting
 *	.07 joh 09-05-91	converted to v5 vxWorks
 *	.08 joh 12-05-91	split vxi_driver.c into vxi_driver.c and 
 *				vxi_resman.c
 *	.09 joh 01-29-91	added MXI support & removed KLUDGE
 *	.10 joh 07-06-92	added A24 & A32 address config
 *	.11 joh 07-07-92	added routine to return A24 or A32 base
 *	.12 joh 07-13-92	merged in model hash support written by
 *				Richard Baker (summer intern)
 *	.13 joh 07-21-92	Now stores extender info in a hierarchical
 *				linked list
 *	.14 joh 07-29-92	vxi record topology needed check for
 *				device present
 *	.15 joh 07-29-92	added sccs id
 *	.16 joh 08-19-92	make name registration
 *	.17 joh 08-21-92	cleaned up A24/A32 MXI setup	
 *	.18 joh 08-26-92	dont return error if a make or model
 *				has already been registered
 *	.19 joh	09-03-92	Use the correct routine in NIVXI
 *				for CPU030 trigger routing
 *	.20 joh	09-30-92	split epvxiOpen() into epvxiOpen() and
 *				epvxiDeviceVerify()
 *	.21 joh	10-30-92	NI CPU030 trigger routing was failing
 *				due to no entry for the 030 in the resman 
 *				tables - it cant see itself in A16.
 *				A work around was installed.
 *	.22 joh 05-24-93	Fixed over-zealous parameter checks in
 *				TTL trigger route
 *      .23 joh 06-03-93        Fixed incorrect MXI BP TTL trigger enable
 *      .24 joh 07-12-93       	Record the task id when opening a device 
 *	.25 joh 07-21-93	Improved DC device allocation in MXI
 *				environment
 *	.26 joh 11-10-93	Now configures multiple DC devices per slot.
 *				Blocked address devices are preallocated
 *				where possible. Independently addressed
 *				multiple devices per slot are allocated on 
 *				demand.
 *	.27 joh 02-06-95	Force natural alignment of blocked address
 *				DC devices.
 *
 * RM unfinished items	
 * -------------------
 *	1. Assigning the cmdr/serv hierarchy from within a DC res man
 *	   needs to be revisited
 *	2. Should this module prevent two triggers from driving
 *	   the same front panel connector at once?
 *
 *
 * NOTES 
 * -----
 *
 *
 */

/*
 * 	Code Portions
 *
 * local
 *	vxi_find_slot		given a VXI modules addr find its slot
 *	vxi_init_ignore_list	init list of interrupt handlers to ignore
 *	vxi_vec_inuse		check to see if vector is in use
 *	vxi_configure_hierarchies	setup commander servant hierarchies
 *	vxi_self_test		test for dev self test passed
 *	open_slot0_device	open slot zero devices
 *	nicpu030_init		NI CPU030 controller setup
 *	nivxi_cpu030_set_modid	set modid on the NICPU030
 *	nivxi_cpu030_clr_all_modid 	clear all modid lines on the NICPU030
 *	set_reg_modid		set modid on a reg based slot0 device	
 *	clr_all_reg_modid	clr all modid on a reg based slot0 dev
 *	vxi_find_sc_devices	find all SC devices and open them
 *	vxi_find_dc_devices	find all DC devices and open them
 *	vxi_count_dc_devices	determine the number of DC devices in
 *				this extender
 *	vxi_init_ignore_list	find addresses of default int handlers
 *	vxi_vec_inuse		test for int vector in use
 *	mxi_map			mat the addresses on a MXI bus extender
 *	vxi_find_mxi_devices	search for and open mxi bus repeaters
 *	map_mxi_inward		map from a VXI crate towards the RM
 *	vxi_address_config	setup A24 and A32 offsets
 *	open_vxi_device		log VXI device info
 *	vxi_record_topology	find slots and extenders for each device
 *
 * for use by IOC core
 *	epvxiResman		entry for a VXI resource manager which
 *				also sets up the MXI bus
 *	epvxiIOReport		call io device specific report routines 
 *				for all registered devices and print 
 *				information about device's configuration
 *	epvxiDeviceList		print info useful when debugging drivers
 *	vxi_init		backwards compatibility
 *	vxi_io_report		backwards compatibility
 *
 * for use by vxi drivers
 *	epvxiLookupLA		find LA given search pattern
 *	epvxiUniqueDriverID	obtain a unique id each call
 * 	epvxiOpen			register a drivers use of a device
 *	epvxiClose		disconnect from a device	
 *	epvxiPConfig		fetch a driver config block given a LA
 *	epvxiRouteTriggerECL	route ECL trig to/from front panel
 *	epvxiRouteTriggerTTL	route TTL trig to/from front panel
 *
 */

static char *sccsId = "$Id$\t$Date$";

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <vxWorks.h>
#include <vme.h>
#include <iv.h>
#include <vxLib.h>
#include <sysSymTbl.h>
#include <taskLib.h>
#include <sysLib.h>
#include <logLib.h>
#include <intLib.h>
#include <tickLib.h>

#include <devLib.h>
#define SRCepvxiLib	/* allocate externals here */
#include <drvEpvxi.h>
#include <drvHp1404a.h>

#define NICPU030

/*
 * required to use private assert() here
 * because they compile this gcc -traditional
 */
#define assert(EXP) ((EXP)?1: (errMessage (S_epvxi_internal, NULL) , 0) )

/*
 * EPICS driver entry point table
 */
typedef long (*DRVSUPFUN) ();
struct {
        long    	number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvVxi={
        2,
        epvxiIOReport,
        epvxiResman};


/*
 * so setting ECL triggers does not mess with the
 * RM's address windows
 */
#define MXI_CONTROL_CONSTANT	0x4000
#define INTX_INT_OUT_ENABLE	0x7f00
#define INTX_INT_IN_ENABLE	0x7f7f

#define abort(A) taskSuspend(0)

#define VXIMSGINTLEVEL  1

#define BELL 7

#define UKN_LA (-1)
#define UKN_SLOT (-1)
#define UKN_CRATE (-1)

#define DEFAULT_VXI_A24_BASE	0x90000
#define DEFAULT_VXI_A24_SIZE	0x10000
#define DEFAULT_VXI_A32_BASE	0x90000000
#define DEFAULT_VXI_A32_SIZE	0x10000000

/*
 * laPassLast and laPassFirst must be last/first respectively
 * and have no purpose outside of being delimiters
 */
enum laPass {
	laPassSC, 
	laPassAnchoredDC, 
	laPassFloatingDC,
	laPassSetWindows};


LOCAL char	niCpu030Initialized;
LOCAL VXIE	root_extender;
LOCAL ELLLIST	crateList;

LOCAL char 		*ignore_list[] = {"_excStub","_excIntStub"};
LOCAL void 		*ignore_addr_list[NELEMENTS(ignore_list)];
LOCAL unsigned 		last_la;
LOCAL unsigned 		first_la;

#define SETMODID(PVXISZ, SLOT) \
(*(PVXISZ)->set_modid)((PVXISZ), SLOT)

#define CLRMODID(PVXISZ) \
(*(PVXISZ)->clear_modid)(PVXISZ);

LOCAL
SYMTAB	*epvxiSymbolTable;
LOCAL
char	epvxiSymbolTableDeviceIdString[] = "%03x:%03x";
LOCAL
char	epvxiSymbolTableMakeIdString[] = "%03x";

/*
 * for the VXI symbol table
 * just contains model names for now
 */
#define EPVXI_MODEL_NAME_SYMBOL	1
#define EPVXI_MAKE_NAME_SYMBOL	2
#define EPVXI_MAX_SYMBOLS_LOG2	8
#define EPVXI_MAX_SYMBOLS  	(1<<EPVXI_MAX_SYMBOLS_LOG2) 
#define EPVXI_MAX_SYMBOL_LENGTH (sizeof(epvxiSymbolTableDeviceIdString)+10) 

/*
 * NIVXI trigger routing
 *
 */
#define TRIG_LINE_TTL_BASE         (0)
#define TRIG_LINE_ECL_BASE         (8)
#define TRIG_LINE_FPIN             (40)
#define TRIG_LINE_FPOUT            (41)

/* forward references */
LOCAL EPVXISTAT 	vxi_alloc_la(
	VXIE		*pvxie,
	unsigned	count,
	unsigned	alignment,	/* number of zero ls bits in LA */
	unsigned	*poffset
);
LOCAL EPVXISTAT 	vxi_find_slot(
	struct vxi_csr	*pcsr,
	unsigned  	*pslot,
	VXISZ		**ppvxisz
);
LOCAL EPVXISTAT 	vxi_self_test(
	void
);
LOCAL EPVXISTAT 	vxi_init_ignore_list(
	void
);
LOCAL EPVXISTAT 	vxi_vec_inuse(
	unsigned 	la
);
LOCAL EPVXISTAT 	mxi_map(
	VXIE		*pvxie,
	enum laPass	pass
);
LOCAL EPVXISTAT 	map_mxi_inward(
	VXIE		*pvxie,
	enum laPass	pass
);
LOCAL EPVXISTAT 	open_vxi_device(
	VXIE		*pvxie,
	unsigned	la
);
LOCAL EPVXISTAT 	verify_valid_window(
	VXIE		*pvxie,
	unsigned	la
);
LOCAL void vxi_begin_normal_operation(
	void
);
LOCAL void	vxi_allocate_int_lines(
	void
);
LOCAL EPVXISTAT 	epvxiSymbolTableInit(
	void
);
LOCAL EPVXISTAT	nicpu030_init(
	VXIE		*pvxie
);
LOCAL void	vxi_find_sc_devices(
	VXIE		*pvxie
);
LOCAL void	vxi_find_dc_devices(
	VXIE		*pvxie
);
LOCAL EPVXISTAT 	vxi_count_dc_devices(
	VXIE		*pvxie,
	unsigned	*pCount,
	unsigned	*pMaxInBlockedDC /* max LA in a blocked addr DC device */
);
LOCAL void	set_reg_modid(
	VXISZ		*pvxisz,
	unsigned	slot
);
LOCAL void	clr_all_reg_modid(
	VXISZ		*pvxisz
);
LOCAL void	nivxi_cpu030_set_modid(
	VXISZ		*pvxisz,
	unsigned        slot
);
LOCAL void	nivxi_cpu030_clr_all_modid(
	VXISZ		*pvxisz
);
LOCAL void	open_slot0_device(
	VXIE		*pvxie,
	unsigned	la 
);
LOCAL void	vxi_configure_hierarchies(
	unsigned	commander_la,
	unsigned	servant_area
);
LOCAL void	vxi_find_mxi_devices(
	VXIE		*pvxie,
	enum laPass	pass	
);
LOCAL void	vxi_unmap_mxi_devices(
	void
);
LOCAL void	vxi_address_config(
	void
);
LOCAL void 	vxi_record_topology(
	void
);
LOCAL void	epvxiExtenderPrint(
	VXIE *pvxie
);
LOCAL void	epvxiSelfTestDelay(
	void
);
LOCAL void	epvxiRegisterCommonMakeNames(
	void
);
LOCAL VXIE	*open_mxi_device(
	unsigned	la,
	VXIE		*pvxie,
	enum ext_type	type
);
LOCAL EPVXISTAT 	epvxiSetDeviceOffline(
	unsigned 	la
);
LOCAL void 	vxi_allocate_address_block(
	VXIE		*pvxie
);
LOCAL EPVXISTAT 	symbol_value_fetch(
	char 		*pname,
	void 		*pdest,
	unsigned 	dest_size
);
LOCAL EPVXISTAT 	report_one_device(
	unsigned	la,
	int		level 
);
LOCAL void 	mxi_io_report(
	struct vxi_csr	*pmxi,
	int 		level 
);
LOCAL 	EPVXISTAT	vxi_la_occupied(
	unsigned 	la
);
LOCAL unsigned blocked_la_count(
	struct vxi_csr *pcsr
);
LOCAL unsigned blocked_la_alignment(
	unsigned blockedLACount
);
LOCAL EPVXISTAT vxi_program_dc_addresses(
	VXIE		*pvxie,
	int		preallocm,
	unsigned	*pOffset,
	VXISZ		*pvxisz,
	int		slot,
	unsigned	minNumberDC
);

LOCAL void vxi_assign_dc_addresses(
	VXIE		*pvxie,
	int		prealloc,
	unsigned	*pOffset,
	VXISZ		*pvxisz,
	unsigned	minNumberDC
);


/*
 *
 * vxi_init()
 *
 * for compatibility with past releases
 *
 *
 */
EPVXISTAT vxi_init(void)
{
	return epvxiResman();
}


/*
 * epvxiResman() 
 *
 * perform vxi resource management functions
 *
 *
 */
EPVXISTAT epvxiResman(void)
{
        EPVXISTAT	status;
	unsigned 	EPICS_VXI_LA_COUNT;
	unsigned 	EPICS_VXI_LA_BASE;
	enum laPass	pass; 


	/*
	 * lookup the symbol for the number of devices
  	 * (if we are running under EPICS)
	 */
	{
		SYM_TYPE	type;
		unsigned char 	*pEPICS_VXI_LA_COUNT = 0;
		unsigned char 	*pEPICS_VXI_LA_BASE = 0;

		status = symFindByName(
				sysSymTbl,
				"_EPICS_VXI_LA_BASE",	
				(char **)&pEPICS_VXI_LA_BASE,
				&type);
		if(status == OK){
			EPICS_VXI_LA_BASE = 
				min(	*pEPICS_VXI_LA_BASE,
					VXIDYNAMICADDR-1);

		}
		else{
			EPICS_VXI_LA_BASE = 0;
		}

		status = symFindByName(
				sysSymTbl,
				"_EPICS_VXI_LA_COUNT",	
				(char **)&pEPICS_VXI_LA_COUNT,
				&type);
		if(status == OK){
			EPICS_VXI_LA_COUNT = 
				min(	*pEPICS_VXI_LA_COUNT,
					VXIDYNAMICADDR-EPICS_VXI_LA_BASE);

		}
		else{
			EPICS_VXI_LA_COUNT = 
				VXIDYNAMICADDR-EPICS_VXI_LA_BASE;
		}
	}

	/*
	 * If VXI devices are assigned an empty address block
	 * then ignore VXI 
	 */
	if(EPICS_VXI_LA_COUNT == 0){
		return VXI_SUCCESS;
	}

	/* 
	 * find out where the VXI LA space is on this processor
 	 * and register the VXI allocation
	 */
	status = sysBusToLocalAdrs(
				VME_AM_SUP_SHORT_IO,
				(char *)VXIBASEADDR,
				(char **)&epvxi_local_base);
	if(status != OK){
		return S_epvxi_badConfig;
	}
#if 0
	/*
	 * 
	 * not the same as epvxi_local_base
	 */
	status = devRegisterAddress(
			"VXI resman",
			atVMEA16,
			VXI_LA_TO_PA(EPICS_VXI_LA_BASE, VXIBASEADDR),
			VXIDEVSIZE*EPICS_VXI_LA_COUNT,
			&#####);
	if(status){
		return status;
	}
	/*
	 * Register space to communicate with VXI DC devices
	 */
	status = devRegisterAddress(
			"VXI resman",
			atVMEA16,
			VXI_LA_TO_PA(VXIDYNAMICADDR, VXIBASEADDR),
			VXIDEVSIZE,
			NULL);
	if(status){
		return status;
	}
#endif

        /*
         * clip the logical address range
         */
        last_la = min(  VXIDYNAMICADDR-1,
                        EPICS_VXI_LA_BASE+EPICS_VXI_LA_COUNT-1);
        last_la = min(  last_la,
                        NELEMENTS(epvxiLibDeviceList)-1);
        first_la = min(EPICS_VXI_LA_BASE,VXIDYNAMICADDR-1);

        /*
         * setup the root extender -
         * in this case just the local CPU which can
         * see the entire VXI address space because
         * it is a VME bus master
         *
         * some constraints are fetched from module_types.h
         */
        root_extender.la = VXI_RESMAN_LA;
        root_extender.type = ext_local_cpu;
        root_extender.la_low = last_la;
        root_extender.la_high = first_la;

        /*
         *
	 * register some common manufacturer names
	 * for consistency
	 *
 	 */
	epvxiRegisterCommonMakeNames();

	/*
	 * wait the obligatory 5 sec prior to first
 	 * A16 VXI access 
	 */
	epvxiSelfTestDelay();

	/*
	 * init the NI CPU030 hardware first so that we
	 * dont end up with a bus error on the first
	 * VME access
	 */
#	ifdef NICPU030
		status = nicpu030_init(&root_extender);
		if(status){
			return status;
		}
#	endif

	/*
	 * find default interrupt handlers
	 */
	status = vxi_init_ignore_list();
        if(status){
                return status;
	}

	/*
	 * close any MXI devices which have open windows but
	 * have not been encountered by this execution
	 * of the resource manager.
	 *
	 * This makes the MXI/VXI configure correctly after
	 * a control x (soft) reboot.
	 */
	vxi_unmap_mxi_devices();

	/*
	 * locate SC and DC devices
	 * setup extender LA windows
	 */
	for(	pass = laPassSC; 
		pass <= laPassSetWindows;
		pass++){

	        status = mxi_map(&root_extender, pass);
		if(status){
			return status;
		}
	}

	/*
	 * nothing out there so just quit
	 */
	if(!root_extender.la_mapped){
		return VXI_SUCCESS;
	}

	/*
 	 * find slot, crate, and extender for each device
	 */
	vxi_record_topology();

	/*
 	 * hpE1404 init
 	 * (turns on self test passed bit) 
	 */
	hpE1404Init();
	
        status = vxi_self_test();
	if(status){
		return status;
	}

	vxi_address_config();

	vxi_configure_hierarchies(
		VXI_RESMAN_LA, 
		last_la);

	vxi_allocate_int_lines();

	vxi_begin_normal_operation();

	/*
	 * let the world know that the RM is done
	 * and VXI devices were found
	 */
	epvxiResourceMangerOK = TRUE;

        return VXI_SUCCESS;
}


/*
 * wait the obligatory 5 sec prior to first
 * A16 VXI access 
 */
LOCAL void epvxiSelfTestDelay(void)
{
	/*
 	 * waits 6 sec to be sure
	 */
	int 	delay;
	int 	current;

	delay = sysClkRateGet()*6;
	current = tickGet();

	if(current < delay){
		printf("VXI Res Man: Waiting for self tests to complete ...");
		taskDelay(delay - current);
		printf("\n");
	} 
}



/*
 *
 *
 * 	mxi_map()
 *
 *
 */
LOCAL EPVXISTAT mxi_map(
VXIE		*pvxie,
enum laPass	pass
)
{
	switch(pass)
	{
	case laPassSC:
		/*
 		 * find the SC devices
		 */
		vxi_find_sc_devices(pvxie);
		break;

	/* 
	 * allocate DC devices
	 */
	case laPassFloatingDC:
		/*
		 * already got a chance on 
		 * laPassAnchoredDC if
		 * something is mapped
		 */
		if(pvxie->la_mapped){
			break;
		}
		vxi_find_dc_devices(pvxie);
		break;


	/* 
	 * allocate DC devices
	 */
	case laPassAnchoredDC:
		/*
		 * wait until laPassFloatingDC if
		 * nothing is mapped
		 */
		if(!pvxie->la_mapped){
			break;
		}
		vxi_find_dc_devices(pvxie);
		break;

	default:
		break;
	}

	/*
	 *
	 * find any MXI bus repeaters
	 *
	 */
	vxi_find_mxi_devices(
		pvxie, 
		pass);

	return VXI_SUCCESS;
}


/*
 *
 * vxi_unmap_mxi_devices()
 *
 * close any MXI devices which have open windows but
 * have not been encountered by this execution
 * of the resource manager.
 *
 * This makes the MXI/VXI configure correctly after
 * a control x (soft) reboot.
 */
LOCAL void vxi_unmap_mxi_devices(void)
{
	struct vxi_csr	*pmxi;
	EPVXISTAT	status;
	int16_t		id;
	unsigned	addr;

        for(addr=first_la; addr<=last_la; addr++){
                /*
                 * only configure devices not seen before
                 */
                if(epvxiLibDeviceList[addr]){
                        continue;
                }

		pmxi = VXIBASE(addr);

  		status = vxMemProbe(	(char *)pmxi,
					READ,
					sizeof(id),
					(char *)&id);
  		if(status<0){
			continue;
		}

		if(!VXIMXI(pmxi)){
			continue;
		}

		/*
		 * force all of these back to the hard reset state
		 */
		pmxi->dir.w.dd.mxi.la_window = 0; 
		pmxi->dir.w.dd.mxi.a16_window_low = 0; 
		pmxi->dir.w.dd.mxi.a16_window_high = 0; 
		pmxi->dir.w.dd.mxi.a24_window_low = 0; 
		pmxi->dir.w.dd.mxi.a24_window_high = 0; 
		pmxi->dir.w.dd.mxi.a32_window_low = 0; 
		pmxi->dir.w.dd.mxi.a32_window_high = 0; 
	}
}



/*
 *
 * vxi_find_mxi_devices()
 *
 */
LOCAL void vxi_find_mxi_devices(
VXIE		*pvxie,
enum laPass	pass
)
{
	struct vxi_csr	*pmxi;
	unsigned	addr;
	VXIDI		*pvxidi;
	VXIE		*pnewvxie;

        for(	addr=first_la; addr<=last_la; addr++){

		pvxidi = epvxiLibDeviceList[addr];

                /*
                 * only configure devices seen before
                 */
                if(!pvxidi){
                        continue;
                }

		/*
		 * skip MXI devices which are not 
 		 * in this extender
 		 */
                if(pvxidi->pvxie != pvxie){
			continue;
		}

		pmxi = VXIBASE(addr);

		pnewvxie = open_mxi_device(
				addr, 
				pvxie, 
				ext_import_mxi_into_vxi);
		if(!pnewvxie){
			continue;
		}
	
		/*
		 * open the LA window outward over the entire LA range  
		 */
		pmxi->dir.w.dd.mxi.control = 
			MXI_UPPER_LOWER_BOUNDS;
		pmxi->dir.w.dd.mxi.la_window = 
			VXIADDRMASK | (VXIADDRMASK<<NVXIADDRBITS);

		/*
		 * close any MXI devices which have open windows but
		 * have not been encountered by this execution
		 * of the resource manager.
		 *
		 * This makes the MXI/VXI configure correctly after
		 * a control x (soft) reboot.
		 */
		if(pass == laPassSC){
			vxi_unmap_mxi_devices();
		}

		/*
		 * for each MXI found that we have not seen before
		 * open the la window inward for all devices
		 */
		map_mxi_inward(pnewvxie, pass);

		/*
		 * Make sure parent window overlaps the child
		 */
		if(pnewvxie->la_mapped){
			pvxie->la_mapped = TRUE;
			pvxie->la_low = min(pvxie->la_low, pnewvxie->la_low);
			pvxie->la_high = max(pvxie->la_high, pnewvxie->la_high);
		}

		/*
		 * disable the window until the last pass
		 */
		if(pass != laPassSetWindows){
			pmxi->dir.w.dd.mxi.la_window =
				0 | (0<<NVXIADDRBITS);
			continue;
		}

		if(pnewvxie->la_mapped){
#			ifdef DEBUG
				printf(	"VXI resman: VXI to MXI(%x) %x-%x\n", 
					addr,
					pnewvxie->la_low,
					pnewvxie->la_high);
#			endif
			pmxi->dir.w.dd.mxi.la_window = 
				(pnewvxie->la_low<<NVXIADDRBITS)
					| (pnewvxie->la_high+1);

			/*
			 * if INTX is installed gate the interrupts off of
			 * INTX
			 */
			if(MXIINTX(pmxi)){
				pmxi->dir.w.dd.mxi.INTX_interrupt = 
					INTX_INT_IN_ENABLE;
			}
		}
		else{
			printf(	"VXI resman: VXI to MXI LA=0x%X is empty\n",
				addr); 
			pmxi->dir.w.dd.mxi.la_window =
				0 | (0<<NVXIADDRBITS);
		}
	}
}


/*
 *
 * OPEN_MXI_DEVICE
 *
 *
 */
LOCAL VXIE	*open_mxi_device(
unsigned	la,
VXIE		*pvxie,
enum ext_type	type
)
{
	VXIDI		*pvxidi;
	VXIE		*pnewvxie;
	struct vxi_csr	*pmxi;

	pmxi = VXIBASE(la);
	if(!VXIMXI(pmxi)){
		return NULL;
	}

	pvxidi = epvxiLibDeviceList[la];
	if(!pvxidi){
		return NULL;
	}				

	/*
	 * is it already open?
	 */
	if(pvxidi->pvxieSelf){
		return pvxidi->pvxieSelf;
	}

	pnewvxie = (VXIE *) calloc(1, sizeof(*pnewvxie));
	if(!pnewvxie){
		errMessage(S_epvxi_noMemory, "MXI device ignored");
		return NULL;
	}

	pnewvxie->type = type;
	pnewvxie->la = la;
	pnewvxie->la_low = last_la;
	pnewvxie->la_high = first_la;
	pnewvxie->pParent = pvxie;

	pvxidi->pvxieSelf = pnewvxie;

	/*
	 * make sure PARENT window includes the MXI
	 * bus extender 
	 */
	pvxie->la_mapped = TRUE;
	pvxie->la_low = min(pvxie->la_low, la);
	pvxie->la_high = max(pvxie->la_high, la);
	ellAdd(&pvxie->extenders, &pnewvxie->node);

	epvxiRegisterModelName(
			VXIMAKE(pmxi),
			VXIMODEL(pmxi),
			"MXI bus extender");
		
	return pnewvxie;
}


/*
 *  MAP_MXI_INWARD
 *
 * for each MXI found that we have not seen before
 * open the la window inward for all devices
 *
 */
LOCAL EPVXISTAT map_mxi_inward(
VXIE		*pvxie,
enum laPass	pass
)
{
	VXIDI		*pvxidi;
	struct vxi_csr	*pmxi_new;
	unsigned	addr;
	EPVXISTAT	status;
	VXIE		*pnewvxie;

	/*
	 * open all new MXI devices now
	 * so that we dont confuse them with
	 * SC devices when a MXI's window is 
	 * completely open.
	 *
	 * If we attempt to communicate with a 
	 * MXI device while another MXI device 
	 * at the same level has its window open 
	 * all the way we see VME bus conflicts.
	 */
       	for(addr=first_la; addr<=last_la; addr++){

		pvxidi = epvxiLibDeviceList[addr];
		if(!pvxidi){
			/*
			 * if it has not been seen before we know
			 * its a MXI device
			 */
			status = open_vxi_device(pvxie, addr);
			if(status==VXI_SUCCESS){
				open_mxi_device(
						addr, 
						pvxie, 
						ext_export_vxi_onto_mxi);
			}
		}
	}

	/*
	 * now step through and open up all MXI devices found
	 */
       	for(addr=first_la; addr<=last_la; addr++){

		pvxidi = epvxiLibDeviceList[addr];
		
               	if(!pvxidi){
			continue;
		}

		pnewvxie = pvxidi->pvxieSelf;

		/*
		 * dont bother with
		 * devices that are 
		 * not extenders
		 * here
		 */
		if(!pnewvxie){
			continue;
		}

		/*
		 * if it is an extender dont
		 * configure it unless
		 * it is a child of the 
		 * current parent
		 */
		if(pvxidi->pvxie != pvxie){
			continue;
		}

		pmxi_new = VXIBASE(addr);

		/*
		 * open the address window inward for all device
		 */
		pmxi_new->dir.w.dd.mxi.control = 
			MXI_UPPER_LOWER_BOUNDS;
		pmxi_new->dir.w.dd.mxi.la_window =
			1 | (1<<NVXIADDRBITS);

		mxi_map(pnewvxie, pass);	
		
		/*
		 * make sure that the parents LA range
		 * covers this child
		 */
		if(pnewvxie->la_mapped){
			pvxie->la_mapped = TRUE;
			pvxie->la_low = min(pvxie->la_low, pnewvxie->la_low);
			pvxie->la_high = max(pvxie->la_high, pnewvxie->la_high);
		}

		/*
		 * temporarily close the windows so that we can discover
		 * iproperly located SC devices
		 */
		if(pass != laPassSetWindows){
			pmxi_new->dir.w.dd.mxi.la_window =
				0 | (0<<NVXIADDRBITS);
			continue;
		}

		/*
		 * restrict the address window inward to only 
		 * cover devices seen during pass laPassSetWindows
		 */
		if(pnewvxie->la_mapped){
#			ifdef DEBUG
				printf(	"VXI resman: MXI to VXI LA=%x %x-%x\n", 
					addr,
					pnewvxie->la_low,
					pnewvxie->la_high);
#			endif
			pmxi_new->dir.w.dd.mxi.la_window = 
				pnewvxie->la_low | 
				((pnewvxie->la_high+1)<<NVXIADDRBITS);

			/*
			 * if INTX is installed route the interrupts onto
			 * INTX
			 */
			if(MXIINTX(pmxi_new)){
				pmxi_new->dir.w.dd.mxi.INTX_interrupt = 
					INTX_INT_OUT_ENABLE;
			}
		}
		else{

			printf(	"VXI resman: MXI to VXI LA=0x%X is empty\n",
				addr); 
			pmxi_new->dir.w.dd.mxi.la_window =
				0 | (0<<NVXIADDRBITS);

#			ifdef REMOVE_UNUSED_SLOT_ZERO_DEVICES
				/*
				 * slot zero device(s) will not 
				 * be needed in this case 
				 *
				 * Take it (them) out of the list in 
				 * case it (they) become(s) 
				 * inaccessable later.
				 */
				pvxisz = (VXISZ *) crateList.node.next;
				while(pvxisz){
					next = (VXISZ *) pvxisz->node.next;
					if(pvxisz->la == pnewvxie->la || 
						pvxisz->pvxie == pnewvxie){
						ellDelete(&crateList, &pvxisz->node);
					}
					pvxisz = next;
				}
#			endif /*REMOVE_UNUSED_SLOT_ZERO_DEVICES*/
		}
	}
	return VXI_SUCCESS;
}


/*
 *
 * open_vxi_device
 *
 *
 */
LOCAL EPVXISTAT open_vxi_device(
VXIE		*pvxie,
unsigned	la
)
{
	struct vxi_csr		*pdevice;
	VXIDI			*plac;
	int16_t			id;
	EPVXISTAT		status;

	/*
	 * just return if this device is known about
	 */
	if(epvxiLibDeviceList[la]){
		return VXI_SUCCESS;
	}

	pdevice = VXIBASE(la);

  	status = vxMemProbe(	(char *)pdevice,
				READ,
				sizeof(id),
				(char *)&id);
  	if(status<0){
		return S_dev_noDevice;
	}

	status = verify_valid_window(pvxie, la);
	if(status){
		errMessage(
			status, 
			"VXI resman: no access to SC device");
		errMessage(
			status, 
			"VXI resman: without MXI LA window overlap.");
		errPrintf(	
			status, 
			__FILE__,
			__LINE__,
			"VXI resman: SC device LA=0X%X", 
			la);
		errPrintf(
			status, 
			__FILE__,
			__LINE__,
			"VXI resman: extender LA=0X%X", 
			pvxie->la);
		errMessage(
			status, 
			"VXI resman: SC device ignored");
		return status;	
	}

	plac = (VXIDI *) calloc(1, sizeof(*plac));
	if(!plac){
		errMessage(S_epvxi_noMemory,"... continuing");
		return S_epvxi_noMemory;
	}

	plac->make = VXIMAKE(pdevice);
	plac->model = VXIMODEL(pdevice);
	plac->class = VXICLASS(pdevice);
	plac->pvxie = pvxie;
	epvxiLibDeviceList[la] = plac;

	pvxie->la_low = min(pvxie->la_low, la);
	pvxie->la_high = max(pvxie->la_high, la);
	pvxie->la_mapped = TRUE;

	if(vxi_vec_inuse(la)){
		errPrintf(
			S_epvxi_badConfig,
			__FILE__,
			__LINE__,
			"SC VXI device at allocated int vec=0x%X",
			la);
		epvxiSetDeviceOffline(la);
	}
	else{
		 if(VXISLOT0MODELTEST(plac->model)){
			plac->slot0_dev = TRUE;
			open_slot0_device(pvxie, la);
		}
	}

#	ifdef DEBUG
		printf("Found LA=0X%X extender LA=0X%X\n", la, pvxie->la);
#	endif

	return VXI_SUCCESS;
}


/*
 *
 * verify_valid_window() 
 *
 * determine if this la is within one
 * of the other extenders at the same level
 *
 */
LOCAL EPVXISTAT verify_valid_window(
VXIE		*pvxie,
unsigned	la
)
{
	VXIE	*pChild;

	/*
	 * If its the root extender we dont care
	 */
	if(!pvxie->pParent){
		return VXI_SUCCESS;
	}

	for(	pChild = (VXIE *) pvxie->pParent->extenders.node.next;
		pChild; 
		pChild = (VXIE *) pChild->node.next){

		/*
		 * of course its ok to be in the
		 * current extender
		 */
		if(pChild == pvxie){
			continue;
		}

		/*
		 * its not ok to overlap other extenders
		 * window
		 */
		if(pChild->la_mapped){
			if(la >= pChild->la_low && 
				la <= pChild->la_high){

				return S_epvxi_badConfig; 
			}
		}
	}

	/*
	 * traverse the hierarchy
	 */
	return verify_valid_window(pvxie->pParent, la);
}


/*
 *
 *
 *	VXI_FIND_SC_DEVICES
 *
 */
LOCAL void vxi_find_sc_devices(
VXIE		*pvxie
)
{
	unsigned 	addr;

	/*
	 * Locate the slots of all SC devices
	 */
	for(addr=first_la; addr<=last_la; addr++){
	
		/*
		 * dont configure devices seen before
		 */
		if(epvxiLibDeviceList[addr]){
			continue;
		}

		(void) open_vxi_device(pvxie, addr);
	}
}


/*
 *
 * vxi_record_topology()
 *
 * Record topological information after all MXIs
 * and slot zero cards have been located since
 * MXIs can appear in the address space prior
 * to their slot zero cards ( so their slots
 * cant be found initially ).
 *
 */
LOCAL void vxi_record_topology(void)
{
	VXIDI		**pplac;
	struct vxi_csr	*pdevice;
	VXISZ		*pvxisz;
	unsigned	la;
	unsigned	slot;
	EPVXISTAT	status;

	for(	la=first_la, pplac = epvxiLibDeviceList; 
		la<=last_la; 
		la++, pplac++){

		if(!*pplac){
			continue;
		}

		pdevice = VXIBASE(la);
		status = vxi_find_slot(pdevice, &slot, &pvxisz);
		if(status==VXI_SUCCESS){
			(*pplac)->slot = slot;
			(*pplac)->slot_zero_la = pvxisz->la;
			(*pplac)->extender_la = pvxisz->pvxie->la;

			if(VXISLOT0MODEL(pdevice)){
				if((*pplac)->slot!=0){
					errPrintf(	
						S_epvxi_badConfig,
						__FILE__,
						__LINE__,
			"VXI slot 0 found in slot %d? LA=0x%X", 
						(*pplac)->slot,
						la);
				}
			}
		}
		else{
			errPrintf(
				status,
				__FILE__,
				__LINE__,
				"LA=0X%X",
				la);
			(*pplac)->slot = UKN_SLOT;
			(*pplac)->slot_zero_la = UKN_LA;
			(*pplac)->extender_la = UKN_LA;
		}
	}
}


/*
 *
 *
 *	VXI_FIND_DC_DEVICES
 *
 */
LOCAL void vxi_find_dc_devices(
VXIE		*pvxie
)
{
	unsigned 	maxInBlockedDC; /* max LA in a blocked addr DC device */
	int		prealloc;
	int16_t		id;
	EPVXISTAT	status;
	unsigned	offset;
	struct vxi_csr	*pcsr;
	VXISZ		*pvxisz;
	unsigned 	nDC;

	/*
	 * dont move DC devices if SC device at address 0xff
	 */
	if(epvxiLibDeviceList[VXIDYNAMICADDR]){
		errPrintf(
			S_epvxi_badConfig,
			__FILE__,
			__LINE__,
			"VXI SC device recorded at dynamic address 0x%X",
			VXIDYNAMICADDR);
		errMessage(
			S_epvxi_badConfig,
			"VXI DC devices ignored");
		return;
	}

	pcsr = VXIBASE(VXIDYNAMICADDR);
	status = vxMemProbe(	(char *)pcsr,
				READ,
				sizeof(id),
				(char *)&id);
  	if(status == OK){
		errPrintf(
			S_epvxi_badConfig,
			__FILE__,
			__LINE__,
			"VXI SC device at dynamic address 0x%X",
			VXIDYNAMICADDR);
		errMessage(
			S_epvxi_badConfig,
			"VXI DC devices ignored");
		return;
	}

	/*
	 * if unanchored force them to all be in one
	 * contiguous block
	 */
	prealloc = FALSE;
	if(!pvxie->la_mapped){

		/*
		 * find out how many DC devices are in the system
		 * and there worst case (blocked address) alignment
		 */
		status = vxi_count_dc_devices(
				pvxie, 
				&nDC,
				&maxInBlockedDC);
		if(status){
			return;
		}
		if(nDC < 1){
			return;
		}
		/*
		 * allocate with worst case (blocked address) alignment
		 */
		status = vxi_alloc_la(
				pvxie,
				nDC,
				blocked_la_alignment (maxInBlockedDC),	
				&offset);
		if(status){
			errMessage(
				status,
		"VXI resman: unanchored DC VXI device block doesnt fit");
			errPrintf(
				status,
				__FILE__,
				__LINE__,
		"VXI resman: %d DC VXI devices in extender LA=0X%X ignored",
				nDC,
				pvxie->la);
			return;
		}
		prealloc = TRUE;
	}

	/*
	 * find all DC devices 
	 */
	pvxisz = (VXISZ *) crateList.node.next; 
	while(pvxisz){
		/*
		 * We wish to selectively configure DC devices 
		 * accessed through the current extender.
		 *
		 * If the slot zero card is the extender then
		 * the LAs of the extender and the slot zero will
		 * match. Otherwise the slotzero card was found
		 * by opening up the window in the extender
		 * and the slot zeros extender will be the
		 * current extender.
		 */
		if(pvxisz->pvxie == pvxie || pvxisz->la == pvxie->la){
			unsigned  count;

			/*
			 * allocate largest blocked address devices first ...
			 * so that we dont end up with pads for alignment 
			 * purposes
			 */
			count = 1 << blocked_la_alignment (maxInBlockedDC);
			while (count) {
				vxi_assign_dc_addresses(
						pvxie,
						prealloc,
						&offset,
						pvxisz,
						count);
				count = count >> 1;
			}
			CLRMODID(pvxisz);
		}
		pvxisz = (VXISZ *) pvxisz->node.next;
	}
}


/*
 * vxi_assign_dc_addresses()
 */
LOCAL void vxi_assign_dc_addresses(
VXIE		*pvxie,
int		prealloc,
unsigned	*pOffset,
VXISZ		*pvxisz,
unsigned	minNumberDC
)
{
	unsigned 	slot;
	unsigned	status;

	for(slot=0;slot<NVXISLOTS;slot++){
		int currentPrealloc;

		SETMODID(pvxisz, slot);
		/*
		 * if there are multiple independently
		 * addressed LAs in the slot then I have
		 * no clean way to determine ahead of
		 * time there number so I abandon
		 * preallocation of space after the first
		 * iteration. This does not apply to
		 * multiple blocked address devices
		 * per slot however.
		 */
		currentPrealloc = prealloc;
		while(TRUE){
			/*
			 * loop here in case there
			 * are multiple independently
			 * addressed devices at this slot
			 */
			status = vxi_program_dc_addresses (
					pvxie,
					currentPrealloc,
					pOffset,
					pvxisz,
					slot,
					minNumberDC);
			if(status != VXI_SUCCESS){
				break;
			}
			currentPrealloc = FALSE;
		}
	}
}


/*
 * vxi_program_dc_addresses ()
 */
LOCAL EPVXISTAT vxi_program_dc_addresses (
VXIE		*pvxie,
int		prealloc,
unsigned	*pOffset,
VXISZ		*pvxisz,
int		slot,
unsigned	minNumberDC
)
{
	unsigned	offset;
	unsigned 	nextOffset;
	unsigned 	count;
	unsigned	mask;
	EPVXISTAT	status;
	int16_t		id;
	struct vxi_csr	*pcsr;

	pcsr = VXIBASE(VXIDYNAMICADDR);

	status = vxMemProbe(	
			(char *)pcsr,
			READ,
			sizeof(id),
			(char *)&id);
	if(status<0){
		return S_epvxi_noDevice;
	}

	count = blocked_la_count(pcsr);
	mask = (1<<blocked_la_alignment(count))-1;

	/*
	 * this is done in multiple passes.
	 * return when the operation 
	 * is postponed until later.
	 */
	if ( (mask+1) < minNumberDC) {
		return S_epvxi_noMemory;
	}

	if(prealloc){
		offset = *pOffset;
	}
	else{
		status = vxi_alloc_la(
				pvxie,
				count,
				blocked_la_alignment(count),	
				&offset);
		if(status){
			errPrintf(
				status,
				__FILE__,
				__LINE__,
				"VXI: %d DC VXI device(s) do(es) not fit",
				count);
			errPrintf(
				status,
				__FILE__,
				__LINE__,
		"VXI: DC VXI device(s) at slot %d in extender LA=0X%X ignored",
				slot,
				pvxie->la);
			return S_epvxi_noMemory;
		}
	}

	/*
	 * blocked addr devices must recv their
	 * addr assignements in unison
	 * at a natural alignement
	 */
	assert ( (offset&mask)==0 );
	pcsr->dir.w.addr = offset;
	nextOffset = offset + mask + 1;

	while (count) {
		status = open_vxi_device(
				pvxie, 
				offset);
		if(status){
			errPrintf(
				status,
				__FILE__,
				__LINE__,
			"VXI resman: DC dev assign to LA=0X%X failed",
				offset);
			errPrintf(
				status,
				__FILE__,
				__LINE__,
			"VXI resman: Slot Zero LA=0X%X",
				pvxisz->la);
			errPrintf(
				status,
				__FILE__,
				__LINE__,
				"VXI resman: DC VXI device ignored");
		}
		count--;
		offset++;
	}

	if(prealloc){
		*pOffset = nextOffset;
	}

	return status;
}


/*
 *
 *
 *	VXI_COUNT_DC_DEVICES
 *
 */
LOCAL EPVXISTAT vxi_count_dc_devices(
VXIE		*pvxie,
unsigned	*pCount,
unsigned	*pMaxInBlockedDC /* max LA in a blocked addr DC device */
)
{
	int16_t		id;
	EPVXISTAT	status;
	struct vxi_csr	*pcsr;
	VXISZ		*pvxisz;
	int		slot;
	unsigned	nDC;
	unsigned	maxInBlockedDC;

	/*
	 * dont count DC devices if SC device at address 0xff
	 */
	if(epvxiLibDeviceList[VXIDYNAMICADDR]){
		status = S_epvxi_badConfig;
		errMessage(status, "SC device at DC address");
		return status;
	}

	pcsr = VXIBASE(VXIDYNAMICADDR);
	status = vxMemProbe(	(char *)pcsr,
				READ,
				sizeof(id),
				(char *)&id);
  	if(status == OK){
		status = S_epvxi_badConfig;
		errMessage(status, "SC device at DC address");
		return status;
	}

	/*
	 * find all dynamic modules
	 */
	nDC=0;
	maxInBlockedDC=0;
	pvxisz = (VXISZ *) crateList.node.next; 
	while(pvxisz){
		/*
		 * We wish to selectively configure DC devices 
		 * accessed through the current extender.
		 *
		 * If the slot zero card is the extender then
		 * the LAs of the extender and the slot zero will
		 * match. Otherwise the slotzero card was found
		 * by opening up the window in the extender
		 * and the slot zeros extender will be the
		 * current extender.
		 *
		 * Counts multiple blocked addr device per slot here.
		 * Does not try to count multiple independently
		 * addressed devices per slot here. Space is
		 * allocated for these DC devices on demand due to
		 * difficulties counting them ahead of time.
		 */
		if(pvxisz->pvxie == pvxie || 
			pvxisz->la == pvxie->la){

			for(slot=0; slot<NVXISLOTS; slot++){
				unsigned 	blksiz;

				SETMODID(pvxisz, slot);

				status = vxMemProbe(	
						(char *)pcsr,
						READ,
						sizeof(id),
						(char *)&id);
				if(status>=0){
					unsigned count;

					/*
					 * Force the count to be the true 
					 * (including alignement overhead)
					 */
					blksiz = blocked_la_count(pcsr);
					count = 1<<blocked_la_alignment(blksiz);
					nDC += count;
					maxInBlockedDC = max (maxInBlockedDC, count);
				}
			}
			CLRMODID(pvxisz);
		}

		pvxisz = (VXISZ *) pvxisz->node.next;
	}
	*pCount = nDC;
	*pMaxInBlockedDC = maxInBlockedDC;
	return VXI_SUCCESS;
}


/*
 *
 * blocked_la_alignment()
 *
 * Rule F.2.7
 *
 * determine the required alignment for DC 
 * blocked address devices
 *
 */
LOCAL unsigned blocked_la_alignment(unsigned blockedLACount)
{
        unsigned alignment;

        if (blockedLACount <= 1) {
                return 0;
        }

        alignment = 0;
        blockedLACount = blockedLACount-1;
        while (blockedLACount) {
                alignment++;
                blockedLACount = blockedLACount >> 1;
        }

        return alignment;
}


/*
 *
 * blocked_la_count()
 *
 */
LOCAL unsigned blocked_la_count(struct vxi_csr *pcsr)
{
	unsigned	blockedAddrCount;

	/*
	 * Rule F.2.6
	 */
	blockedAddrCount = VXINDCDEVICES(pcsr);
	if(blockedAddrCount==0 || blockedAddrCount==0xff){
		blockedAddrCount = 1;
	}
	return blockedAddrCount;
}


/*
 *
 * open slot 0  device
 *
 *
 */
LOCAL void open_slot0_device(
VXIE		*pvxie,
unsigned	la 
)
{
	struct vxi_csr		*pcsr;
	EPVXISTAT		status;
	VXISZ			*pvxisz;

	pcsr = VXIBASE(la);

	/*
	 * MXI's are device class extended
	 */
	if(VXICLASS(pcsr) != VXI_REGISTER_DEVICE){
		if(!VXIMXI(pcsr)){
			errPrintf(
				S_epvxi_badConfig,
				__FILE__,
				__LINE__,
"Only register based slot 0 devices currently supported LA=0x%X",
				la);
			return;
		}
	}
	
	pvxisz = (VXISZ *) crateList.node.next;
	while(pvxisz){
		if(pvxisz->pcsr == pcsr){
			return;
		}
		pvxisz = (VXISZ *) pvxisz->node.next;
	}

	pvxisz = (VXISZ *) calloc(1, sizeof(*pvxisz));
	if(!pvxisz){
		errMessage(
			S_epvxi_noMemory,
			"continuing...");
		return;
	}
	pvxisz->reg = TRUE;
	pvxisz->pcsr = pcsr;
	pvxisz->set_modid = set_reg_modid;
	pvxisz->clear_modid = clr_all_reg_modid;
	pvxisz->pvxie = pvxie;
	pvxisz->la = la;

	ellAdd(&crateList, &pvxisz->node);

	/*
	 * force the slot zero device into a known state
	 */
	CLRMODID(pvxisz);

	if(!epvxiLibDeviceList[la]){
		status = open_vxi_device(pvxie, la);
		if(status){
			errPrintf(
				status,
				__FILE__,
				__LINE__,
				"attempted slot zero device open la=0X%X",
				la);
			return;
		}
	}
}





/*
 *
 *	NICPU030_INIT()	
 *	check to see if this code is running on a
 *	national instruments cpu030 installed in
 *	slot zero.
 *
 */
#ifdef NICPU030
LOCAL EPVXISTAT nicpu030_init(
VXIE	*pvxie
)
{
	int		i;
	EPVXISTAT	status = S_epvxi_internal;
	int16_t		model;
	SYM_TYPE	type;
	UINT8		la;
	
	/*
	 * If we are running this code on the NI 030
	 * we are the resource manager and the NI 030 will
	 * be the first slot zero card found.
	 */
	if(niCpu030Initialized){
		return VXI_SUCCESS;
	}

	for(i=0; i<NELEMENTS(nivxi_func_names); i++){

		if(pnivxi_func[i]){
			continue;
		}

		status = symFindByName(
				sysSymTbl,
				nivxi_func_names[i],
				(char **) &pnivxi_func[i],
				&type);
		if(status != OK){
			return VXI_SUCCESS;
		}
	}

	status = (*pnivxi_func[(unsigned)e_vxiinit])();
        if(status<0){
		status = S_epvxi_internal;
		errMessage(
			status, 
			"NI \"vxiinit\" failed");
                return status;
        }

	status = (*pnivxi_func[(unsigned)e_InitVXIlibrary])();
	if(status<0){
		status = S_epvxi_internal;
		errMessage(
			status, 
			"NIVXI library init failed");
#if 0
		return status;
#endif
	}

#	define VXI_MODEL_REGISTER 2
	status = (*pnivxi_func[(unsigned)e_VXIinLR])(
			VXI_MODEL_REGISTER,
			sizeof(model),
			&model);
	if(status<0){
		status = S_epvxi_internal;
		errMessage(
			status, 
                	"vxi resman: CPU030 model type read failed");
                return status;
        }

	la = (*pnivxi_func[(unsigned)e_GetMyLA])();

	niCpu030Initialized = TRUE;

	if(VXISLOT0MODELTEST(VXIMODELMASK(model))){
		VXISZ	*pvxisz;

		pvxisz = (VXISZ *) calloc(1, sizeof(*pvxisz));
		if(!pvxisz){
			status = S_epvxi_noMemory;
			errMessage(status, NULL);
			return status;
		}

		pvxisz->nicpu030 = TRUE;
		pvxisz->set_modid = nivxi_cpu030_set_modid;
		pvxisz->clear_modid = nivxi_cpu030_clr_all_modid;
		pvxisz->la = la;
		pvxisz->pvxie = pvxie;

		ellAdd(&crateList, &pvxisz->node);
	}
	return VXI_SUCCESS;
}
#endif



/*
 *
 *	vxi_alloc_la()	
 * 
 */
LOCAL EPVXISTAT vxi_alloc_la(
VXIE		*pvxie,
unsigned	count,
unsigned	alignment,	/* number of zero ls bits in LA */
unsigned	*poffset
)
{
	EPVXISTAT	status;
	unsigned	hla;
	VXIDI		*pvxidi;
	unsigned	base_la;
	unsigned	peak;
	unsigned 	la;
	unsigned	mask = (1<<alignment)-1;
	
	if(count<1){
		status = S_epvxi_internal;
		errMessage(status,NULL);
		return status;
	}

	peak = 0;
	if(pvxie->la_mapped){

		/*
		 * look inside the range thats mapped (or above) first
		 * (from the bottom up))
		 */
		for(base_la=la=pvxie->la_low; la<=last_la; la++){

			pvxidi = epvxiLibDeviceList[la];

			/*
			 * If we are starting out with the alignement
			 * wrong then just continue
			 */
			if (peak==0) {
				base_la = la;
				if (base_la&mask) {
					continue;
				}
			}

			/*
			 * skip all devices seen before
			 */
			if(pvxidi){
				/*
				 * LA window cant cross extender boundaries
				 * so just quit
				 */
				if(pvxidi->pvxie != pvxie){
					break;
				}
				peak = 0;
				continue;
			}

			if(vxi_la_occupied(la)){
				peak = 0;
				continue;
			}

			peak++;

			/*
			 * if count is not evenly divisable by
			 * (mask+1) then extend the block size
			 * out to natural alignment
			 */
			if (peak >= count && ((la+1)&mask)==0 ) {
				assert ( (base_la&mask)==0 );
				*poffset = base_la;
				return VXI_SUCCESS;
			}
		}

		hla = pvxie->la_low;
	}
	else{
		/*
		 * unachored DC device allocations go from high 
		 * to low so that we avoid mc680xx reserved
		 * interrupt vectors
		 */
		hla = last_la;
	}

	/*
	 * Now look from the top down 
	 *
	 * stop before unsigned la=0 is decremented
	 * (shouldnt allocate the resource manager's LA anyways)
	 */
	peak=0;
	la=hla;
	base_la = la - (count-1);
	for(; la>=max(VXI_RESMAN_LA+1,first_la); la--){

		/*
		 * If we are starting out with the alignement
		 * wrong then just continue
		 */
		if (peak==0) {
			base_la = la - (count-1);
			if (base_la&mask) {
				continue;
			}
		}

		pvxidi = epvxiLibDeviceList[la];

		/*
		 * skip all devices seen before
		 */
		if(pvxidi){
			/*
			 * LA window cant cross extender boundaries
			 * (if something is mapped already) so just quit
			 */
			if(pvxie->la_mapped){
				if(pvxidi->pvxie != pvxie){
					break;
				}
			}

			peak= 0;
			continue;
		}

		if(vxi_la_occupied(la)){
			peak= 0;
			continue;
		}

		peak++;

		/*
		 * if count is not evenly divisable by
		 * (mask+1) then extend the block size
		 * out to natural alignment
		 */
		if (peak >= count && la == base_la) {
			assert ( (base_la&mask)==0 );
			*poffset = base_la;
			return VXI_SUCCESS;
		}
	}

	return S_epvxi_internal;
}


/*
 *
 * vxi_la_occupied()
 *
 */
LOCAL EPVXISTAT vxi_la_occupied(unsigned la)
{
	struct vxi_csr 	*pcsr;
	int16_t		*pi16;
	int16_t		i16;
	EPVXISTAT	s;

	/*
 	 * dont allocate the resource manager's LA
	 */
	if(la == VXI_RESMAN_LA){
		return TRUE;
	}

	/*
	 * Check to see if this LA belongs to
	 * the NI 030 CPU (that does not show 
	 * in A16 when it is a VME BM)
	 */
	if(niCpu030Initialized){
		if(la == (*pnivxi_func[(unsigned)e_GetMyLA])()){
			return TRUE;
		}
	}

	/*
	 * Probe the entire LA space
	 */
	pcsr = VXIBASE(la);
	for(	pi16 = (int16_t *) &pcsr->dir.r.make;
		pi16 <= (int16_t *) &pcsr->dir.r.dd.reg.ddx2e;
		pi16++){

	  	s = vxMemProbe(	(char *)pi16,
				READ,
				sizeof(i16),
				(char *)&i16);
	  	if(s == OK){
			return TRUE;
		}
	}

	/* 
	 * dont allow vxi int vec to overlap 
	 * VME vectors in use 
	 */
	if(vxi_vec_inuse(la)){
		return TRUE;
	}
		
	return FALSE;
}



/*
 *
 *	VXI_FIND_SLOT
 *	given a VXI module's addr find its slot
 *
 */
LOCAL EPVXISTAT vxi_find_slot(
struct vxi_csr		*pcsr,
unsigned  		*pslot,
VXISZ			**ppvxisz
)
{
	VXISZ		*pvxisz;
	EPVXISTAT	status;
	unsigned char	slot;

	status = S_epvxi_slotNotFound;

	/*
	 * RULE C.2.7
	 */
	if(VXIMODIDSTATUS(pcsr->dir.r.status)){
		errMessage(status, "device's MODID status is active & no MODID?");
		return status;
	}

	pvxisz = (VXISZ *) crateList.node.next;
	while(pvxisz){

		/*
		 * if it is a slot zero card
		 * then dont bother searching
		 */
		if(pvxisz->pcsr == pcsr){
			*pslot = 0;
			*ppvxisz = pvxisz;
			status = VXI_SUCCESS;
			break;
		}
 
		for(slot=0;slot<NVXISLOTS;slot++){
			SETMODID(pvxisz, slot);
  			if(VXIMODIDSTATUS(pcsr->dir.r.status)){
				*pslot = slot;
				*ppvxisz = pvxisz;
				status = VXI_SUCCESS;
				break;
			}
		}
		CLRMODID(pvxisz);

		if(status == VXI_SUCCESS)
			break;

		pvxisz = (VXISZ *) pvxisz->node.next;
	}

	return status;
}


/*
 *
 *	VXI_RESET_DC
 *	force all dynamic devices back to address 0xff
 *	(In case this is a ctrl X restart)
 *
 *	not tested with at5vxi modules
 */
#ifdef JUNKYARD
LOCAL EPVXISTAT vxi_reset_dc(void)
{
	register unsigned 	addr;
	unsigned 		slot;
	unsigned 		crate;
	int16_t			id;
	EPVXISTAT		status;
	struct vxi_csr_w	*pcr;
	struct vxi_csr_r	*psr;

	for(addr=first_la; addr<=last_la; addr++){

		psr = (struct vxi_csr_r *) VXIBASE(addr);
		pcr = (struct vxi_csr_w *) psr;

	  	status = vxMemProbe(	psr,
					READ,
					sizeof(id),
					&id);
  		if(status == ERROR)
			continue;

		status = vxi_find_slot(psr, &slot, &crate);
		if(status){
			return status;
		}

		SETMODID(slot, crate);


		pcr->addr = VXIDYNAMICADDR;
	}

	return VXI_SUCCESS;
}
#endif


/*
 *
 *	VXI_DC_TEST
 *	determine if a VXI module in the static address range is dynamic
 *
 */
#ifdef JUNKYARD
LOCAL EPVXISTAT vxi_dc_test(
unsigned 	current_addr
)
{
	register unsigned 	addr;
	unsigned 		slot;
	unsigned 		crate;
	int16_t			id;
	EPVXISTAT		status;
	struct vxi_csr_w	*pcr;
	struct vxi_csr_r	*psr;

	static unsigned		open_addr;
	unsigned		dynamic;

	for(addr=first_la; addr<=last_la; addr++){

	  	status = vxMemProbe(	VXIBASE(addr),
					READ,
					sizeof(id),
					&id);
  		if(status == ERROR){
			open_addr = addr;
			break;
		}
	}

	psr = (struct vxi_csr_r *) VXIBASE(current_addr);
	pcr = (struct vxi_csr_w *) psr;

	status = vxi_find_slot(psr, &slot, &crate);
	if(status){
		errMessage(status,NULL);
		return status;
	}

	SETMODID(slot, crate);
	pcr->addr = open_addr;

	psr = (struct vxi_csr_r *) VXIBASE(open_addr);
	pcr = (struct vxi_csr_w *) psr;

	status = vxMemProbe(	psr,
				READ,
				sizeof(id),
				&id);

	if(status==OK){
		dynamic = TRUE;
		pcr->addr = current_addr;
	}
	else
		dynamic = FALSE;

	status = vxMemProbe(	VXIBASE(current_addr),
				READ,
				sizeof(id),
				&id);
	if(status == ERROR)
		return S_epvxi_internal;


	return dynamic;
}
#endif


/*
 *
 *      VXI_CONFIGURE_HIERARCHIES
 *
 */
LOCAL void vxi_configure_hierarchies(
unsigned	commander_la,
unsigned	servant_area
)
{
        EPVXISTAT		status;
        struct vxi_csr		*pcsr;
	unsigned long		response;
	VXIDI			**ppvxidi;
	VXIDI			*pvxidi;
	unsigned		sla;
	unsigned		last_sla;
	unsigned		area;

	last_sla = servant_area+commander_la;

	if(last_sla >= NELEMENTS(epvxiLibDeviceList)){
		errPrintf(	
			S_epvxi_internal,
			__FILE__,
			__LINE__,
			"VXI resman: Clipping servant area (LA=0x%X)",
			commander_la);
		last_sla = NELEMENTS(epvxiLibDeviceList)-1;
	}

	sla = commander_la+1;
	ppvxidi = &epvxiLibDeviceList[sla];
	for(	;
		sla<=last_sla;
		sla += area+1, ppvxidi += area+1){

		pvxidi = *ppvxidi;
		area = 0;

		if(!pvxidi){
			continue;
		}

		pvxidi->commander_la = commander_la;

		if(!pvxidi->st_passed){
			continue;
		}

		pcsr = VXIBASE(sla);

		if(VXICLASS(pcsr) != VXI_MESSAGE_DEVICE){
			continue;
		}

		if(commander_la != VXI_RESMAN_LA){
        	        status = epvxiCmdQuery(
					commander_la,
					(unsigned long)MBC_GRANT_DEVICE | sla,
					&response);
			if(status){
				errPrintf(
					status,
					__FILE__,
					__LINE__,
					"VXI resman: GD failed (LA=0x%X)",
					sla);
			}
			else{
				printf(
					"VXI resman: gd resp %x\n",
					response);
			}
		}
		if(VXICMDR(pcsr)){
        	        status = epvxiCmdQuery(
					sla,
					(unsigned long)MBC_READ_SERVANT_AREA,
					&response);
			if(status){
				errPrintf(
					status,
					__FILE__,
					__LINE__,
					"VXI resman: RSA failed (LA=0x%X)",
					sla);
			}
			else{
				area = response & MBR_READ_SERVANT_AREA_MASK;

				printf(	"The servant area was %d (LA=0x%X)\n",
					area,
					sla);

				vxi_configure_hierarchies(
					sla, 
					area);
			}
		}
	}
}


/*
 *
 *      VXI_BEGIN_NORMAL_OPERATION
 *
 */
LOCAL void vxi_begin_normal_operation(void)
{
        EPVXISTAT		status;
	unsigned		la;
	VXIDI			**ppvxidi;
	VXIDI			*pvxidi;
	struct vxi_csr		*pcsr;

	for(	la=0, ppvxidi = epvxiLibDeviceList;
		ppvxidi < epvxiLibDeviceList+NELEMENTS(epvxiLibDeviceList);
		ppvxidi++, la++){

		unsigned	cmdr;
		unsigned long	cmd;
		unsigned long	resp;

		pvxidi = *ppvxidi;

		if(!pvxidi){
			continue;
		}

		pcsr = VXIBASE(la);

		if(!pvxidi->st_passed){
			continue;
		}

		if(VXICLASS(pcsr) != VXI_MESSAGE_DEVICE){
			continue;
		}

		cmdr = VXICMDR(pcsr);

		cmd = 	MBC_BEGIN_NORMAL_OPERATION;
/*
 * this will send the begin nml op command to servants which
 * have a commander
 *
 * more work needs to be done here if this situation occurs
 * see below
 */
		if(cmdr){
			cmd |= MBC_TOP_LEVEL_CMDR;
		}
		status = epvxiCmdQuery(la, cmd, &resp); 
		if(status){
			errPrintf(
				status,
				__FILE__,
				__LINE__,
	"VXI resman: Device rejected BEGIN_NORMAL_OPERATION LA=0x%X (reason=%d)",
				la,
				status);
		}
		else if(
			MBR_STATUS(resp)!=MBR_STATUS_SUCCESS ||
			MBR_BNO_STATE(resp)!=MBR_BNO_STATE_NO){
			errPrintf(
				S_epvxi_msgDeviceFailure,
				__FILE__,
				__LINE__,
	"VXI resman: Device rejected BEGIN_NORMAL_OPERATION LA=0x%X (status=%x) (state=%x)",
				la,
				MBR_STATUS(resp),
				MBR_BNO_STATE(resp));
		}
		else{
			pvxidi->msg_dev_online = TRUE;
		}

		/*
		 * Dont send begin normal operation cmd
		 * to servants who have a commander
		 */
/*
 * apparently this is not a good enough test
 * for CMDR since some devices are rejecting this cmd
 */
#if 0
		if(cmdr){
			unsigned long	sa=0;

			printf("Found a msg based cmdr\n");

                	status = epvxiCmdQuery(
                       		        la,
                               		(unsigned long)MBC_READ_SERVANT_AREA,
					&sa);
                	if(status){
				errMessage(
					status,
                        		"vxi resman: rsa failed");
			}
			else{
				sa = sa & MBR_READ_SERVANT_AREA_MASK;
				printf(
					"The servant area was %d\n",
					sa);
				la += sa;
			}
		}
#endif
	}
}


/*
 *
 *	VXI_SELF_TEST
 *	check self test bits and place in safe state if failed
 *	print message about failed devices
 *
 */
LOCAL EPVXISTAT vxi_self_test(void)
{
	unsigned 		la;
	uint16_t		wd;
	struct vxi_csr		*pcsr;
	VXIDI			**ppvxidi;


	for(	la=0, ppvxidi = epvxiLibDeviceList;
		ppvxidi < epvxiLibDeviceList+NELEMENTS(epvxiLibDeviceList);
		ppvxidi++, la++){

		if(!*ppvxidi){
			continue;
		}

		pcsr = VXIBASE(la);

		wd = pcsr->dir.r.status;

		if(VXIPASSEDSTATUS(wd)){
			(*ppvxidi)->st_passed = TRUE;	
		}
		else{
			errMessage(
				S_epvxi_selfTestFailed,
				"VXI resman: device self test failed");
			epvxiSetDeviceOffline(la);
		}
	}

	return VXI_SUCCESS;
}


/*
 *
 *
 * epvxiSetDeviceOffline()
 *
 */
LOCAL EPVXISTAT epvxiSetDeviceOffline(
unsigned 	la
)
{
	struct vxi_csr		*pcsr;

	pcsr = VXIBASE(la);

	errPrintf(	
		S_epvxi_badConfig,
		__FILE__,
		__LINE__,
		"WARNING: VXI device placed off line %c(LA=0x%X)",
		BELL,
		la);

	pcsr->dir.w.control = VXISAFECONTROL;

	return VXI_SUCCESS;
}


/*
 *
 *	VXI_ADDRESS_CONFIG
 *
 */
LOCAL void vxi_address_config(void)
{
	char		*pBase;
	EPVXISTAT	status;

	/*
	 * fetch the EPICS address ranges from the global
	 * symbol table if they are available
	 */
	status = symbol_value_fetch(
			"_EPICS_VXI_A24_BASE", 
			&root_extender.A24_base, 
			sizeof(root_extender.A24_base));
	if(status){
		root_extender.A24_base = DEFAULT_VXI_A24_BASE;
	}
	status = symbol_value_fetch(
			"_EPICS_VXI_A24_SIZE", 
			&root_extender.A24_size, 
			sizeof(root_extender.A24_size));
	if(status){
		root_extender.A24_size = DEFAULT_VXI_A24_SIZE;
	}
	status = symbol_value_fetch(
			"_EPICS_VXI_A32_BASE", 
			&root_extender.A32_base, 
			sizeof(root_extender.A32_base));
	if(status){
		root_extender.A32_base = DEFAULT_VXI_A32_BASE;
	}
	status = symbol_value_fetch(
			"_EPICS_VXI_A32_SIZE", 
			&root_extender.A32_size, 
			sizeof(root_extender.A32_size));
	if(status){
		root_extender.A32_size = DEFAULT_VXI_A32_SIZE;
	}
	
        /* 
 	 * find A24 and A32 on this processor
	 */
	status = sysBusToLocalAdrs(
                                VME_AM_STD_SUP_DATA,
                                (char *)root_extender.A24_base,
                                &pBase);
	if(status == OK){
		root_extender.A24_base = (long) pBase;
		root_extender.A24_ok = TRUE;
	}
	else{
		root_extender.A24_ok = FALSE;
                errMessage(	
			S_epvxi_badConfig,
			"A24 VXI Base Addr problems");
        }
	status = sysBusToLocalAdrs(
                                VME_AM_EXT_SUP_DATA,
                                (char *)root_extender.A32_base,
                                &pBase);
	if(status == OK){
		root_extender.A32_base = (long) pBase;
		root_extender.A32_ok = TRUE;
	}
	else{
		root_extender.A32_ok = FALSE;
                errMessage(	
			S_epvxi_badConfig,
			"A32 VXI Base Addr problems");
        }

	vxi_allocate_address_block(&root_extender);
}


/*
 *
 *	VXI_ALLOCATE_ADDRESS_BLOCK	
 *
 */
LOCAL void vxi_allocate_address_block(
VXIE	*pvxie
)
{
	unsigned 		la;
	struct vxi_csr		*pcsr;
	VXIDI			**ppvxidi;
	VXIE			*psubvxie;
	unsigned long		A24_base;
	unsigned long		A24_size;
	unsigned long		A32_base;
	unsigned long		A32_size;

	if(!pvxie->la_mapped){
		return;
	}

	switch(pvxie->type){
	case ext_export_vxi_onto_mxi:
	case ext_import_mxi_into_vxi:
		pvxie->A24_base =  MXIA24ALIGN(pvxie->A24_base);
		pvxie->A32_base =  MXIA32ALIGN(pvxie->A32_base);
		pvxie->A24_size &= ~MXIA24MASK;
		pvxie->A32_size &= ~MXIA32MASK;
		break;
	case ext_local_cpu:
	default:
		break;
	}

	A24_base = pvxie->A24_base;
	A24_size = pvxie->A24_size;
	A32_base = pvxie->A32_base;
	A32_size = pvxie->A32_size;

	psubvxie = (VXIE *) &pvxie->extenders.node;
	while(psubvxie = (VXIE *) ellNext((ELLNODE *)psubvxie)){

		psubvxie->A24_base = A24_base;
		psubvxie->A24_size = A24_size;
		psubvxie->A32_base = A32_base;
		psubvxie->A32_size = A32_size;
		psubvxie->A24_ok = pvxie->A24_ok;
		psubvxie->A32_ok = pvxie->A32_ok;

		vxi_allocate_address_block(psubvxie);

		if(psubvxie->A24_mapped){
			A24_base = psubvxie->A24_base + psubvxie->A24_size;
			A24_size -= psubvxie->A24_size;
			pvxie->A24_mapped = TRUE;
		}

		if(psubvxie->A32_mapped){
			A32_base = psubvxie->A32_base + psubvxie->A32_size;
			A32_size -= psubvxie->A32_size;
			pvxie->A32_mapped = TRUE;
		}
	}

	for(	la=pvxie->la_low, ppvxidi = &epvxiLibDeviceList[la];	
		ppvxidi <= &epvxiLibDeviceList[pvxie->la_high];
		ppvxidi++, la++){

		unsigned long	m;
		unsigned long	size;
		unsigned long	mask;

		if(!*ppvxidi){
			continue;
		}

		/*
		 * dont configure devices lower in the hierarchy
		 */
		if((*ppvxidi)->A24_mapped || (*ppvxidi)->A32_mapped){
			continue;
		}

		pcsr = VXIBASE(la);

		m = VXIREQMEM(pcsr);

		switch(VXIADDRSPACE(pcsr)){
		case VXI_ADDR_EXT_A24:
			if(!pvxie->A24_ok){
				break;
			}

			/*
			 * perform any needed alignment
			 */
			size = VXIA24MEMSIZE(m);
			if(size>A24_size){
				errPrintf(
					S_epvxi_badConfig,
					__FILE__,
					__LINE__,
	"VXI A24 device does not fit Request=%d Avail=%d LA=0X%X",
					size,
					A24_size,
					la);
				epvxiSetDeviceOffline(la);
				break;
			}
			mask = size-1;
			A24_base = ((A24_base)+mask)&(~mask);
			pcsr->dir.w.offset = A24_base>>8;		
			pcsr->dir.w.control = VXIMEMENBLCONTROL;		
			(*ppvxidi)->pFatAddrBase = (void *) A24_base;
			(*ppvxidi)->A24_mapped = TRUE;
			pvxie->A24_mapped = TRUE;
			A24_base += size;
			A24_size -= size;
			break;

		case VXI_ADDR_EXT_A32:
			if(!pvxie->A32_ok){
				break;
			}

			/*
			 * perform any needed alignment
			 */
			size = VXIA32MEMSIZE(m);
			if(size>A32_size){
				errPrintf(
					S_epvxi_badConfig,
					__FILE__,
					__LINE__,
	"VXI A32 device does not fit Request=%d Avail=%d LA=0X%X",
					size,
					A32_size,
					la);
				epvxiSetDeviceOffline(la);
				break;
			}
			mask = size-1;
			A32_base = (A32_base+mask)&(~mask);
			pcsr->dir.w.offset = A32_base>>16;		
			pcsr->dir.w.control = VXIMEMENBLCONTROL;		
			(*ppvxidi)->pFatAddrBase = (void *) A32_base;
			(*ppvxidi)->A32_mapped = TRUE;
			pvxie->A32_mapped = TRUE;
			A32_base += size;
			A32_size -= size;
			break;

		default:
			/*
			 * do nothing
			 */
			break;
		}
	}

	pcsr = VXIBASE(pvxie->la);

	if(pvxie->A24_mapped){
		pvxie->A24_size = pvxie->A24_size - A24_size;
		pvxie->A24_size = MXIA24ALIGN(pvxie->A24_size);
	}

	if(pvxie->A32_mapped){
		pvxie->A32_size = pvxie->A32_size - A32_size;
		pvxie->A32_size = MXIA32ALIGN(pvxie->A32_size);
	}

	switch(pvxie->type){
	case ext_export_vxi_onto_mxi:
		if(pvxie->A24_mapped){
			/*
			 * window enables only after the low 
			 * byte is written
			 */
			pcsr->dir.w.dd.mxi.a24_window_high = 
				(pvxie->A24_base+pvxie->A24_size) 
					>> MXIA24MASKSIZE;
			pcsr->dir.w.dd.mxi.a24_window_low = 
				pvxie->A24_base 
					>> MXIA24MASKSIZE;	
		}
		if(pvxie->A32_mapped){
			/*
			 * window enables only after the low 
			 * byte is written
			 */
			pcsr->dir.w.dd.mxi.a32_window_high = 
				(pvxie->A32_base+pvxie->A32_size) 
					>> MXIA32MASKSIZE;
			pcsr->dir.w.dd.mxi.a32_window_low = 
				pvxie->A32_base	
					>> MXIA32MASKSIZE;
		}
		break;

	case ext_import_mxi_into_vxi:
		if(pvxie->A24_mapped){
			/*
			 * window enables only after the low 
			 * byte is written
			 */
			pcsr->dir.w.dd.mxi.a24_window_high = 
				pvxie->A24_base 
					>> MXIA24MASKSIZE;	
			pcsr->dir.w.dd.mxi.a24_window_low = 
				(pvxie->A24_base+pvxie->A24_size) 
					>> MXIA24MASKSIZE;
		}
		if(pvxie->A32_mapped){
			/*
			 * window enables only after the low 
			 * byte is written
			 */
			pcsr->dir.w.dd.mxi.a32_window_high = 
				pvxie->A32_base	
					>> MXIA32MASKSIZE;
			pcsr->dir.w.dd.mxi.a32_window_low = 
				(pvxie->A32_base+pvxie->A32_size) 
					>> MXIA32MASKSIZE;
		}
		break;

	case ext_local_cpu:
	default:
		break;
	}
}


/*
 *
 * symbol_value_fetch
 *
 */
LOCAL EPVXISTAT symbol_value_fetch(
char 		*pname,
void 		*pdest,
unsigned 	dest_size
)
{
	EPVXISTAT	status;
	SYM_TYPE	type;
	char		*pvalue;

	status = symFindByName(
				sysSymTbl,
				pname,	
				&pvalue,
				&type);
	if(status == OK){
		memcpy (pdest, pvalue, dest_size);
		return VXI_SUCCESS;
	}
	else{
		return S_epvxi_internal;
	}
}


/*
 *
 *	VXI_INIT_IGNORE_LIST
 *	init list of interrupt handlers to ignore
 *
 */
LOCAL EPVXISTAT vxi_init_ignore_list(void)
{
  	int 		i;
	SYM_TYPE	type;
	EPVXISTAT	status;

  	for(i=0; i<NELEMENTS(ignore_list); i++){
		status =
		  	symFindByName(	sysSymTbl,
					ignore_list[i],
					(char **) &ignore_addr_list[i],
					&type);
		if(status != OK){
			status = S_epvxi_internal;
			errMessage(
				status,
				"Default int handler not found");
			return status;
		}
	}

	return VXI_SUCCESS;
}


/*
 *
 *	VXI_VEC_INUSE
 *	check to see if vector is in use
 *
 */
LOCAL EPVXISTAT vxi_vec_inuse( 
unsigned	addr
)
{
  	int 	i;
	void	*psub;

	psub = (void *) intVecGet((FUNCPTR *)INUM_TO_IVEC(addr));
  	for(i=0; i<NELEMENTS(ignore_list); i++)
		if(ignore_addr_list[i] == psub)
			return FALSE;

	return TRUE;
}


/*
 *
 *      SET_REG_MODID
 *
 */
LOCAL void set_reg_modid(
VXISZ		*pvxisz,
unsigned	slot
)
{
	if(!(pvxisz->reg)){
		errMessage(
			S_epvxi_internal,
			"bad crate for set_reg_modid");
		return;
	}
	VXI_SET_REG_MODID(pvxisz->pcsr, slot);
}

/*
 *
 *      CLR_ALL_REG_MODID
 *
 */
LOCAL void clr_all_reg_modid(
VXISZ		*pvxisz
)
{
	if(!(pvxisz->reg)){
		errMessage(
			S_epvxi_internal,
			"bad crate for clr_all_reg_modid");
		return;
	}
	VXI_CLR_ALL_REG_MODID(pvxisz->pcsr);
}



/*
 *
 *      NIVXI_CPU030_SET_MODID
 *
 */
#ifdef NICPU030
LOCAL void nivxi_cpu030_set_modid(
VXISZ		*pvxisz,
unsigned        slot
)
{
	EPVXISTAT status;

	if(niCpu030Initialized){
		status = (*pnivxi_func[(unsigned)e_SetMODID])(TRUE,1<<slot);
		if(status<0){
			errMessage(
				S_epvxi_internal,
				"CPU030 set modid failed");
		}
	}
}
#endif

/*
 *
 *      NIVXI_CPU030_CLR_ALL_MODID
 *
 */
#ifdef NICPU030
LOCAL void nivxi_cpu030_clr_all_modid(
VXISZ		*pvxisz
)
{
	EPVXISTAT status;

	if(niCpu030Initialized){
		status = (*pnivxi_func[(unsigned)e_SetMODID])(FALSE,0);
		if(status<0){
			errMessage(
				S_epvxi_internal,
				"CPU030 clr all modid failed");
		}
	}
}
#endif


/*
 *
 * epvxiDeviceList()
 *
 */
EPVXISTAT epvxiDeviceList(void)
{
	int 	i;
	VXIDI	**ppmxidi;
	VXIDI	*pmxidi;
	
	i=0;
	ppmxidi = epvxiLibDeviceList; 
	while(ppmxidi<epvxiLibDeviceList+NELEMENTS(epvxiLibDeviceList)){
		pmxidi = *ppmxidi;
		if(pmxidi){
			printf(	
		"Logical Address 0x%02X Slot %2d SZ LA 0x%02X Class 0x%02X\n", 
				i,
				pmxidi->slot,
				pmxidi->pvxisz->la,
				pmxidi->class);
			printf("\t");
			if(pmxidi->pvxieSelf){
				printf("extender, ");
			}
			if(pmxidi->msg_dev_online){
				printf("msg online, ");
			}
			printf("driver ID %d, ", pmxidi->driverID);
			if(taskIdVerify(pmxidi->taskID)>=0){
				printf(	"opened by task %s, ", 
					taskName(pmxidi->taskID));
			}
			printf("cmdr la=0x%X, ", pmxidi->commander_la);
			printf("extdr la=0x%X, ", pmxidi->extender_la);
			printf("slot-zero la=0x%X, ", pmxidi->slot_zero_la);
			printf("make 0X%X, ", (unsigned) pmxidi->make);
			printf("model 0x%X, ", pmxidi->model);
			printf(	"pio_report_func %x, ", 
				(unsigned) pmxidi->pio_report_func);
			printf("\n");
		}
		i++;
		ppmxidi++;
	}

	return VXI_SUCCESS;
}


/*
 *
 * epvxiCrateList()
 *
 */
EPVXISTAT epvxiCrateList(void)
{
	VXISZ	*pvxisz;

	printf("VXI crate list\n");
	pvxisz = (VXISZ *) crateList.node.next;
	while(pvxisz){

		printf("LA=0X%X", pvxisz->la);
		printf(	", extender LA=0X%X", 
			pvxisz->pvxie->la);
		if(pvxisz->reg){
			printf(", register device");
		}
		if(pvxisz->msg){
			printf(", message device");
		}
		if(pvxisz->nicpu030){
			printf(", NI030");
		}
		printf("\n");
		pvxisz = (VXISZ *) pvxisz->node.next;
	}

	return VXI_SUCCESS;
}


/*
 *
 * epvxiUniqueDriverID()
 *
 * return a non zero unique id for a VXI driver
 */
long epvxiUniqueDriverID(void)
{
	if(epvxiNextDriverID<UNINITIALIZED_DRIVER_ID){
		return epvxiNextDriverID++;
	}
	else{
		return UNINITIALIZED_DRIVER_ID;
	}
}


/*
 *
 * epvxiOpen()
 *
 * 1)	Register the driver which will own a VXI device
 * 2)	Verify that the resource manager has run
 * 3) 	Verify that the device has passed its self test
 * 4)   Allocate a configuration block for the dev of size driverConfigSize
 * 5) 	Install an optional io report function
 *
 */
EPVXISTAT epvxiOpen(
unsigned	la,
int		vxiDriverID,
unsigned long	driverConfigSize,
void		(*pio_report_func)()
)
{
	VXIDI		*pvxidi;
	void		*pconfig;
	EPVXISTAT	status;


	if(vxiDriverID == UNINITIALIZED_DRIVER_ID){
		return S_epvxi_notOwner;
	}

	status = epvxiDeviceVerify(la);
	if(status){
		return status;
	}

	pvxidi = epvxiLibDeviceList[la];
	
	if(pvxidi->driverID == vxiDriverID){
		return S_epvxi_deviceOpen;
	}
	else if(pvxidi->driverID != NO_DRIVER_ATTACHED_ID){
		return S_epvxi_notOwner;
	}

	if(driverConfigSize){
		pconfig = (void *)calloc(1,driverConfigSize);
		if(!pconfig){
			return S_epvxi_noMemory;
        	}
		pvxidi->pDriverConfig = pconfig;
	}
	else{
		pvxidi->pDriverConfig = NULL;
	}

	pvxidi->pio_report_func = pio_report_func;

	pvxidi->taskID = taskIdSelf();
	pvxidi->driverID = vxiDriverID;

	return VXI_SUCCESS;
}


/*
 *
 * epvxiDeviceVerify()
 *
 *
 */
EPVXISTAT epvxiDeviceVerify(unsigned la)
{
	EPVXISTAT	status;
	VXICSR		*pcsr;
	VXIDI		*pvxidi;
	uint16_t	device_status;

	if(la > NELEMENTS(epvxiLibDeviceList)){
		return S_epvxi_badLA;
	}

	pvxidi = epvxiLibDeviceList[la];
	if(!pvxidi){
		return S_epvxi_uknDevice;
	}

	/*
	 * verify that the device exists
	 * and check the self test in memory
	 * since this may run before
	 * the self tests are verified.
	 */
	pcsr = VXIBASE(la);
	status = vxMemProbe(    (char *)&pcsr->dir.r.status,
				READ,
				sizeof(pcsr->dir.r.status),
				(char *)&device_status);
	if(status != OK){
		return S_epvxi_uknDevice;
	}
	if(!VXIPASSEDSTATUS(device_status)){
		return S_epvxi_selfTestFailed;
	}	

	return VXI_SUCCESS;
}


/*
 *
 * epvxiClose()
 *
 * 1)	Unregister a driver's ownership of a device
 * 2) 	Free driver's configuration block if one is allocated
 */
EPVXISTAT epvxiClose(
unsigned        la,
int		vxiDriverID
)
{
        VXIDI   *pvxidi;

	if(la > NELEMENTS(epvxiLibDeviceList)){
		return S_epvxi_badLA;
	}

        pvxidi = epvxiLibDeviceList[la];

        if(pvxidi){
                if(pvxidi->driverID == vxiDriverID){
			pvxidi->driverID = NO_DRIVER_ATTACHED_ID;
			if(pvxidi->pDriverConfig){
				free(pvxidi->pDriverConfig);
				pvxidi->pDriverConfig = NULL;
			}
                        return VXI_SUCCESS;
                }
                return S_epvxi_notOwner;
        }

	return S_epvxi_notOwner;	
}


/*
 *
 * epvxiLookupLA()
 *
 */
EPVXISTAT epvxiLookupLA(
epvxiDeviceSearchPattern	*pdsp,
void				(*pfunc)(),
void				*parg 
)
{
	VXIDI			*plac;
	unsigned		i;

	for(i=first_la; i<=last_la; i++){
		long	flags;

		flags = pdsp->flags;
		plac = epvxiLibDeviceList[i];

		/*
		 * skip devices not present
		 */
		if(!plac){
			continue;
		}

		if(flags & VXI_DSP_make){
			if(plac->make != pdsp->make){
				continue;
			}
		}	

		if(flags & VXI_DSP_model){
			if(plac->model != pdsp->model){
				continue;
			}
		}

                if(flags & VXI_DSP_class){
                        if(plac->class != pdsp->class){
                                continue;
                        }
                }

                if(flags & VXI_DSP_slot){
                        if(plac->slot != pdsp->slot){
                                continue;
                        }
                }

                if(flags & VXI_DSP_slot_zero_la){
                        if(plac->slot_zero_la != pdsp->slot_zero_la){
                                continue;
                        }
                }

                if(flags & VXI_DSP_commander_la){
                        if(plac->commander_la != pdsp->commander_la){
                                continue;
                        }
		}

                if(flags & VXI_DSP_extender_la){
                        if(plac->extender_la != pdsp->extender_la){
                                continue;
                        }
		}

		(*pfunc)(i, parg);

	}

        return VXI_SUCCESS;
}


/*
 * epvxiRouteTriggerECL()
 */
EPVXISTAT epvxiRouteTriggerECL(
unsigned        la,             /* slot zero device logical address     */
unsigned	enable_map,	/* bits 0-5  correspond to trig 0-5	*/
				/* a 1 enables a trigger		*/
				/* a 0 disables	a trigger		*/ 
unsigned	io_map		/* bits 0-5  correspond to trig 0-5	*/
				/* a 1 sources the front panel		*/ 
				/* a 0 sources the back plane		*/ 
)
{
	VXIDI			*plac;
        struct vxi_csr          *pcsr;
	char			mask;
	EPVXISTAT		status;
	int			i;

	mask = (1<<VXI_N_ECL_TRIGGERS)-1;
	if((io_map|enable_map) & ~mask){
		return S_epvxi_badTrigger;
	}

	/*
	 * CPU030 trigger routing
	 */
	if(	niCpu030Initialized &&
		pnivxi_func[(unsigned)e_MapTrigToTrig] && 
		pnivxi_func[(unsigned)e_GetMyLA]){
		if(la == (*pnivxi_func[(unsigned)e_GetMyLA])()){
			
			for(	i=0;
				i<VXI_N_ECL_TRIGGERS;
				enable_map = enable_map >> 1,
					io_map = io_map >> 1,
					i++){

				int	(*pfunc)();
				int	src;
				int	dest;
				
				if(!(enable_map&1)){
					continue;
				}

				if(io_map&1){
					src = TRIG_LINE_FPOUT;
					dest = TRIG_LINE_ECL_BASE + i;
				}
				else{
					src = TRIG_LINE_FPIN;
					dest = TRIG_LINE_ECL_BASE + i;
				}

				pfunc = pnivxi_func[(unsigned)e_MapTrigToTrig];
	 			status = (*pfunc)(
						la, 
						src, 
						dest,
						0);
				if(status < 0){
					status = S_epvxi_badTrigIO;
					errPrintf(
						status,
						__FILE__,
						__LINE__,
				"NI CPU030 ECL trig map fail LA=0X%X", 
						la);
					return status;
				}	

			}
                	return VXI_SUCCESS;
		}
	}

	plac = epvxiLibDeviceList[la];
	if(plac){
		if(!plac->st_passed){
			return S_epvxi_selfTestFailed;
		}
	}
	else{
		return S_epvxi_noDevice;
	}

        pcsr = VXIBASE(la);

	if(VXIMXI(pcsr)){
		int ctrl = MXI_CONTROL_CONSTANT;

		if(enable_map & (1<<0)){
			ctrl |= MXI_ECL0_ENABLE;
		}
		if(io_map & (1<<0)){
			ctrl |= MXI_ECL0_BP_TO_FP;
		}
		else{
			ctrl |= MXI_ECL0_FP_TO_BP;
		}


                if(enable_map & (1<<1)){
                        ctrl |= MXI_ECL1_ENABLE;
                }
                if(io_map & (1<<1)){
                        ctrl |= MXI_ECL1_BP_TO_FP;
                }
                else{
                        ctrl |= MXI_ECL1_FP_TO_BP;
                }

		pcsr->dir.w.dd.mxi.control = ctrl;

		return VXI_SUCCESS;
	}

	/*
	 * HP MODEL E1404 trigger routing 
	 */
	if(VXIMAKE(pcsr)==VXI_MAKE_HP){ 
		if(	VXIMODEL(pcsr)==VXI_HP_MODEL_E1404_REG || 
			VXIMODEL(pcsr)==VXI_HP_MODEL_E1404_REG_SLOT0){
                	return hpE1404RouteTriggerECL(
				la, 
				enable_map, 
				io_map);
		}
	}

	status = S_epvxi_uknDevice;
	errPrintf(
		status,
		__FILE__,
		__LINE__,
		"failed to map ECL trigger for (la=0x%X)", 
		la);
        return status;
}


/*
 * epvxiRouteTriggerTTL()
 *
 */
EPVXISTAT epvxiRouteTriggerTTL(
unsigned        la,             /* slot zero device logical address     */
unsigned	enable_map,	/* bits 0-5  correspond to trig 0-5	*/
				/* a 1 enables a trigger		*/
				/* a 0 disables	a trigger		*/ 
unsigned	io_map		/* bits 0-5  correspond to trig 0-5	*/
				/* a 1 sources the front panel		*/ 
				/* a 0 sources the back plane		*/ 
)
{
	VXIDI			*plac;
        struct vxi_csr          *pcsr;
	unsigned		mask;
	EPVXISTAT		status;
	int			i;

	mask = (1<<VXI_N_TTL_TRIGGERS)-1;
	if((io_map|enable_map) & ~mask){
		return S_epvxi_badTrigger;
	}

	/*
	 * NI CPU030 trigger routing 
	 */
	if(	niCpu030Initialized &&
		pnivxi_func[(unsigned)e_MapTrigToTrig] && 
		pnivxi_func[(unsigned)e_GetMyLA]){
		if(la == (*pnivxi_func[(unsigned)e_GetMyLA])()){
			
			for(	i=0;
				i<VXI_N_TTL_TRIGGERS;
				enable_map = enable_map >> 1,
					io_map = io_map >> 1,
					i++){

				int	(*pfunc)();
				int	src;
				int	dest;
				
				if(!(enable_map&1)){
					continue;
				}

				if(io_map&1){
					src = TRIG_LINE_FPOUT;
					dest = TRIG_LINE_TTL_BASE + i;
				}
				else{
					src = TRIG_LINE_FPIN;
					dest = TRIG_LINE_TTL_BASE + i;
				}

				pfunc = pnivxi_func[(unsigned)e_MapTrigToTrig];
	 			status = (*pfunc)(
						la, 
						src, 
						dest,
						0);
				if(status < 0){
					status = S_epvxi_badTrigIO;
					errPrintf(	
						status,
						__FILE__,
						__LINE__,
				"NI030 TTL trig map fail LA=0X%X", 
						la);
					return status;
				}	

			}
                	return VXI_SUCCESS;
		}
	}

	plac = epvxiLibDeviceList[la];
	if(plac){
		if(!plac->st_passed){
			return S_epvxi_selfTestFailed;
		}
	}
	else{
		return S_epvxi_noDevice;
	}


        pcsr = VXIBASE(la);

	if(VXIMXI(pcsr)){
		int16_t	tmp;

		tmp = ~io_map & enable_map;
		tmp = (enable_map<<8) | tmp;
		pcsr->dir.w.dd.mxi.trigger_config = tmp;
	
		return VXI_SUCCESS;
	}

	/*
	 * HP MODEL E1404 trigger routing 
	 */
	if(VXIMAKE(pcsr)==VXI_MAKE_HP){ 
		if(	VXIMODEL(pcsr)==VXI_HP_MODEL_E1404_REG ||
			VXIMODEL(pcsr)==VXI_HP_MODEL_E1404_REG_SLOT0){
                	return hpE1404RouteTriggerTTL(
					la, 	
					enable_map, 
					io_map);
		}
	}

	status = S_epvxi_uknDevice;
	errPrintf(
		status,
		__FILE__,
		__LINE__,
		"Failed to map TTL trigger for (LA=%x%X)", 
		la);
        return status;
}


/*
 * 	vxi_io_report()
 */
EPVXISTAT vxi_io_report(
unsigned	level
)
{
	return epvxiIOReport(level);
}


/*
 *
 *     epvxiIOReport 
 *
 *      call io report routines for all registered devices
 *
 */
EPVXISTAT epvxiIOReport(
unsigned 	level 
)
{
	unsigned	la;
        unsigned	resmanLA;
        EPVXISTAT	status;

        /* Get local address from VME address. */
	/* in case the resource manager has not been called */
	if(!epvxi_local_base){
		status = sysBusToLocalAdrs(
                                VME_AM_SUP_SHORT_IO,
                                (char *)VXIBASEADDR,
                                (char **)&epvxi_local_base);
		if(status != OK){
			status = S_epvxi_badConfig;
			errMessage(
				status,
				"A16 base map failed");
			return status;
		}
	}

	/*
	 * special support for the niCPU030
	 * since it does not see itself
	 */
	nicpu030_init(&root_extender);

	if(niCpu030Initialized){
		if(pnivxi_func[(unsigned)e_GetMyLA]){
			resmanLA = (*pnivxi_func[(unsigned)e_GetMyLA])();
			printf("VXI LA 0x%02X ", resmanLA);		
		}
	}
	else{
		printf("VXI LA      ");		
	}

	printf("%s resident resource manager\n",
		sysModel());

        for(la=first_la; la<=last_la; la++){
		report_one_device(la, level);
        }

        return VXI_SUCCESS;
}


/*
 *
 *	report_one_device()
 *
 */
LOCAL EPVXISTAT report_one_device(
unsigned	la,
int		level 
)
{
	VXIDI			*plac;
	VXISZ			*pvxisz;
        unsigned		slot;
	EPVXISTAT		status;
	int			make;
	int			model;
        struct vxi_csr          *pcsr;
        int16_t                 id;

	pcsr = VXIBASE(la);
	status = vxMemProbe(    (char *)pcsr,
				READ,
				sizeof(id),
				(char *)&id);
	if(status != OK){
		return S_epvxi_internal;
	}

	status = vxi_find_slot(pcsr, &slot, &pvxisz);
	if(status){
		pvxisz = NULL;
		slot = UKN_SLOT;
	}


	/*
	 * the logical address
	 */
	printf("VXI LA 0x%02X ", la);

	/*
	 * crate and slot
	 */
	if(pvxisz){
		printf(	"slot zero LA=0X%02X slot %2d ",
			pvxisz->la,
			slot);
	}
	else{
		printf(	"slot zero LA=?? slot=?? ");
	}


	/*
	 * make
	 */
	make = VXIMAKE(pcsr);
	{
		char		buf[32];
		unsigned 	nactual;

		status = epuxiLookupMakeName(
				make,
				buf,
				sizeof(buf)-1,
				&nactual);		
		if(status==VXI_SUCCESS){
			buf[sizeof(buf)-1] = NULL;
               		printf("%s ", buf);
		}
		else{
               		printf("make 0x%03X ", make);
		}
	}

	/*
	 * model
	 */
	model = VXIMODEL(pcsr);
	{
		char		model_name[32];
		unsigned int	nread;	

		status = epuxiLookupModelName(
				make, 
				model, 
				model_name, 
				sizeof(model_name)-1, 
				&nread);
		if(status){
			printf(	"model 0x%03X ", model);
		}
		else{
			model_name[sizeof(model_name)]=NULL;
			printf(	"%s ", model_name);
		}
	}

	printf("\n");

	if(!VXIPASSEDSTATUS(pcsr->dir.r.status)){
		printf("\t---- Self Test Failed ----\n");
		return VXI_SUCCESS;
	}

	/*
	 * call their io report routine if they supply one
 	 */
	plac = epvxiLibDeviceList[la];
	if(plac){
		if(plac->pio_report_func){
			(*plac->pio_report_func)(la, level);
		}
	}

	if(level == 0){
		return VXI_SUCCESS;
	}

	/*
 	 * print out physical addresses of the cards
	 */
	printf("\tA16=0x%X ", (int) VXIBASE(la));
	if(VXIMEMENBL(pcsr)){
		long	VMEmod = NULL;
		char	*VMEaddr = NULL;
		char	*pname = NULL;
		char	*pbase;

		switch(VXIADDRSPACE(pcsr)){
		case VXI_ADDR_EXT_A24:
			VMEmod = VME_AM_STD_SUP_DATA;
			VMEaddr = (char *) (pcsr->dir.w.offset<<8);
			pname = "A24";
			break;
		case VXI_ADDR_EXT_A32:
			VMEmod = VME_AM_EXT_SUP_DATA;	 
			VMEaddr = (char *) (pcsr->dir.w.offset<<16);
			pname = "A32";
			break;
		}
		if(pname){
			status = sysBusToLocalAdrs(
					VMEmod,
					VMEaddr,
					&pbase);
			if(status>=0){
				printf(	"%s=0x%X", 
					pname, 
					(unsigned)pbase);
			}
			else{
				printf(	"failure mapping %s=%x", 
					pname, 	
					(unsigned)VMEaddr);
			}
		}
	}
	printf("\n");

	if(VXISLOT0MODEL(pcsr)){
		printf("\tSlot Zero Device\n");
	}

	printf( "\t%s device",
		vxi_device_class_names[VXICLASS(pcsr)]);
	switch(VXICLASS(pcsr)){
	case VXI_MEMORY_DEVICE:
		break;

	case VXI_EXTENDED_DEVICE:
		if(VXIMXI(pcsr)){
			mxi_io_report(pcsr, level);
		}
		break;

	case VXI_MESSAGE_DEVICE:
	{
		unsigned long	resp;

		if(VXICMDR(pcsr)){
			printf(", cmdr");
		}
		if(VXIFHS(pcsr)){
			printf(", fh");
		}
		if(VXISHM(pcsr)){
			printf(", shm");
		}
		if(VXIMBINT(pcsr)){
			printf(", interrupter");
		}
		if(VXIVMEBM(pcsr)){
			printf(", VME bus master");
		}
		if(VXISIGREG(pcsr)){
			printf(", has signal reg");
		}
		printf("\n");
		/*
	  	 * all message based devices are required to
		 * implement this command query 
		 */
		status = epvxiCmdQuery(
				la, 
				(unsigned long)MBC_READ_PROTOCOL, 
				&resp);
		if(status==VXI_SUCCESS){
			printf("\tprotocols(");
			if(MBR_REV_12(resp)){
				printf("Rev 1.2 device, ");
			}
			if(MBR_RP_LW(resp)){
				printf("long word serial, ");
			}
			if(MBR_RP_ELW(resp)){
				printf("extended long word serial, ");
			}
			if(MBR_RP_I(resp)){
				printf("VXI instr, ");
			}
			if(MBR_RP_I4(resp)){
				printf("488 instr, ");
			}
			if(MBR_RP_TRG(resp)){
				printf("sft trig, ");
			}
			if(MBR_RP_PH(resp)){
				printf("prog int hdlr, ");
			}
			if(MBR_RP_PI(resp)){
				printf("prog interrupter, ");
			}
			if(MBR_RP_EG(resp)){
				printf("event gen, ");
			}
			if(MBR_RP_RG(resp)){
				printf("resp gen, ");
			}
			printf(")");
		}
               	break;
	}
	case VXI_REGISTER_DEVICE:
		break;

	}
	printf("\n");

	return VXI_SUCCESS;
}


/*
 *
 *	mxi_io_report()
 *
 *
 */
LOCAL void mxi_io_report(
struct vxi_csr	*pmxi,
int 		level 
)
{
	unsigned 	la;
	unsigned 	ha;
	unsigned	a;
	unsigned	b;
	char		*msg;

        printf(", MXI sub class\n\t");

	if(pmxi->dir.w.dd.mxi.control & MXI_UPPER_LOWER_BOUNDS){
		la = VXIADDRMASK & 
			pmxi->dir.w.dd.mxi.la_window;
		ha = VXIADDRMASK & 
			(pmxi->dir.w.dd.mxi.la_window>>NVXIADDRBITS);
		if(la<ha){
			msg = "local VXI LA's seen by MXI";
			/*
			 * ha > 0 since la < ha
			 */
			ha--;
		}		
		else if(la>ha){
			msg = "MXI LA's seen by this crate";
			/*
			 * la > 0 since la > ha
			 */
			la--;
		}
		else if(la == 0){
			msg = "LA window disabled";
			la = 0;
			ha = 0;
		}
		else if(la >= 0x80){
			msg = "MXI LA's seen by this crate";
			la = 0;
			ha = 0xff;
		}
		else{
			msg = "local VXI LA's seen by MXI";
			la = 0;
			ha = 0xff;
		}

		printf(", %s 0x%X-0x%X\n\t", 
			msg,
			la,
			ha);

		a = pmxi->dir.w.dd.mxi.a24_window_low;
		b = pmxi->dir.w.dd.mxi.a24_window_high;
		printf(", A24 window 0x%X-0x%X", 
			a,
			b);

		a = pmxi->dir.w.dd.mxi.a32_window_low;
		b = pmxi->dir.w.dd.mxi.a32_window_high;
		printf(", A32 window 0x%X-0x%X", 
			a,
			b);
	}
#	ifdef BASE_PLUS_SIZE_MXI_SUPPORT
	else{
		la = VXIADDRMASK & 
			pmxi->dir.w.dd.mxi.la_window;
		ha = la + (MXI_LA_WINDOW_SIZE_MASK & 
			(pmxi->dir.w.dd.mxi.la_window>>NVXIADDRBITS));

		printf(", LA window 0x%X-0x%X", 
			la,
			ha);
	}
#	endif
}


/*
 *
 *	vxi_allocate_int_lines()
 *
 *
 */
LOCAL void vxi_allocate_int_lines(void)
{
        EPVXISTAT		status;
        struct vxi_csr		*pcsr;
	VXIDI			**ppvxidi;
	VXIDI			*pvxidi;
	unsigned		la;
	unsigned long		resp;
	unsigned long		cmd;
	unsigned        	line_count;

	for(	la=0, ppvxidi = epvxiLibDeviceList;
		ppvxidi < epvxiLibDeviceList+NELEMENTS(epvxiLibDeviceList);
		ppvxidi++, la++){

		pvxidi = *ppvxidi;

		if(!pvxidi){
			continue;
		}

                pcsr = VXIBASE(la);

                if(VXICLASS(pcsr) != VXI_MESSAGE_DEVICE){
                        continue;
                }

		/*
		 * find out if this is a programmable interrupter
		 */
		status = epvxiCmdQuery(
				la,
				(unsigned long)MBC_READ_PROTOCOL,
				&resp);
		if(status){
			errPrintf(
				status,
				__FILE__,
				__LINE__,
				"Device rejected READ_PROTOCOL (LA=0x%X)",
				la);
			continue;
		}
		if(!MBR_RP_PI(resp)){
			continue;
		}

		printf("Programming interrupter (LA=0x%X)\n", la);

		cmd = MBC_READ_INTERRUPTERS;
		status = epvxiCmdQuery(
				la,
				cmd,
				&resp);
		if(status){
			errPrintf( 
				status,
				__FILE__,
				__LINE__,
			"Device rejected READ_INTERRUPTERS (LA=0x%X)",
				la);
			continue;
		}
		line_count = resp&MBR_READ_INTERRUPTERS_MASK;
		while(line_count--){
			cmd = 	MBC_ASSIGN_INTERRUPTER_LINE |
				((line_count+1)<<4) |
				VXIMSGINTLEVEL;
			sysIntEnable(VXIMSGINTLEVEL);
			status = epvxiCmdQuery(
					la,
					cmd,
					&resp);
			if(status){
				errPrintf(
					status,
					__FILE__,
					__LINE__,
					"Device rejected ASSIGN_INT(LA=0x%X)",
					la);
				continue;
			}
			if(MBR_STATUS(resp) != MBR_STATUS_SUCCESS){
				errPrintf( 
					S_epvxi_msgDeviceFailure,
					__FILE__,
					__LINE__,
					"ASSIGN_INT failed (LA=0x%X)",
					la);
				continue;
			}
		}
	}
}




/*
 *
 * epvxiSymbolTableInit()
 * (written by Richard Baker LANL summer intern)
 *
 */
LOCAL EPVXISTAT epvxiSymbolTableInit(void)
{

   	epvxiSymbolTable = symTblCreate(
				EPVXI_MAX_SYMBOLS_LOG2,
				FALSE,
				memSysPartId);
	if(!epvxiSymbolTable){
		return S_epvxi_noMemory;
	}

	return VXI_SUCCESS;
}


/*
 *
 * epuxiRegisterModelName()
 * (written by Richard Baker LANL summer intern)
 *
 *
 */
EPVXISTAT epvxiRegisterModelName(
unsigned int make, 
unsigned int model, 
char *pmodel_name
)
{ 
 	char 		name[EPVXI_MAX_SYMBOL_LENGTH];
 	char 		*pcopy;
	EPVXISTAT	status;

 	if(!epvxiSymbolTable){   /* initialize table at 1st call */
		status = epvxiSymbolTableInit();
		if(status){
			return status;
		}
 	}

 	sprintf(name, epvxiSymbolTableDeviceIdString, make,model);
 	pcopy = (char *) malloc(strlen(pmodel_name)+1);
 	if(pcopy == NULL){
   		return S_epvxi_noMemory;
	}

 	strcpy(pcopy, pmodel_name);

 	status = symAdd(
			epvxiSymbolTable, 
			name, 
			pcopy, 
			EPVXI_MODEL_NAME_SYMBOL,
			NULL);
	if(status < 0){
		char 		*pold_model_name;
		SYM_TYPE	type;

		free(pcopy);

		status = symFindByNameAndType(
				epvxiSymbolTable, 
				name, 
				&pold_model_name, 
				&type,
				EPVXI_MODEL_NAME_SYMBOL,
				~0);
		if(status<0){
			return S_epvxi_noMemory;
		}
		else if(strcmp(pmodel_name, pold_model_name)){
			return S_epvxi_nameMismatch;
		}
	}

	return VXI_SUCCESS;
}


/*
 *
 * epvxiRegisterMakeName()
 *
 *
 */
EPVXISTAT epvxiRegisterMakeName(
unsigned int make, 
char *pmake_name
)
{ 
 	char 		name[EPVXI_MAX_SYMBOL_LENGTH];
 	char 		*pcopy;
	EPVXISTAT	status;

 	if(!epvxiSymbolTable){   /* initialize table at 1st call */
		status = epvxiSymbolTableInit();
		if(status){
			return status;
		}
 	}

 	sprintf(name, epvxiSymbolTableMakeIdString, make);
 	pcopy = (char *) malloc(strlen(pmake_name)+1);
 	if(pcopy == NULL){
   		return S_epvxi_noMemory;
	}

 	strcpy(pcopy, pmake_name);

 	status = symAdd(
			epvxiSymbolTable, 
			name, 
			pcopy, 
			EPVXI_MAKE_NAME_SYMBOL,
			NULL);
	if(status<0){
		char 		*pold_make_name;
		SYM_TYPE	type;

		free(pcopy);

		status = symFindByNameAndType(
				epvxiSymbolTable, 
				name, 
				&pold_make_name, 
				&type,
				EPVXI_MAKE_NAME_SYMBOL,
				~0);
		if(status<0){
			return S_epvxi_noMemory;
		}
		else if(strcmp(pmake_name, pold_make_name)){
			return S_epvxi_nameMismatch;
		}
	}

	return VXI_SUCCESS;
}


/*
 *
 * epuxiLookupMakeName()
 * (written by Richard Baker LANL summer intern)
 *
 */
EPVXISTAT epuxiLookupMakeName(
unsigned int 	make, 		/* VXI manuf. */
char 		*pbuffer,	/* model name return */
unsigned int 	bufsize,	/* size of supplied buf */ 
unsigned int 	*preadcount)	/* n bytes written */
{
 	char 		name[EPVXI_MAX_SYMBOL_LENGTH];
	char 		*pmake_name;
	SYM_TYPE	type;
	EPVXISTAT	status;

 	if(!epvxiSymbolTable){   /* initialize table at 1st call */
		status = epvxiSymbolTableInit();
		if(status){
			return status;
		}
 	}

 	sprintf(name, epvxiSymbolTableMakeIdString, make);
	status = symFindByNameAndType(
			epvxiSymbolTable, 
			name, 
			&pmake_name, 
			&type,
			EPVXI_MAKE_NAME_SYMBOL,
			~0);
	if(status<0){
		return S_epvxi_noMatch;
	}
	if(type != EPVXI_MAKE_NAME_SYMBOL){
		abort(0);
	}
	*preadcount = min(strlen(pmake_name)+1, bufsize);
	strncpy(pbuffer, pmake_name, bufsize);

	return VXI_SUCCESS;
}

 
/*
 *
 * epuxiLookupModelName()
 * (written by Richard Baker LANL summer intern)
 *
 */
EPVXISTAT epuxiLookupModelName(
unsigned int 	make, 		/* VXI manuf. */
unsigned int 	model, 		/* VXI model code */
char 		*pbuffer,	/* model name return */
unsigned int 	bufsize,	/* size of supplied buf */ 
unsigned int 	*preadcount)	/* n bytes written */
{
 	char 		name[EPVXI_MAX_SYMBOL_LENGTH];
	char 		*pmodel_name;
	SYM_TYPE	type;
	EPVXISTAT	status;

 	if(!epvxiSymbolTable){   /* initialize table at 1st call */
		status = epvxiSymbolTableInit();
		if(status){
			return status;
		}
 	}

 	sprintf(name, epvxiSymbolTableDeviceIdString, make, model);
	status = symFindByNameAndType(
			epvxiSymbolTable, 
			name, 
			&pmodel_name, 
			&type,
			EPVXI_MODEL_NAME_SYMBOL,
			~0);
	if(status<0){
		return S_epvxi_noMatch;
	}
	if(type != EPVXI_MODEL_NAME_SYMBOL){
		abort(0);
	}
	*preadcount = min(strlen(pmodel_name)+1, bufsize);
	strncpy(pbuffer, pmodel_name, bufsize);

	return VXI_SUCCESS;
}


/*
 *
 * epvxiExtenderList()
 *
 * list any bus extenders
 */
EPVXISTAT epvxiExtenderList(void)
{
	epvxiExtenderPrint(&root_extender);

	return VXI_SUCCESS;
}



/*
 *
 * epvxiExtenderPrint
 *
 *
 */
LOCAL void epvxiExtenderPrint(VXIE *pvxie)
{
	VXIE	*psubvxie;


	printf(	"%s Extender LA=0x%02X\n", 
		ext_type_name[pvxie->type], 
		pvxie->la);

	if(pvxie->la_mapped){
		printf("\tLA window 0x%02X - 0x%02X\n",
			pvxie->la_low,
			pvxie->la_high);
	}

	if(pvxie->A24_mapped){
		printf("\tA24 window base=0x%08X size=0x%08X\n",
			pvxie->A24_base,
			pvxie->A24_size);
	}

	if(pvxie->A32_mapped){
		printf("\tA32 window base=0x%08X size=0x%08X\n",
			pvxie->A32_base,
			pvxie->A32_size);
	}

	psubvxie = (VXIE *) &pvxie->extenders.node;
	while(psubvxie = (VXIE *) ellNext((ELLNODE *)psubvxie)){
		epvxiExtenderPrint(psubvxie);
	}
}


/*
 *
 * register some common manufacturer names
 * for consistency
 *
 */
LOCAL void epvxiRegisterCommonMakeNames(void)
{
	int 		i;
	EPVXISTAT	status;

	for(i=0; i<NELEMENTS(vxi_vi); i++){
		status = epvxiRegisterMakeName(
			vxi_vi[i].make,
			vxi_vi[i].pvendor_name);
		if(status){
			errPrintf(
				S_epvxi_internal,
				__FILE__,
				__LINE__,
				"Failed to regster make name %s", 
				vxi_vi[i].pvendor_name);
		}
	}
}
