/*
 * routines for the VXI device support and resource management
 *
 *
 *
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
 *
 * To do
 * -----
 *	.01	Should this module prevent two triggers from driving
 *		the same front panel connector at once?
 *	.02	The resource manager sets up a default front
 *		panel trigger routing. Perhaps this should
 *		be left to the application
 *
 *
 * RM unfinished items	
 * -------------------
 *	1. does not handle multiple dc in one slot
 *	2. does not handle blocked address devices
 *	3. does not configure std and ext addr spaces
 *	4. Assigning the hierachy from within a DC res man
 *	   needs to be revisited
 *
 *
 * NOTES 
 * .01	this software was tested with Rev D of the NI MXI bus extenders
 *	-the INTX stuff does not work properly on this rev
 *	-hopefully Rev E will solve the problem
 *
 *
 */

/*
 * 	Code Portions
 *
 * local
 *	vxi_find_slot0		Find the slot 0 module if present
 *	vxi_find_offset		find a free space for n devices
 *	vxi_find_slot		given a VXI modules addr find its slot
 *	vxi_self_test		resrc manager self test functions
 *	vxi_init_ignore_list	init list of interrupt handlers to ignore
 *	vxi_vec_inuse		check to see if vector is in use
 *	vxi_configure_hierarchies	setup commander servant hierarchies
 *	vxi_self_test		test for dev self test passed
 *	vxi_find_slot0		find (and open) slot zero devices
 *	vxi_find_slot		what slot is that LA in
 *	open_slot0_device	open slot zero devices
 *	nicpu030_slot0_test	is it a NICPU030 slot zero module
 *	nivxi_cpu030_set_modid	set modid on the NICPU030
 *	nivxi_cpu030_clr_all_modid 	clear all modid lines on the NICPU030
 *	set_reg_modid		set modid on a reg based slot0 device	
 *	clr_all_reg_modid	clr all modid on a reg based slot0 dev
 *	vxi_find_sc_devices	find all SC devices and open them
 *	vxi_find_dc_devices	find all DC devices and open them
 *	vxi_init_ignore_list	find addresses of default int handlers
 *	vxi_vec_inuse		test for int vector in use
 *	vxi_find_offset		find LA for a DC device to assume
 *	mxi_map			mat the addresses on a MXI bus extender
 *	vxi_find_mxi_devices	search for and open mxi bus repeaters
 *	map_mxi_inward		map from a VXI crate towards the RM
 *
 * for use by IOC core
 *	epvxiResman		entry for a VXI resource manager which
 *				also sets up the MXI bus
 *	vxi_io_report		call io device specific report routines 
 *				for all registered devices and print 
 *				information about device's configuration
 *	epvxiDeviceList		print info useful when debugging drivers
 *	vxi_init		compatibility
 *
 * for use by vxi drivers
 *	epvxiLookupLA		find LA given search pattern
 *	epvxiUniqueDriverID	obtain a unique id each call
 * 	epvxiOpen			register a drivers use of a device
 *	epvxiClose		disconnect from a device	
 *	epvxiPConfig		fetch a driver config block given a LA
 *	epvxiRouteTriggerECL	route ECL trig to/from front pannel
 *	epvxiRouteTriggerTTL	route TTL trig to/from front pannel
 *
 */

#include <vme.h>
#include <vxWorks.h>
#ifdef V5_vxWorks
#	include <68k/iv.h>
#else
#include <iv68k.h>
#endif
#include <sysSymTbl.h>

#define SRCepvxiLib	/* allocate externals here */
#include <epvxiLib.h>

#define NICPU030

#define VXI_HP_MODEL_E1404		0x110
#define VXI_HP_MODEL_E1404_SLOT0	0x10

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

#define UKN_SLOT (-1)
#define UKN_CRATE (-1)

/*
 * bits for the device type and a pointer
 * to its configuration registers if
 * available (The national Instruments
 * cpu030 does not conform with vxi
 * standard by not making these registers
 * available to programs running on it).
 */
struct slot_zero_device{
	char		present:1;
	char		reg:1;
	char		msg:1;
	char		nicpu030:1;
	struct vxi_csr	*pcsr;
	void		(*set_modid)();
	void		(*clear_modid)();
};
LOCAL struct slot_zero_device	vxislot0[10];

LOCAL char 		*ignore_list[] = {"_excStub","_excIntStub"};
LOCAL void 		*ignore_addr_list[NELEMENTS(ignore_list)];
LOCAL unsigned char	last_la;

#define SETMODID(SLOT, CRATE) \
(*vxislot0[CRATE].set_modid)(CRATE, SLOT)

#define CLRMODID(CRATE) \
(*vxislot0[CRATE].clear_modid)(CRATE);

/* forward references */
int 	vxi_find_slot0();
int 	vxi_find_offset();
int 	vxi_find_slot();
int 	vxi_self_test();
int 	vxi_init_ignore_list();
int 	vxi_vec_inuse();
int	mxi_map();
int	map_mxi_inward();
int	open_vxi_device();
int	vxi_begin_normal_operation();
int	vxi_allocate_int_lines();
void	nicpu030_slot0_test();
void	vxi_find_sc_devices();
void	vxi_find_dc_devices();
void	set_reg_modid();
void	clr_all_reg_modid();
void	nivxi_cpu030_set_modid();
void	nivxi_cpu030_clr_all_modid();
void	open_slot0_device();
void	vxi_configure_hierarchies();
void	vxi_find_mxi_devices();
void	vxi_unmap_mxi_devices();
	

/*
 *
 * vxi_init()
 *
 * for compatibility with past releases
 *
 *
 */
vxi_init()
{
	int status;

	status = epvxiResman(); 
	if(status>=0){
		return OK;
	}
	else{
		return ERROR;
	}
}


/*
 * epvxiResman() 
 *
 * perform vxi resource management functions
 *
 *
 */
int
epvxiResman()
{
        unsigned                crate;
        unsigned        	nd;
        unsigned       		lla;
        unsigned        	hla;
        int             	status;

        /* 
 	 * find out where the VXI LA space is on this processor
	 */
	status = sysBusToLocalAdrs(
                                VME_AM_USR_SHORT_IO,
                                VXIBASEADDR,
                                &epvxi_local_base);
	if(status != OK){
                printf("Addressing error in vxi driver\n");
                return ERROR;
        }

	/*
 	 * clip the EPICS logical address range
	 */
	last_la = min(VXIDYNAMICADDR-1, EPICS_VXI_LA_COUNT-1);
	last_la = min(last_la, NELEMENTS(epvxiLibDeviceList)-1);

	/*
	 * perform this step if you want to reassign LA to DC
	 * devices after a soft reboot
	 */
#       ifdef RESET_DC_DEVICES
                vxi_reset_dc();
#       endif

        if(vxi_init_ignore_list()==ERROR)
                return ERROR;

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
	 * locate SC, DC, and MXI devices
	 */
        mxi_map(VXI_RESMAN_LA, &nd, &lla, &hla);
	if(!nd){
		return OK;
	}

        vxi_self_test();

	vxi_configure_hierarchies(
		VXI_RESMAN_LA, 
		last_la);

	vxi_allocate_int_lines();

	vxi_begin_normal_operation();
/*
 *
 * Triggers used to be hard routed here there
 */	
#ifdef MAP_TRIGGERS
        /*
	 * install a default trigger routing
         */
#	define DEFAULT_TRIG_ENABLE_MAP	1 
#	define DEFAULT_TRIG_IO_MAP	0
        for(crate=0; crate<NELEMENTS(vxislot0); crate++){

		if(!vxislot0[crate].present){
			continue;
		}

		status = epvxiRouteTriggerECL(
				VXI_PA_TO_LA(vxislot0[crate].pcsr), 
				DEFAULT_TRIG_ENABLE_MAP, 
				DEFAULT_TRIG_IO_MAP);
		if(status<0){
			logMsg("vxi resman: trig setup failed (la=%d)\n",
				VXI_PA_TO_LA(vxislot0[crate].pcsr));
		}
	}
#endif

	/*
	 * let the world know that the RM is done
	 * and VXI devices were found
	 */
	epvxiResourceMangerOK = TRUE;

        return OK;
}




/*
 *
 *
 * 	mxi_map()
 *
 *
 */
LOCAL int
mxi_map(base_la, pnd, plla, phla)
unsigned	base_la;
unsigned	*pnd;
unsigned	*plla;
unsigned	*phla;
{
	unsigned	nd;
	unsigned	lla;
	unsigned	hla;
	int		status;

	*phla = 0;
	*plla = last_la;
	*pnd = FALSE;


	/*
	 *
	 * find the slot zero device
	 *
	 */
	status = vxi_find_slot0();
	if(status<0){
		logMsg("VXI slot 0 not present- VXI ignored\n");
		return OK;
	}

	/*
	 *
 	 * find the SC devices
	 *
	 */
	vxi_find_sc_devices(&nd, &lla, &hla);
	if(nd){
		*plla = min(*plla, lla);
		*phla = max(*phla, hla);
		*pnd = TRUE;
	} 	


	/*
 	 *
	 * find the DC devices
	 *
	 */
	vxi_find_dc_devices(*pnd ? lla : base_la, &nd, &lla, &hla);
	if(nd){
		*plla = min(*plla, lla);
		*phla = max(*phla, hla);
		*pnd = TRUE;
	} 	

	
	/*
	 *
	 * find any MXI bus repeaters
	 *
	 */
	vxi_find_mxi_devices(*pnd ? *phla+1 : base_la, &nd, &lla, &hla);
	if(nd){
		*plla = min(*plla, lla);
		*phla = max(*phla, hla);
		*pnd = TRUE;
	} 	

	return OK;
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
LOCAL void
vxi_unmap_mxi_devices()
{
	struct vxi_csr	*pmxi;
	int		status;
	short		id;
	unsigned	addr;

        for(addr=0; addr<=last_la; addr++){
                /*
                 * only configure devices not seen before
                 */
                if(epvxiLibDeviceList[addr]){
                        continue;
                }

		pmxi = VXIBASE(addr);

  		status = vxMemProbe(	pmxi,
					READ,
					sizeof(id),
					&id);
  		if(status<0){
			continue;
		}

		if(!VXIMXI(pmxi)){
			continue;
		}

		pmxi->dir.w.dd.mxi.la_window = 0; 
	}
}



/*
 *
 * vxi_find_mxi_devices()
 *
 */
LOCAL void
vxi_find_mxi_devices(base_la, pnd, plla, phla)
unsigned	base_la;
unsigned	*pnd;
unsigned	*plla;
unsigned	*phla;
{
	struct vxi_csr	*pmxi;
	unsigned	addr;

        for(addr=0; addr<=last_la; addr++){

                /*
                 * only configure devices seen before
                 */
                if(!epvxiLibDeviceList[addr]){
                        continue;
                }

		/*
		 * skip mxi devices seen before
		 */
                if(epvxiLibDeviceList[addr]->mxi_dev){
			continue;
		}

		pmxi = VXIBASE(addr);

		if(!VXIMXI(pmxi)){
			continue;
		}

		/*
		 * this is a MXI device
		 */
                epvxiLibDeviceList[addr]->mxi_dev = TRUE;

		/*
		 * open the address window outward for all device
		 */
		pmxi->dir.w.dd.mxi.control = 
			MXI_UPPER_LOWER_BOUNDS;
		pmxi->dir.w.dd.mxi.la_window = 
			VXIADDRMASK | VXIADDRMASK<<NVXIADDRBITS;

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
		 * for each MXI found that we have not seen before
		 * open the la window inward for all devices
		 */
		map_mxi_inward(base_la, pnd, plla, phla);

		if(*pnd){
			logMsg(	"VXI resman: VXI to MXI(%d) %d-%d\n", 
				addr,
				*plla,
				*phla);
			pmxi->dir.w.dd.mxi.la_window = 
				*plla<<NVXIADDRBITS | *phla+1;
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
			logMsg(	"VXI resman: VXI to MXI(%d) is empty\n",
				addr); 
			pmxi->dir.w.dd.mxi.la_window =
				0 | 0<<NVXIADDRBITS;
		}
	}
}



/*
 *  MAP_MXI_INWARD
 *
 * for each MXI found that we have not seen before
 * open the la window inward for all devices
 *
 */
LOCAL int
map_mxi_inward(base_la, pnd, plla, phla)
unsigned	base_la;
unsigned	*pnd;
unsigned	*plla;
unsigned	*phla;
{
	struct vxi_csr	*pmxi_new;
	unsigned	addr;
	int		status;

       	for(addr=0; addr<=last_la; addr++){

		/*
		 * skip all devices seen before
		 */
               	if(epvxiLibDeviceList[addr]){
			continue;
		}

		status = open_vxi_device(addr);
		if(status < 0){
			continue;
		}

		pmxi_new = VXIBASE(addr);

		if(!VXIMXI(pmxi_new)){
			continue;
		}

                epvxiLibDeviceList[addr]->mxi_dev = TRUE;

		/*
		 * open the address window inward for all device
		 */
		pmxi_new->dir.w.dd.mxi.control = 
			MXI_UPPER_LOWER_BOUNDS;
		pmxi_new->dir.w.dd.mxi.la_window =
			1 | 1<<NVXIADDRBITS;

		mxi_map(base_la, pnd, plla, phla);	
		
		/*
		 * restrict the address window inward to only 
		 * cover devices seen
		 */
		/*
		 * make sure window includes the MXI
		 * bus extender 
		 */
		*plla = min(*plla, addr);
		*phla = max(*phla, addr);

		/*
		 * if INTX is installed gate the interrupts onto
		 * INTX
		 */
		if(MXIINTX(pmxi_new)){
			pmxi_new->dir.w.dd.mxi.INTX_interrupt = 
				INTX_INT_OUT_ENABLE;
		}

		if(*pnd){
			logMsg(	"VXI resman: MXI to VXI(%d) %d-%d\n", 
				addr,
				*plla,
				*phla);
			pmxi_new->dir.w.dd.mxi.la_window = 
				*plla | (*phla+1)<<NVXIADDRBITS;
		}
		else{
			logMsg(	"VXI resman: MXI to VXI(%d) is empty\n"); 
			pmxi_new->dir.w.dd.mxi.la_window =
				0 | 0<<NVXIADDRBITS;
		}
	}
}


/*
 *
 * open_vxi_device
 *
 *
 */
LOCAL int 
open_vxi_device(la)
unsigned	la;
{
	struct vxi_csr		*pdevice;
	VXIDI			*plac;
	short			id;
	int			status;

	/*
	 * just return if this device is known about
	 */
	if(epvxiLibDeviceList[la]){
		return OK;
	}

	pdevice = VXIBASE(la);

  	status = vxMemProbe(	pdevice,
				READ,
				sizeof(id),
				&id);
  	if(status<0){
		return ERROR;
	}

	plac = (VXIDI *) calloc(1, sizeof(**epvxiLibDeviceList));
	if(!plac){
		logMsg("resman: calloc failed ... continuing\n");
		return ERROR;
	}

	plac->make = VXIMAKE(pdevice);
	plac->model = VXIMODEL(pdevice);
	plac->class = VXICLASS(pdevice);
	epvxiLibDeviceList[la] = plac;

	if(vxi_vec_inuse(la)){
		logMsg(	"WARNING: SC VXI device at allocated int vec=%x\n", 
			la);
		logMsg(	"WARNING: VXI device placed off line %c(la=%d)\n",
			BELL,
			la);
		pdevice->dir.w.control = VXISAFECONTROL;
	}
	else{
		 if(VXISLOT0MODELTEST(plac->model)){
			/*
			 * This takes care of MXI devices
			 * that show up after the slot 0 device
			 * search has completed
			 */
			plac->slot0_dev = TRUE;
			open_slot0_device(la);
		}
	}

	{
		unsigned slot, crate;

		status = vxi_find_slot(pdevice, &slot, &crate);
		if(status==OK){
			plac->slot = slot;
			plac->crate = crate;
		}
		else{
			logMsg(	"slot could not be found for LA %d\n", 
				la);
			plac->slot = 0xff;
			plac->crate = 0xff;
		}
	}

	return OK;
}


/*
 *
 *
 *	VXI_FIND_SC_DEVICES
 *
 */
LOCAL void
vxi_find_sc_devices(pnew_device_found, plow_la, phigh_la)
unsigned	*pnew_device_found;
unsigned	*plow_la;
unsigned	*phigh_la;
{
	register unsigned 	addr;
	register		status;

	/*
	 * Locate the slots of all SC devices
	 */
	*plow_la = last_la;
	*phigh_la = 0;
	*pnew_device_found = FALSE;
	for(addr=0; addr<=last_la; addr++){
	
		/*
		 * dont configure devices seen before
		 */
		if(epvxiLibDeviceList[addr]){
			continue;
		}

		status = open_vxi_device(addr);
		if(status < 0){
			continue;
		}

		*pnew_device_found  = TRUE;
		*plow_la = min(*plow_la, addr);
		*phigh_la = max(*phigh_la, addr);

	}
}


/*
 *
 *
 *	VXI_FIND_DC_DEVICES
 *
 */
LOCAL void
vxi_find_dc_devices(base_la, pnew_device_found, plow_la, phigh_la)
unsigned	base_la;
unsigned	*pnew_device_found;
unsigned	*plow_la;
unsigned	*phigh_la;
{
	short			id;
	register		status;
	unsigned		offset;
	struct vxi_csr		*pcsr;
	int			crate;
	int			slot;

	*pnew_device_found = FALSE;
	*phigh_la = 0;
	*plow_la = last_la;

	/*
	 * dont move DC devices if SC device at address 0xff
	 */
	for(crate=0; crate<NELEMENTS(vxislot0); crate++){
		if(!vxislot0[crate].present){
			break;
		}

		CLRMODID(crate);
	}

	pcsr = VXIBASE(VXIDYNAMICADDR);
	status = vxMemProbe(	pcsr,
				READ,
				sizeof(id),
				&id);
  	if(status == OK){
		logMsg("VXI SC device at dynamic address %x\n",pcsr);
		logMsg("VXI DC devices ignored\n");
		return;
	}

	/*
	 * find all dynamic modules
	 */
	for(crate=0; crate<NELEMENTS(vxislot0); crate++){

		if(!vxislot0[crate].present){
			break;
		}

		CLRMODID(crate);

		for(slot=0;slot<NVXISLOTS;slot++){
			SETMODID(slot,crate);
			/*
			 * put some reasonable limit on the number 
			 * of addr which can occupy one slot
			 */
			status = vxMemProbe(	pcsr,
						READ,
						sizeof(id),
						&id);
			if(status<0){
				continue;
			}

			status = vxi_find_offset(base_la, 1, &offset);
			if(status<0){
				logMsg("VXI resman: DC VXI device dosnt fit\n");
				logMsg("VXI resman: DC VXI device ignored\n");
				continue;
			}
			pcsr->dir.w.addr = offset;
			status = open_vxi_device(offset);
			if(status<0){
				logMsg("VXI resman: DC dev failed to DC\n");
				logMsg("VXI resman: DC VXI device ignored\n");
				continue;
			}
			*plow_la = min(*plow_la, offset);
			*phigh_la = max(*phigh_la, offset);
			*pnew_device_found = TRUE;
			epvxiLibDeviceList[offset]->crate = crate;
		}
		CLRMODID(crate);
	}
}


/*
 *
 *
 *	VXI_FIND_SLOT0
 *	Find the slot 0 module if present
 *	(assume that it is a static address)
 *
 */
LOCAL int
vxi_find_slot0()
{
	register int		status;
	register unsigned 	addr;
	struct vxi_csr		*pcsr;
	short			id;
	
#	ifdef NICPU030
		nicpu030_slot0_test();
#	endif

	for(addr=0; addr<=last_la; addr++){

		/*
		 * only add slot zero devices we have not
		 * seen before
		 */
                if(epvxiLibDeviceList[addr]){
			continue;
		}
	
		pcsr = VXIBASE(addr);

	  	status = vxMemProbe(	pcsr,
					READ,
					sizeof(id),
					&id);
  		if(status<0){
			continue;
		}

		/*
		 *	Is it a slot 0 card ?
		 */
		if(!VXISLOT0MODEL(pcsr)){
			continue;
		}
	
		open_slot0_device(addr);

	}
	if(vxislot0[0].present){
		return OK;
	}
	else{
		return ERROR;
	}
}


/*
 *
 * open slot 0  device
 *
 *
 */
LOCAL void
open_slot0_device(la)
unsigned	la;
{
	unsigned		index;
	struct vxi_csr		*pcsr;
	int			status;

	pcsr = VXIBASE(la);

	if(	VXICLASS(pcsr) != VXI_REGISTER_DEVICE &&
		!VXIMXI(pcsr)){

		logMsg("Only register based slot 0 devices\n");
		logMsg("currently supported\n");
		return;
	}
	
	for(index=0; index < NELEMENTS(vxislot0); index++){
		if(vxislot0[index].pcsr == pcsr){
			return;
		}
		if(!vxislot0[index].present){
			break;
		}
	}
	if(index>=NELEMENTS(vxislot0)){
		logMsg(	
			"VXI Slot0s After %d'th ignored\n",
			NELEMENTS(vxislot0));
		return;
	}

	vxislot0[index].present = TRUE;
	vxislot0[index].reg = TRUE;
	vxislot0[index].pcsr = pcsr;
	vxislot0[index].set_modid = set_reg_modid;
	vxislot0[index].clear_modid = clr_all_reg_modid;

	if(!epvxiLibDeviceList[la]){
		status = open_vxi_device(la);
		if(status!=OK){
			logMsg("slot 0 card ignored\n");
			vxislot0[index].present = FALSE;
			return;
		}
	}

	/*
	 * if this happenens the card is wacko
	 */
	if(	epvxiLibDeviceList[la]->slot != 0 &&
		epvxiLibDeviceList[la]->slot != UKN_SLOT){

		logMsg(	"VXI slot 0 found in slot %d\n", 
			epvxiLibDeviceList[la]->slot);
		logMsg("VXI la %d hardware failure\n", la);
		logMsg("slot 0 card ignored\n");
		vxislot0[index].present = FALSE;
		return;
	}

	/*
 	 * hpE1404 specific stuff
	 */
        if(	VXIMAKE(pcsr) == VXI_MAKE_HP){
		if(	VXIMODEL(pcsr) == VXI_HP_MODEL_E1404 ||
			VXIMODEL(pcsr) == VXI_HP_MODEL_E1404_SLOT0){
			hpE1404Init(la);
		}
	}
	
	logMsg(	"VXI resman: slot zero found at LA=%d crate=%d\n", 
		la, 
		epvxiLibDeviceList[la]->crate);
}





/*
 *
 *	NICPU030_SLOT0_TEST()	
 *	check to see if this code is running on a
 *	national instruments cpu030 installed in
 *	slot zero.
 *
 */
#ifdef NICPU030
LOCAL void
nicpu030_slot0_test()
{
	int		i;
	int		status;
	short		model;
	UTINY		type;
	
	if(vxislot0[0].present){
		return;
	}

	for(i=0; i<NELEMENTS(nivxi_func_names); i++){

		if(pnivxi_func[i]){
			continue;
		}

		status = symFindByName(
				sysSymTbl,
				nivxi_func_names[i],
				&pnivxi_func[i],
				&type);
		if(status != OK){
			return;
		}
	}

	status = (*pnivxi_func[(unsigned)e_vxiInit])();
        if(status<0){
                logMsg("NI cpu030 init failed\n");
                return;
        }

	status = (*pnivxi_func[(unsigned)e_InitVXIlibrary])();
        if(status<0){
                logMsg("NIVXI library init failed\n");
                return;
        }

#	define VXI_MODEL_REGISTER 2
	status = (*pnivxi_func[(unsigned)e_VXIinLR])(
			VXI_MODEL_REGISTER,
			sizeof(model),
			&model);
	if(status<0){
                logMsg("CPU030 model type read failed\n");
                return;
        }

	if(VXISLOT0MODELTEST(VXIMODELMASK(model))){
		vxislot0[0].present = TRUE;
		vxislot0[0].nicpu030 = TRUE;
		vxislot0[0].set_modid = nivxi_cpu030_set_modid;
		vxislot0[0].clear_modid = nivxi_cpu030_clr_all_modid;
	
		logMsg(	"VXI resman: NI CPU030 slot zero found\n"); 
	}
}
#endif


/*
 *
 *	VXI_COUNT_DC_DEVICES
 *	count all the dynamic configuration devices of the same make and model
 *
 */
#ifdef JUNKYARD
LOCAL int
vxi_count_dc_devices(make, model)
unsigned			make;
unsigned			model;
{
	unsigned 		slot;
	unsigned 		crate;
	int			count=0;
	register int		status;
	short			id;
	struct vxi_csr_r	*pcsr;

	pcsr = (struct vxi_csr_r *) VXIBASE(VXIDYNAMICADDR);

	for(crate=0; crate<NELEMENTS(vxislot0); crate++){
 
		if(!vxislot0[crate].present)
			break;

		for(slot=0; slot<NVXISLOTS; slot++){
			SETMODID(slot, crate);
	  		status = vxMemProbe(	pcsr,
						READ,
						sizeof(id),
						&id);
  			if(status == OK)
				if(	make == VXIMAKE(pcsr) 
					&& model == VXIMODEL(pcsr)
					)
					count++;
		}
	}

	return count;
}
#endif


/*
 *
 *	VXI_FIND_OFFSET
 *	find a free space for n devices
 * 
 */
LOCAL int
vxi_find_offset(base_la, count, poffset)
unsigned	base_la;	
unsigned	count;
unsigned	*poffset;
{
	unsigned		peak = 0;
	unsigned 		addr;
	struct vxi_csr_r	*pcsr;
	register int		status;
	short			id;
	

	for(addr=base_la; addr<=last_la; addr++){

		pcsr = (struct vxi_csr_r *) VXIBASE(addr);
	  	status = vxMemProbe(	pcsr,
					READ,
					sizeof(id),
					&id);
  		if(status == OK){
			peak= 0;
			continue;
		}

		/* dont allow vxi int vec to overlap VME vectors in use */
		if(vxi_vec_inuse(addr)){
			peak= 0;
			continue;
		}

		peak++;

		if(peak >= count){
			*poffset = ((int)addr)-(count-1);
			return OK;
		}
	}
		
	return ERROR;
}



/*
 *
 *	VXI_FIND_SLOT
 *	given a VXI module's addr find its slot
 *
 */
int
vxi_find_slot(pcsr, pslot, pcrate)
struct vxi_csr		*pcsr;
unsigned  		*pslot;
unsigned		*pcrate;
{
	register unsigned char	slot;
	register unsigned char	crate;
	register		status;

	status = ERROR;

	for(crate=0; crate<NELEMENTS(vxislot0); crate++){
 
		if(!vxislot0[crate].present)
			break;

		for(slot=0;slot<NVXISLOTS;slot++){
			SETMODID(slot, crate);
  			if(VXIMODIDSTATUS(pcsr->dir.r.status)){
				*pslot = slot;
				*pcrate = crate;
				status = OK;
				break;
			}
		}
		CLRMODID(crate);

		if(status == OK)
			break;
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
LOCAL int
vxi_reset_dc()
{
	register unsigned 	addr;
	unsigned 		slot;
	unsigned 		crate;
	short			id;
	register		status;
	struct vxi_csr_w	*pcr;
	struct vxi_csr_r	*psr;

	for(addr=0; addr<last_la; addr++){

		psr = (struct vxi_csr_r *) VXIBASE(addr);
		pcr = (struct vxi_csr_w *) psr;

	  	status = vxMemProbe(	psr,
					READ,
					sizeof(id),
					&id);
  		if(status == ERROR)
			continue;

		status = vxi_find_slot(psr, &slot, &crate);
		if(status == ERROR){
			return ERROR;
		}

		SETMODID(slot, crate);


		pcr->addr = VXIDYNAMICADDR;
	}

	return OK;
}
#endif


/*
 *
 *	VXI_DC_TEST
 *	determine if a VXI module in the static address range is dynamic
 *
 */
#ifdef JUNKYARD
LOCAL int
vxi_dc_test(current_addr)
unsigned 	current_addr;
{
	register unsigned 	addr;
	unsigned 		slot;
	unsigned 		crate;
	short			id;
	register		status;
	struct vxi_csr_w	*pcr;
	struct vxi_csr_r	*psr;

	static unsigned		open_addr;
	unsigned		dynamic;

	for(addr=0; addr<last_la; addr++){

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
	if(status == ERROR){
		abort();
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
		abort();


	return dynamic;
}
#endif


/*
 *
 *      VXI_CONFIGURE_HIERARCHIES
 *
 */
LOCAL void 
vxi_configure_hierarchies(commander_la, servant_area)
unsigned	commander_la;
unsigned	servant_area;
{
        int			status;
        struct vxi_csr		*pcsr;
	unsigned long		response;
	VXIDI			**ppvxidi;
	VXIDI			*pvxidi;
	unsigned		sla;
	unsigned		last_sla;
	unsigned		area;

	last_sla = servant_area+commander_la;

	if(last_sla >= NELEMENTS(epvxiLibDeviceList)){
		logMsg(	"vxi resman: Clipping servant area (la=%d)\n",
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
			if(status<0){
				logMsg(	"vxi resman: gd failed (la=%d)\n",
					sla);
			}
			else{
				logMsg(	"vxi resman: gd resp %x\n",
					response);
			}
		}
		if(VXICMDR(pcsr)){
        	        status = epvxiCmdQuery(
					sla,
					(unsigned long)MBC_READ_SERVANT_AREA,
					&response);
			if(status<0){
				logMsg(	"vxi resman: rsa failed (la=%d)\n",
					sla);
			}
			else{
				area = response & MBR_READ_SERVANT_AREA_MASK;

				logMsg(	"The servant area was %d (la=%d)\n",
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
LOCAL int
vxi_begin_normal_operation()
{
        int			status;
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
		if(status<0){
			logMsg(	
	"vxi resman: beg nml op cmd failed (la=%d) (reason=%d)\n",
				la,
				status);
		}
		else if(
			MBR_STATUS(resp)!=MBR_STATUS_SUCCESS ||
			MBR_BNO_STATE(resp)!=MBR_BNO_STATE_NO){
			logMsg(	
	"vxi resman: beg nml op cmd failed (la=%d) (status=%x) (state=%x)\n",
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

			logMsg("Found a msg based cmdr\n");

                	status = epvxiCmdQuery(
                       		        la,
                               		(unsigned long)MBC_READ_SERVANT_AREA,
					&sa);
                	if(status<0){
                        	logMsg("vxi resman: rsa failed\n");
			}
			else{
				sa = sa & MBR_READ_SERVANT_AREA_MASK;
				logMsg(	"The servant area was %d\n",
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
LOCAL int
vxi_self_test()
{
	unsigned 		la;
	short			wd;
	int			status;
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
			unsigned	slot, crate;

			logMsg("VXI device self test failed\n");
			logMsg(	"\tmake %x\t model %x\n",
				pcsr->dir.r.make,
				pcsr->dir.r.model);
			logMsg("\taddr %x\n",la);
			status = vxi_find_slot(pcsr, &slot, &crate);
			if(status == OK)
				logMsg("\tslot %d crate %d\n\n", 
					slot, 
					crate);
			pcsr->dir.w.control = VXISAFECONTROL;
		}
	}

	return OK;
}


/*
 *
 *	VXI_INIT_IGNORE_LIST
 *	init list of interrupt handlers to ignore
 *
 */
LOCAL int
vxi_init_ignore_list()
{

  	int 	i;
	char 	type;
	int	status;

  	for(i=0; i<NELEMENTS(ignore_list); i++){
		status =
		  	symFindByName(	sysSymTbl,
					ignore_list[i],
					&ignore_addr_list[i],
					&type);
		if(status == ERROR)
			return ERROR;
	}

	return OK;
}


/*
 *
 *	VXI_VEC_INUSE
 *	check to see if vector is in use
 *
 */
LOCAL int
vxi_vec_inuse(addr)
unsigned	addr;
{
  	int 	i;
	void	*psub;

	psub = (void *) intVecGet(INUM_TO_IVEC(addr));
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
LOCAL void
set_reg_modid(crate, slot)
unsigned	crate;
unsigned	slot;
{
	VXI_SET_REG_MODID(vxislot0[crate].pcsr, slot);
}

/*
 *
 *      CLR_ALL_REG_MODID
 *
 */
LOCAL void
clr_all_reg_modid(crate)
unsigned	crate;
{
	VXI_CLR_ALL_REG_MODID(vxislot0[crate].pcsr);
}



/*
 *
 *      NIVXI_CPU030_SET_MODID
 *
 */
#ifdef NICPU030
LOCAL void
nivxi_cpu030_set_modid(crate, slot)
unsigned        crate;
unsigned        slot;
{
	 (*pnivxi_func[(unsigned)e_SetMODID])(TRUE,1<<slot);
}
#endif

/*
 *
 *      NIVXI_CPU030_CLR_ALL_MODID
 *
 */
#ifdef NICPU030
LOCAL void
nivxi_cpu030_clr_all_modid(crate)
unsigned        crate;
{
	 (*pnivxi_func[(unsigned)e_SetMODID])(FALSE,0);
}
#endif


/*
 *
 * epvxiDeviceList()
 *
 */
void
epvxiDeviceList()
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
		"Logical Address %3d Slot %2d Crate %2d Class %2x\n", 
				i,
				pmxidi->slot,
				pmxidi->crate,
				pmxidi->class);
			printf("\t");
			if(pmxidi->mxi_dev){
				printf("mxi, ");
			}
			if(pmxidi->msg_dev_online){
				printf("msg online, ");
			}
			printf("driver ID %d, ", pmxidi->driverID);
			printf("cmdr la %d, ", pmxidi->commander_la);
			printf("make %d, ", pmxidi->make);
			printf("model %d, ", pmxidi->model);
			printf("pio_report_func %x, ", pmxidi->pio_report_func);
			printf("\n");
		}
		i++;
		ppmxidi++;
	}
}


/*
 *
 * vxiUniqueDriverID()
 *
 * return a non zero unique id for a VXI driver
 */
int
vxiUniqueDriverID()
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
 * vxiOpen()
 *
 * 1)	Register the driver which will own a VXI device
 * 2)	Verify that the resource manager has run
 * 3) 	Verify that the device has passed its self test
 * 4)   Allocate a configuration block for the dev of size driverConfigSize
 * 5) 	Install an optional io report function
 *
 */
int
vxiOpen(la, vxiDriverID, driverConfigSize, pio_report_func)
unsigned	la;
int		vxiDriverID;
unsigned long	driverConfigSize; 
void		(*pio_report_func)();
{
	VXIDI	*pvxidi;
	void	*pconfig;

	if(la > NELEMENTS(epvxiLibDeviceList)){
		return VXI_BAD_LA;
	}

	if(vxiDriverID == UNINITIALIZED_DRIVER_ID){
		return VXI_NOT_OWNER;
	}

	pvxidi = epvxiLibDeviceList[la];
	
	if(pvxidi){
		if(pvxidi->driverID == vxiDriverID){
			return VXI_DEVICE_OPEN;
		}
		else if(pvxidi->driverID != NO_DRIVER_ATTACHED_ID){
			return VXI_NOT_OWNER;
		}
	}
	else{
		return VXI_UKN_DEVICE;
	}
	
	if(!pvxidi->st_passed){
		return VXI_SELF_TEST_FAILED;
	}	

	if(driverConfigSize){
		pconfig = (void *)calloc(1,driverConfigSize);
		if(!pconfig){
			return VXI_NO_MEMORY;
        	}
		pvxidi->pDriverConfig = pconfig;
	}
	else{
		pvxidi->pDriverConfig = NULL;
	}

	pvxidi->pio_report_func = pio_report_func;

	pvxidi->driverID = vxiDriverID;

	return VXI_SUCCESS;
}


/*
 *
 * vxiClose()
 *
 * 1)	Unregister a driver's ownership of a device
 * 2) 	Free driver's configuration block if one is allocated
 */
int
vxiClose(la, vxiDriverID)
unsigned        la;
int		vxiDriverID;
{
        VXIDI   *pvxidi;

	if(la > NELEMENTS(epvxiLibDeviceList)){
		return VXI_BAD_LA;
	}

        pvxidi = epvxiLibDeviceList[la];

        if(pvxidi){
                if(pvxidi->driverID == vxiDriverID){
			free(pvxidi);
                        return VXI_SUCCESS;
                }
                return VXI_NOT_OWNER;
        }

	return VXI_NOT_OPEN;	
}


/*
 *
 * epvxiLookupLA()
 *
 */
int
epvxiLookupLA(pdsp, pfunc, parg)
epvxiDeviceSearchPattern	*pdsp;
void				(*pfunc)();
void				*parg;
{
	VXIDI			*plac;
	unsigned		i;

	for(i=0; i<=last_la; i++){
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
                        if(VXI_PA_TO_LA(vxislot0[plac->crate].pcsr) 
						!= pdsp->slot_zero_la){
                                continue;
                        }
                }

                if(flags & VXI_DSP_commander_la){
                        if(plac->commander_la != pdsp->commander_la){
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
int
epvxiRouteTriggerECL(la, enable_map, io_map)
unsigned        la;             /* slot zero device logical address     */
unsigned	enable_map;	/* bits 0-5  correspond to trig 0-5	*/
				/* a 1 enables a trigger		*/
				/* a 0 disables	a trigger		*/ 
unsigned	io_map;		/* bits 0-5  correspond to trig 0-5	*/
				/* a 1 sources the front panel		*/ 
				/* a 0 sources the back plane		*/ 
{
        struct vxi_csr          *pcsr;
	char			mask;
	int			status;

	mask = (1<<VXI_N_ECL_TRIGGERS)-1;

	if((enable_map|io_map) & (~mask)){
		return VXI_BAD_TRIGGER;
	}

	/*
	 * CPU030 trigger routing
	 */
	if(pnivxi_func[(unsigned)e_MapECLtrig] && 
			pnivxi_func[(unsigned)e_GetMyLA]){
		if(la == (*pnivxi_func[(unsigned)e_GetMyLA])()){
	 		status = (*pnivxi_func[(unsigned)e_MapECLtrig])(
					la, 
					enable_map, 
					~io_map);
			if(status >= 0){
                		return VXI_SUCCESS;
			}	
		}
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
		if(	VXIMODEL(pcsr)==VXI_HP_MODEL_E1404 || 
			VXIMODEL(pcsr)==VXI_HP_MODEL_E1404_SLOT0){
                	return hpE1404RouteTriggerECL(
				la, 
				enable_map, 
				io_map);
		}
	}

	logMsg("failed to map ECL trigger for (la=%d)\n", la);

        return VXI_UKN_DEVICE;
}


/*
 * vxiRouteTriggerTTL()
 *
 */
int
vxiRouteTriggerTTL(la, enable_map, io_map)
unsigned        la;             /* slot zero device logical address     */
unsigned	enable_map;	/* bits 0-5  correspond to trig 0-5	*/
				/* a 1 enables a trigger		*/
				/* a 0 disables	a trigger		*/ 
unsigned	io_map;		/* bits 0-5  correspond to trig 0-5	*/
				/* a 1 sources the front panel		*/ 
				/* a 0 sources the back plane		*/ 
{
        struct vxi_csr          *pcsr;
	unsigned		mask;
	int			status;

	mask = (1<<VXI_N_TTL_TRIGGERS)-1;

	if(enable_map&mask || io_map&mask){
		return VXI_BAD_TRIGGER;
	}

	/*
	 * NI CPU030 trigger routing 
	 */
	if(pnivxi_func[(unsigned)e_MapTTLtrig] && 
			pnivxi_func[(unsigned)e_GetMyLA]){
		if(la == (*pnivxi_func[(unsigned)e_GetMyLA])()){
	 		status = (*pnivxi_func[(unsigned)e_MapTTLtrig])(
					la, 
					enable_map, 
					~io_map);
			if(status >= 0){
                		return VXI_SUCCESS;
			}	
		}
	}

        pcsr = VXIBASE(la);

	if(VXIMXI(pcsr)){
		short	tmp;

		tmp = (enable_map<<8) | ~io_map; 
		pcsr->dir.w.dd.mxi.trigger_config = tmp;
	
		return VXI_SUCCESS;
	}

	/*
	 * HP MODEL E1404 trigger routing 
	 */
	if(VXIMAKE(pcsr)==VXI_MAKE_HP){ 
		if(	VXIMODEL(pcsr)==VXI_HP_MODEL_E1404 ||
			VXIMODEL(pcsr)==VXI_HP_MODEL_E1404_SLOT0){
                	return hpE1404RouteTriggerTTL(
					la, 	
					enable_map, 
					io_map);
		}
	}

	logMsg("failed to map TTL trigger for (la=%d)\n", la);

        return VXI_UKN_DEVICE;
}




/*
 *
 *      VXI_IO_REPORT
 *
 *      call io report routines for all registered devices
 *
 */
int
vxi_io_report(level)
unsigned 	level;
{
        register int            i;
        register unsigned	la;
        register                status;
        unsigned                slot;
        unsigned                crate;
        short                   id;
        struct vxi_csr          *pcsr;
	VXIDI			*plac;

        /* Get local address from VME address. */
	/* in case the resource manager has not been called */
	if(!epvxi_local_base){
		status = sysBusToLocalAdrs(
                                VME_AM_USR_SHORT_IO,
                                VXIBASEADDR,
                                &epvxi_local_base);
		if(status != OK){
			logMsg("VXI addressing error\n");
			return ERROR;
		}
	}

        printf("<< VXI logical address table >>\n");

        for(la=0; la<=last_la; la++){
		char	*pmake;
		int	make;

                pcsr = VXIBASE(la);
                status = vxMemProbe(    pcsr,
                                        READ,
                                        sizeof(id),
                                        &id);
                if(status != OK){
                        continue;
                }

               	status = vxi_find_slot(pcsr, &slot, &crate);
               	if(status == ERROR){
			printf("VXI: slot could not be found\n");
                       	slot = UKN_SLOT;
			crate = UKN_CRATE;
		}

		make = VXIMAKE(pcsr);
		pmake = NULL;
		for(i=0; i<NELEMENTS(vxi_vi); i++){
			if(vxi_vi[i].make == make){
				pmake = vxi_vi[i].pvendor_name;
				break;
			}
		}

                printf("VXI LA %2x ", la);
		if(pmake){
                	printf("%30s ", pmake);
		}
		else{
                	printf("make %4x ", make);
		}

		printf("model %4x crate %2d slot %2d\n",
                        (int) VXIMODEL(pcsr),
                        (int) crate,
                        (int) slot);

		if(!VXIPASSEDSTATUS(pcsr->dir.r.status)){
			printf("\t---- Self Test Failed ----\n");
			continue;
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
			continue;
		}

		printf("\taddress %8x\n",
                        (int) VXIBASE(la));

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
			if(status>=0){
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
					printf("evnt gen, ");
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


        }
        return OK;
}


/*
 *
 *	mxi_io_report()
 *
 *
 */
LOCAL int
mxi_io_report(pmxi, level)
struct vxi_csr	*pmxi;
int 		level;
{
	unsigned la;
	unsigned ha;

        printf(", MXI sub class");

	if(pmxi->dir.w.dd.mxi.control & MXI_UPPER_LOWER_BOUNDS){
		la = VXIADDRMASK & 
			pmxi->dir.w.dd.mxi.la_window;
		ha = VXIADDRMASK & 
			(pmxi->dir.w.dd.mxi.la_window>>NVXIADDRBITS);
	}
	else{
		la = VXIADDRMASK & 
			pmxi->dir.w.dd.mxi.la_window;
		ha = la + (MXI_LA_WINDOW_SIZE_MASK & 
			(pmxi->dir.w.dd.mxi.la_window>>NVXIADDRBITS));
	}
	printf(", LA window %x-%x", 
		la,
		ha);

}


/*
 *
 *	vxi_allocate_int_lines()
 *
 *
 */
LOCAL int
vxi_allocate_int_lines()
{
        int			status;
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
		if(status<0){
			logMsg( "Device rejected READ_PROTOCOL (la=%d)\n",
				la);
			continue;
		}
		if(!MBR_RP_PI(resp)){
			continue;
		}

		logMsg("Programming interrupter (la=%d)\n", la);

		cmd = MBC_READ_INTERRUPTERS;
		status = epvxiCmdQuery(
				la,
				cmd,
				&resp);
		if(status<0){
			logMsg( "Device rejected READ_INTERRUPTERS (la=%d)\n",
				la);
			continue;
		}
		line_count = resp&MBR_READ_INTERRUPTERS_MASK;
		while(line_count--){
			cmd = 	MBC_ASSIGN_INTERRUPTER_LINE |
				(line_count+1)<<4 |
				VXIMSGINTLEVEL;
			sysIntEnable(VXIMSGINTLEVEL);
			status = epvxiCmdQuery(
					la,
					cmd,
					&resp);
			if(status<0){
				logMsg( "Device rejected ASSIGN_INT(la=%d)\n",
				la);
				continue;
			}
			if(MBR_STATUS(resp) != MBR_STATUS_SUCCESS){
				logMsg( "ASSIGN_INT failed (la=%d)\n",
				la);
				continue;
			}
		}
	}
}


