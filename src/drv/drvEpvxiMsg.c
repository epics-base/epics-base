/*
 *     	share/src/drv/$Id$
 *
 *      driver for VXI message based devices 
 *
 *      Author:      Jeff Hill
 *      Date:        042792
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
 *      Modification Log:
 *      -----------------
 *      .01 joh 042792  first release
 *	.02 joh	052292	Always append a NULL even if at the end of
 *			the buffer in vxi_read()
 *	.03 joh 060292	Added debug mode
 *	.04 joh	072992	added signal register for the HP1404
 * 	.05 joh 082592	added arg to epvxiRead() and epvxiWrite()
 * 	.06 mgb 080493	Removed V5/V4 and EPICS_V2 conditionals
 *
 *	Improvements
 *	------------
 *	.01 joh 051992	Some work on fast handshake exists in this file.
 *			If a card with fast handshake is found to exist
 *			then this code will be tested and uncommented.
 *
 */

static char	*sccsId = "$Id$\t$Date$";

#include <vxWorks.h>
#include <semLib.h>
#include <drvEpvxi.h>
#include <fast_lock.h>
#include <iv.h>

enum msgDeviceSyncType {
	syncInt, 
	syncSignal, 
	syncPoll,
};

typedef
struct epvxiMessageDeviceInfo{
	unsigned		err:1;		/* error pending 	*/
	unsigned		trace:1;	/* debug trace on	*/
	unsigned long		timeout;	/* in ticks 		*/
	enum msgDeviceSyncType	syncType;
	SEM_ID			syncSem;
	FAST_LOCK		lck;
}VXIMDI;

#define VXIMSGSYNCDELAY 1

#define DEFAULTMSGTMO	(sysClkRateGet()*10)	/* 10 sec */
#define MAXIMUMTMO	(0xffffff)

/*
 * set to a valid LA when the LA of the
 * commander is located
 */
LOCAL
int	msgCommanderLA = (-1);

LOCAL
char	vxiMsgSignalInit;

#define VXI_HP_MODEL_E1404_SLOT0 	0x010
#define VXI_HP_MODEL_E1404_MSG 		0x111
#define VXI_HP_MODEL_E1404 		0x110

#define abort(A)	taskSuspend(0)

#define epvxiPMsgConfig(LA)	((VXIMDI *)(epvxiLibDeviceList[LA]->pMsgConfig))

/*
 * local functions
 */
void 	static set_la(
	int	la,
	int	*pla
);

static void 	vxiMsgInt(
	unsigned 	la
);

static void 	signalHandler(
	unsigned short	signal
);

static int 	epvxiReadSlowHandshake(
	unsigned        la,
	char            *pbuf,
	unsigned long	count,
	unsigned long	*pread_count,
	unsigned long	option
);

static int 	epvxiReadFastHandshake(
	unsigned	la,
	char		*pbuf,
	unsigned long	count,
	unsigned long	*pread_count,
	unsigned long	option
);

static int 	vxiMsgClose(
	unsigned        la
);

static int 	vxiMsgOpen(
	unsigned	la
);

static void	vxiMsgSignalSetup(
	void
);

static void	vxiCPU030MsgSignalSetup(
	void
);

static void	vxiHP1404MsgSignalSetup(
	void
);

static int 	vxiAttemptAsyncModeControl(
	unsigned	la,
	unsigned long	cmd
);

static int 	vxiMsgSync(
	unsigned	la,
	unsigned	resp_mask,
	unsigned	resp_state,
	int		override_err
);

static int 	fetch_protocol_error(
	unsigned	la
);


/*
 * should be in a header
 */
static int	vxi_msg_test(
	unsigned	la
);

static int	vxi_msg_print_id(
	unsigned	la
);

static int 	vxi_msg_test_protocol_error(
	unsigned	la
);


/*
 *
 * vxi_msg_test()
 *
 */
vxi_msg_test(
	unsigned	la
)
{
	char		buf[512];
	unsigned long	count;
	int		status;

	status = epvxiWrite(la, "*IDN?", 5, &count, epvxiWriteOptNone);
	if(status != VXI_SUCCESS){
		return status;
	}
	status = epvxiRead(la, buf, sizeof(buf)-1, &count, epvxiReadOptNone);
	if(status != VXI_SUCCESS){
		return status;
	}

	buf[count] = NULL;
	printf("%s %d\n", buf,count);

	status = epvxiWrite(la, "*TST?", 5, &count, epvxiWriteOptNone);
	if(status != VXI_SUCCESS){
		return status;
	}
	status = epvxiRead(la, buf, sizeof(buf)-1, &count, epvxiReadOptNone);
	if(status != VXI_SUCCESS){
		return status;
	}

	buf[count] = NULL;
	printf("%s %d\n", buf, count);

	return VXI_SUCCESS;
}


/*
 *
 * vxi_msg_print_id
 *
 */
int	vxi_msg_print_id(
	unsigned	la
)
{
        char    	buf[32];
        unsigned long	count;
	char		*pcmd = "*IDN?";
	int		status;

        status = epvxiWrite(la, pcmd, strlen(pcmd), &count, epvxiWriteOptNone);
	if(status != VXI_SUCCESS){
		return status;
	}
        status = epvxiRead(la, buf, sizeof(buf)-1, &count, epvxiReadOptNone);
	if(status != VXI_SUCCESS){
		return status;
	}

        buf[count] = NULL;
        printf(" %s ", buf);

        return VXI_SUCCESS;
}


/*
 *
 *  vxi_msg_test_protocol_error
 *
 */
int 	vxi_msg_test_protocol_error(
	unsigned	la
)
{
	unsigned long	resp;
	int		i;
	int		status;

	for(i=0;i<1000;i++){
		status = epvxiCmd(la, MBC_READ_PROTOCOL);
		if(status<0){
			return status;
		}
	}
        return VXI_SUCCESS;
}


/*
 * epvxiCmd()
 *
 * deliver a command to a msg based device
 *
 */
int
epvxiCmd(
unsigned	la,
unsigned long	cmd
)
{
        struct vxi_csr		*pcsr;
	VXIMDI			*pvximdi;
	int			status;

#	ifdef DEBUG
		logMsg("cmd to be sent %4x (la=%d)\n", cmd, la);
#	endif

	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

        pcsr = VXIBASE(la);

	FASTLOCK(&pvximdi->lck);

	/*
 	 * RULE C.3.3
 	 * A commander shall not send any command requiring a servant to
	 * place data in in its data registers until the commander has read
	 * (from the data registers) all data generated by previous commands
	 * (and the read ready bit is set to zero).
	 */
	status = vxiMsgSync(
			la, 
			VXIWRITEREADYMASK|VXIREADREADYMASK, 
			VXIWRITEREADYMASK,
			cmd == MBC_CLEAR);
	if(status>=0){
		pcsr->dir.w.dd.msg.dlow = cmd;
	}
	else{
	        /*
       	 	 * RULE C.3.2
        	 */
	        if(pcsr->dir.r.dd.msg.response&VXIREADREADYMASK){
       		         status = VXI_UNREAD_DATA;
       	 	}
	}

	FASTUNLOCK(&pvximdi->lck);

	if(status == VXI_PROTOCOL_ERROR){
		return fetch_protocol_error(la);
	}

	if(pvximdi->trace){
		printf( "VXI Trace: (la=%3d) Cmd   -> %x\n",
			la,
			cmd);
	}

	return status;
}



/*
 * epvxiQuery()
 *
 * query the response to a command
 *
 */
int
epvxiQuery(
unsigned	la,
unsigned long	*presp
)
{
        struct vxi_csr		*pcsr;
	VXIMDI			*pvximdi;
	int			status;

	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

        pcsr = VXIBASE(la);

	FASTLOCK(&pvximdi->lck);

	status = vxiMsgSync(
			la, 
			VXIREADREADYMASK, 
			VXIREADREADYMASK,
			FALSE);
	if(status<=0){
		*presp = pcsr->dir.r.dd.msg.dlow;
	}

	FASTUNLOCK(&pvximdi->lck);

#	ifdef DEBUG
		logMsg("resp returned %4x (la=%d)\n", *presp, la);
#	endif

	if(status == VXI_PROTOCOL_ERROR){
		return fetch_protocol_error(la);
	}

	if(pvximdi->trace){
		printf( "VXI Trace: (la=%3d) Query -> %x\n",
			la,
			*presp);
	}

	return status;
}


/*
 * epvxiCmdQuery()
 */
int
epvxiCmdQuery(
unsigned	la,
unsigned long	cmd,
unsigned long 	*presp 
)
{
	int	status;

	status = epvxiCmd(la, cmd);
	if(status<0){
		return status;
	}
	status = epvxiQuery(la, presp);
	return status;
}


/*
 *	epvxiRead()
 *
 * 	Read a string using fast handshake mode
 * 	or call a routine to do a slow handshake
 * 	if that is all that is supported. 
 */
int
epvxiRead(
unsigned	la,
char		*pbuf,
unsigned long	count,
unsigned long	*pread_count,
unsigned long	option
)
{
	VXIMDI			*pvximdi;
	int			status;

	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

	/*
	 * does the device support fast handshake
	 */
#	ifdef FASTHANDSHAKE
		if(VXIFHS(pcsr)){
			status = epvxiReadFastHandshake(
					la, 
					pbuf, 
					count, 
					pread_count,
					option);
		}
		else{
#	endif
			status = epvxiReadSlowHandshake(
					la, 
					pbuf, 
					count, 
					pread_count,
					option);
#	ifdef FASTHANDSHAKE
		}
#	endif

	if(pvximdi->trace){
		printf( "VXI Trace: (la=%3d) Read -> %*s\n",
			la,
			count,
			pbuf);
	}

	return status;
}


#ifdef FASTHANDSHAKE
@@@@ needs to tell them if their buffer is full
	and the EOM bit wasnt se @@@@
/*
 *	epvxiReadFastHandshake()
 *
 * 	Read a string using fast handshake mode
 * 	or call a routine to do a slow handshake
 * 	if that is all that is supported. 
 *
 *	This function will be tested and installed
 *	if a card with fast handshake s found to exist
 *
 */
LOCAL 
int 	epvxiReadFastHandshake(
	unsigned	la,
	char		*pbuf,
	unsigned long	count,
	unsigned long	*pread_count,
	unsigned long	option
)
{
        struct vxi_csr		*pcsr;
	VXIMDI			*pvximdi;
	short			resp;
	int			fhm;
	short			cmd;
	int			status;
	int			i;

	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

        pcsr = VXIBASE(la);

	FASTLOCK(&pvximdi->lck);
	fhm = FALSE;
	/*
	 * always leave room to write a NULL termination
	 */
	for(i=0; i<(count-1); i++){

		while(TRUE){	
			/*
			 * wait for fast handshake mode
			 */
			if(!fhm){
				status = vxiMsgSync(
						la, 
						VXIFHSMMASK, 
						0,
						FALSE);
				if(status<0){
					*pread_count = i;
					goto exit;
				}
				fhm = TRUE;
			}

			cmd = MBC_BR;
			status = vxMemProbe(
					&pcsr->dir.r.dd.msg.dlow,
					WRITE,
					sizeof(pcsr->dir.r.dd.msg.dlow),
					&cmd);
			if(status == OK){
				break;
			}
			fhm = FALSE;
		}

        	while(TRUE){
			/*
			 * wait for fast handshake mode
			 */
			if(!fhm){
				status = vxiMsgSync(
						la, 
						VXIFHSMMASK, 
						0,
						FALSE);
				if(status<0){
					*pread_count = i;
					goto exit;
				}
				fhm = TRUE;
			}

			status = vxMemProbe(
					&pcsr->dir.r.dd.msg.dlow,
					READ,
					sizeof(pcsr->dir.r.dd.msg.dlow),
					&resp);
			if(status == OK){
				break;
			}
			fhm = FALSE;
		}

		*pbuf = resp;
		pbuf++;
		if(resp & MBC_END){
			*pread_count = i+1;
			break;
		}
	}
        status = VXI_SUCCESS;
exit:
	FASTUNLOCK(&pvximdi->lck);

	if(status == VXI_PROTOCOL_ERROR){
		return fetch_protocol_error(la);
	}

	*pbuf = NULL;

	return status;
}
#endif


/*
 * epvxiReadSlowHandshake()
 */
LOCAL 
int 	epvxiReadSlowHandshake(
	unsigned        la,
	char            *pbuf,
	unsigned long	count,
	unsigned long	*pread_count,
	unsigned long	option
)
{
	VXIMDI			*pvximdi;
        struct vxi_csr		*pcsr;
        short                   resp;
        int                     status;
        int			function_status;
	int			i;

	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

        pcsr = VXIBASE(la);

	/*
	 * always leave room to write a NULL termination
	 */
	if(count<1){
		return VXI_BUFFER_FULL;
	}

	FASTLOCK(&pvximdi->lck);

	/*
	 * always leave room to write a NULL termination
	 */
	function_status = VXI_BUFFER_FULL;
        for(i=0; i<(count-1); i++){

                /*
                 * wait for handshake
		 *
		 * RULE C.3.3 specifies that there shouldnt be
		 * any unread data present at this point .
                 */
		status = vxiMsgSync(
				la, 
				VXIWRITEREADYMASK|VXIDORMASK|VXIREADREADYMASK, 
				VXIWRITEREADYMASK|VXIDORMASK,
				FALSE);
		if(status<0){
	        	if(pcsr->dir.r.dd.msg.response&VXIREADREADYMASK){
				function_status = VXI_UNREAD_DATA;
			}
			else{
				function_status = status;
			}
			break;
		}

		pcsr->dir.w.dd.msg.dlow = MBC_BR;

                /*
                 * wait for handshake
                 */
		status = vxiMsgSync(
				la, 
				VXIREADREADYMASK, 
				VXIREADREADYMASK,
				FALSE);
		if(status<0){
			function_status = status;
			break;
		}

                resp = pcsr->dir.r.dd.msg.dlow;

                *pbuf = resp;
                pbuf++;
                if(resp & MBC_END){
			
			/*
			 * so the read count will be correct below
			 */
			i++;	
			function_status = VXI_SUCCESS;
                        break;
                }
        }
	FASTUNLOCK(&pvximdi->lck);

        *pread_count = i;

	/*
	 * append the NULL
	 */
	*pbuf = NULL;

	if(function_status == VXI_PROTOCOL_ERROR){
		return fetch_protocol_error(la);
	}

	return function_status;
}


/*
 * epvxiWrite()
 * (set the end bit on the last byte sent)
 */
int
epvxiWrite(
unsigned        la,
char            *pbuf,
unsigned long	count,
unsigned long	*pwrite_count,
unsigned long	option	
)
{
	VXIMDI			*pvximdi;
       	struct vxi_csr		*pcsr;
	int			i;
	short			cmd;
	short 			extra;
	int			status;
	char			*pstr;

	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

        pcsr = VXIBASE(la);

	FASTLOCK(&pvximdi->lck);
	pstr = pbuf;
	if(option&epvxiWriteOptPartialMsg){
		extra = 0;
	}
	else{
		extra = MBC_END;
	}
	for(i=0; i<count; i++){

                /*
                 * wait for handshake
                 */
		status = vxiMsgSync(
				la, 
				VXIWRITEREADYMASK|VXIDIRMASK, 
				VXIWRITEREADYMASK|VXIDIRMASK,
				FALSE);
		if(status<0){
			*pwrite_count = i;
			goto exit;
		}
		if(i == count-1){
			cmd = extra | MBC_BA | *pstr;
		}
		else{
			cmd = MBC_BA | *pstr;
		}
        	pcsr->dir.r.dd.msg.dlow = cmd;
		pstr++;
	}
	*pwrite_count = i;
        status = VXI_SUCCESS; 
exit:
	FASTUNLOCK(&pvximdi->lck);

	if(status == VXI_PROTOCOL_ERROR){
		return fetch_protocol_error(la);
	}

	if(pvximdi->trace){
		printf( "VXI Trace: (la=%3d) Write -> %*s\n",
			la,
			count,
			pbuf);
	}

	return status;
}


/*
 *
 * epvxiSetTimeout()
 *
 * change the message based transfer timeout
 * (timeout is in milli sec)
 *
 */
int
epvxiSetTimeout(
unsigned 	la,
unsigned long	timeout
)
{
	VXIMDI		*pvximdi;
	int		status;

	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

	/*
 	 * order of operations significant here
	 */
	if(timeout > MAXIMUMTMO){
		return VXI_TIMEOUT_TO_LARGE;
	}

	pvximdi->timeout = (timeout * sysClkRateGet())/1000;	

	return VXI_SUCCESS;
}


/*
 *
 * epvxiSetTraceEnable()
 *
 * turn trace mode on or off
 *
 */
int
epvxiSetTraceEnable(la, enable)
unsigned	la;
int		enable;
{
	VXIMDI		*pvximdi;
	int		status;

	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

	pvximdi->trace = enable?TRUE:FALSE; 

	return VXI_SUCCESS;
}


/*
 *
 * vxiMsgClose()
 *
 *
 */
LOCAL 
int vxiMsgClose(
unsigned        la
)
{
	int		status;
	VXIMDI		*pvximdi;

	pvximdi = epvxiPMsgConfig(la);
	if(!pvximdi){
		return VXI_NOT_OPEN;
	}

	status = semDelete(pvximdi->syncSem);
	if(status<0){
		logMsg(	"%s: vxiMsgClose(): bad sem id\n",
			__FILE__);
	}
	FASTLOCKFREE(&pvximdi->lck);
	return VXI_SUCCESS;
}


/*
 *
 * vxiMsgOpen()
 *
 *
 */
LOCAL 
int 	vxiMsgOpen(
	unsigned	la
)
{
	int 		status;
	VXIDI		*pvxidi;
	VXIMDI		*pvximdi;
	unsigned long	resp;
	unsigned long   read_proto_resp;
	unsigned long	cmd;
       	struct vxi_csr	*pcsr;
	int		signalSync = FALSE;
	int		intSync = FALSE;


	/*
	 *	 return quickly if we have been here before
	 */
	pvxidi = epvxiLibDeviceList[la];
	if(pvxidi->pMsgConfig){
		return VXI_SUCCESS;
	}

	status = epvxiDeviceVerify(la);
	if(status<0){
		return status;
	}

	pcsr = VXIBASE(la);
	if(VXICLASS(pcsr) != VXI_MESSAGE_DEVICE){
		return VXI_NOT_MSG_DEVICE;
	}

	pvximdi = (VXIMDI *) calloc(1, sizeof(*pvximdi));
	if(!pvximdi){
		return VXI_NO_MEMORY;
	}

	pvxidi->pMsgConfig = (void *) pvximdi;

	vxiMsgSignalSetup();

		pvximdi->syncSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if(!pvximdi->syncSem){
		return VXI_NO_MEMORY;
	}

	/*
	 *
	 * assume the worst for the transfers below
	 *
	 */
	pvximdi->timeout = DEFAULTMSGTMO;
	pvximdi->syncType = syncPoll;
	FASTLOCKINIT(&pvximdi->lck);

	/*
	 * if it is not an interrupter or a signal
	 * generator then we poll
	 */
	if(!VXIMBINT(pcsr) && !VXIVMEBM(pcsr)){
		return VXI_SUCCESS;
	}

	/*
	 * if it is not a response generator then we poll
	 */
	status = epvxiCmdQuery(
			la,
			(unsigned long) MBC_READ_PROTOCOL,
			&read_proto_resp);
	if(status<0){
		/*
		 * All devices are required by the VXI standard 
		 * to accept this command while in the
		 * configure state or in the normal operation
		 * state. Some dont.
		 *
		 */
		logMsg(	"%s: Device rejected MBC_READ_PROTOCOL (la=%d)\n",
			__FILE__,
			la);
		return VXI_SUCCESS;
	}

return VXI_SUCCESS;

	if(!MBR_RP_RG(read_proto_resp)){
		return VXI_SUCCESS;
	}

logMsg("mb device has response gen\n");

	/*
	 * try to setup interrupt synchronization first
	 * (this works even if we dont have a signal register)
	 */
	if(VXIMBINT(pcsr)){
		cmd = 	MBC_ASYNC_MODE_CONTROL |
			MBC_AMC_RESP_ENABLE |
			MBC_AMC_RESP_INT_ENABLE;
		status = vxiAttemptAsyncModeControl(la, cmd);
		if(status>=0){
			logMsg(	"%s: mb device has int sync!\n",
				__FILE__);
			intSync = TRUE;
		}
	}

	/*
	 * hopefully a signal register is available if we get to here
	 */
	if(VXIVMEBM(pcsr) && !intSync && msgCommanderLA>=0){
		cmd = 	MBC_ASYNC_MODE_CONTROL |
			MBC_AMC_RESP_ENABLE |
			MBC_AMC_EVENT_ENABLE |
			MBC_AMC_RESP_SIGNAL_ENABLE |
			MBC_AMC_EVENT_SIGNAL_ENABLE;
		status = vxiAttemptAsyncModeControl(la, cmd);
		if(status>=0){
			logMsg(	"%s: mb device has signal sync!\n",
				__FILE__);
			signalSync = TRUE;
		}
	}

	if(!intSync && !signalSync){
		logMsg(	"%s: mb responder failed to configure\n",
			__FILE__);
		return VXI_SUCCESS;
	}

	cmd = 	MBC_CONTROL_RESPONSE;
	status = epvxiCmdQuery(
			la,
			cmd,
			&resp);
	if(status<0){
		logMsg(	"%s: Control response rejected by responder\n",
			__FILE__);
		vxiMsgClose(la);
		return status;
	}
	if(	MBR_STATUS(resp) != MBR_STATUS_SUCCESS ||
		(resp^cmd)&MBR_CR_CONFIRM_MASK){
		logMsg(	"%s: Control Response Failed %x\n", 
			__FILE__,
			resp);
		return VXI_SUCCESS;
	}
logMsg("sent ctrl resp (la=%d) (cmd=%x)\n", la, cmd);

logMsg("synchronized msg based device is ready!\n");

	if(intSync){
		pvximdi->syncType = syncInt;
	}
	if(signalSync){
		pvximdi->syncType = syncSignal;
	}

	return VXI_SUCCESS;
}


/*
 *
 * vxiMsgSignalSetup 
 *
 *
 */
LOCAL 
void	vxiMsgSignalSetup(
	void
)
{
	int 		status;
	static char	vxiMsgSignalInit;

	if(vxiMsgSignalInit){
		return;
	}

	vxiMsgSignalInit = TRUE;

	vxiHP1404MsgSignalSetup();

	if(msgCommanderLA<0){
		vxiCPU030MsgSignalSetup();
	}

	if(msgCommanderLA<0){
		logMsg(	"%s: Unable to locate determine the commander's LA\n",
			__FILE__);
	}
}


/*
 *
 * vxiCPU030MsgSignalSetup 
 *
 *
 */
LOCAL 
void	vxiCPU030MsgSignalSetup(
	void
)
{
	int	niMsgLA;
	int	status;

	if(	!pnivxi_func[(unsigned)e_EnableSignalInt] || 
		!pnivxi_func[(unsigned)e_SetSignalHandler] ||
		!pnivxi_func[(unsigned)e_RouteSignal] ||
		!pnivxi_func[(unsigned)e_GetMyLA]){
		return;
	}

	niMsgLA = (*pnivxi_func[(unsigned)e_GetMyLA])();

#	define ANY_DEVICE (-1)
#	define MSG_RESP_ENABLE (0x3f)
	status = (*pnivxi_func[(unsigned)e_RouteSignal])(
				ANY_DEVICE, 
				~0);	/* enable every thing */
	if(status<0){
		return;
	}

#	define UKN_DEVICE (-2)
	status = (*pnivxi_func[(unsigned)e_SetSignalHandler])(
				UKN_DEVICE, 
				signalHandler);
	if(status<0){
		return;
	}

	status = (*pnivxi_func[(unsigned)e_EnableSignalInt])();
	if(status){
		return;
	}

	msgCommanderLA = niMsgLA;

	return;
}


/*
 *
 * vxiHP1404MsgSignalSetup
 *
 *
 */
LOCAL 
void	vxiHP1404MsgSignalSetup(
	void
)
{
	epvxiDeviceSearchPattern	dsp;
	int				hpMsgLA = -1;
	int				hpRegLA = -1;
	int				status;

	dsp.flags = VXI_DSP_make | VXI_DSP_model;
	dsp.make = VXI_MAKE_HP;
	dsp.model = VXI_HP_MODEL_E1404_MSG;
	status = epvxiLookupLA(&dsp, set_la, (void *)&hpMsgLA);
	if(status<0){
		return;
	}
	if(hpMsgLA<0){
		return;
	}
	dsp.flags = VXI_DSP_make | VXI_DSP_slot;
	dsp.make = VXI_MAKE_HP;
	dsp.slot = epvxiLibDeviceList[hpMsgLA]->slot;
	status = epvxiLookupLA(&dsp, set_la, (void *)&hpRegLA);
	if(status<0){
		return;
	}

	if(hpRegLA<0){
		return;
	}

	msgCommanderLA = hpMsgLA;
	hpE1404SignalConnect(hpRegLA, signalHandler);

	return;
}


/*
 *
 * set_la
 *
 *
 */
LOCAL 
void 	set_la(
	int	la,
	int	*pla
)
{
	*pla = la;
}


/*
 *
 * vxiAttemptAsyncModeControl 
 *
 *
 */
LOCAL 
int 	vxiAttemptAsyncModeControl(
	unsigned	la,
	unsigned long	cmd
)
{
	int 		status;
	unsigned long	resp;
	unsigned long   tmpcmd;

	if(msgCommanderLA<0 && cmd&MBC_AMC_RESP_SIGNAL_ENABLE){
		return ERROR;
	}

	/*
	 * this step tells the device what la to signal at
	 */
	if(cmd & MBC_AMC_RESP_SIGNAL_ENABLE){
		tmpcmd = MBC_IDENTIFY_COMMANDER | msgCommanderLA;
		status = epvxiCmd(
				la,
				tmpcmd);
		if(status<0){
			logMsg(	"%s: IDENTIFY_COMMANDER rejected (la=%d)\n",
				__FILE__,
				la);
			return ERROR;
		}
logMsg("sent id cmdr (la=%d) (cmd=%x)\n", la, tmpcmd);
	}

	status = epvxiCmdQuery(
			la,
			cmd,
			&resp);
	if(status<0){
		logMsg(	"%s: Async mode control rejected (la=%d)\n",
			__FILE__,
			la);
		return ERROR;
	}
	if(	MBR_STATUS(resp) != MBR_STATUS_SUCCESS ||
		(resp^cmd)&MBR_AMC_CONFIRM_MASK){
		logMsg(	"%s: async mode ctrl failure (la=%d,cmd=%x,resp=%x)\n",
			__FILE__,
			la,
			cmd,
			resp);
		return ERROR;
	}
logMsg("sent asynch mode control (la=%d) (cmd=%x)\n",la,cmd);


	if(cmd & MBC_AMC_RESP_INT_ENABLE){
		intConnect(
			INUM_TO_IVEC(la),	
			vxiMsgInt,
			la);	
logMsg("connected to interrupt (la=%d)\n", la);
	}

	return OK;
}


/*
 *
 * vxiMsgSync()
 *
 *
 */
LOCAL 
int 	vxiMsgSync(
	unsigned	la,
	unsigned	resp_mask,
	unsigned	resp_state,
	int		override_err
)
{
	VXIMDI		*pvximdi;
	struct vxi_csr	*pcsr;
	int		status;
	long		timeout;
	unsigned short	resp;
  	int		pollcnt = 100;

	
	while(!(pvximdi = epvxiPMsgConfig(la))){
		status = vxiMsgOpen(la);
		if(status != VXI_SUCCESS){
			return status;
		}
	}

	pcsr = VXIBASE(la);

#	ifdef DEBUG
		logMsg(	"Syncing to resp mask %4x, request %4x (la=%d)\n",
			resp_mask,
			resp_state,
			la);
#	endif

	timeout = pvximdi->timeout;
	do{
		int sync;

		resp = pcsr->dir.r.dd.msg.response;

		sync = !((resp^resp_state)&resp_mask);

		if(!(resp & VXIERRNOTMASK)){
			if(!override_err && !pvximdi->err){
				pvximdi->err = TRUE;
				return VXI_PROTOCOL_ERROR;
			}
		}

		if(sync){
			return VXI_SUCCESS;
		}
 
		/*
		 * this improves VXI throughput at the 
		 * expense of sucking CPU
		 */
		if(pollcnt>0){
			pollcnt--;
		}
		else{
			status = semTake(
					pvximdi->syncSem, 
					VXIMSGSYNCDELAY);
			if(status < 0){
				timeout -= VXIMSGSYNCDELAY;
			}
		}
	}
	while(timeout>0);

	/*
	 * sync timed out if we got here
	 */
	logMsg(	"%s: msg dev timed out after %d sec\n", 
		__FILE__,
		(pvximdi->timeout-timeout) / sysClkRateGet());
	logMsg(	"%s: resp mask %4x, request %4x, actual %4x\n",
		__FILE__,
		resp_mask,
		resp_state,
		resp);

	return VXI_MSG_DEVICE_TMO;
}


/*
 *
 * fetch_protocol_error
 *
 */
LOCAL 
int 	fetch_protocol_error(
	unsigned	la
)
{
	VXIMDI		*pvximdi;
	unsigned long	error;
	struct vxi_csr	*pcsr;
	unsigned short	resp;
	int		status;

	pvximdi = epvxiPMsgConfig(la);
	if(!pvximdi){
		return VXI_ERR_FETCH_FAIL;
	}

	status = epvxiCmdQuery(	
			la,
			(unsigned long)MBC_READ_PROTOCOL_ERROR,
			&error);
	if(status>=0){
		logMsg("%s: serial protocol error (code = %x)\n", 
			__FILE__,
			error);
	}
	else{
		logMsg(	"%s: serial protocol error fetch failed\n",
			__FILE__);
		return VXI_ERR_FETCH_FAIL;
	}

	pcsr = VXIBASE(la);
	resp = pcsr->dir.r.dd.msg.response;

	if(resp & VXIERRNOTMASK){
		pvximdi->err = FALSE;
	}
	else{
		logMsg(	"%s: Device failed to clear its ERR bit (la=%d)\n",
			__FILE__,
			la);
	}

	switch(error){
	case MBE_MULTIPLE_QUERIES:
		return VXI_MULTIPLE_QUERIES;
	case MBE_UNSUPPORTED_CMD:
		return VXI_UNSUPPORTED_CMD;
	case MBE_DIR_VIOLATION:
		return VXI_DIR_VIOLATION;
	case MBE_DOR_VIOLATION:
		return VXI_DOR_VIOLATION;
	case MBE_RR_VIOLATION:
		return VXI_RR_VIOLATION;
	case MBE_WR_VIOLATION:
		return VXI_WR_VIOLATION;
	case MBE_NO_ERROR:
	default:
		break;
	}
	return VXI_ERR_FETCH_FAIL;
}


/*
 *
 * vxiMsgInt
 *
 *
 */
LOCAL 
void 	vxiMsgInt(
	unsigned 	la
)
{
	VXIMDI		*pvximdi;
	int		status;
	
	/*
	 * verify that this device is open for business
	 */
	pvximdi = epvxiPMsgConfig(la);
	if(pvximdi){

		/*
		 *
		 * wakeup pending tasks
		 *
		 */
		status = semGive(pvximdi->syncSem);
		if(status<0){
			logMsg("%s: vxiMsgInt(): bad sem id\n",
			__FILE__); 
		}
	}
	else{
		logMsg(	"%s: vxiMsgInt(): msg int to ukn or closed dev\n",
			__FILE__);
	}
}



/*
 * signalHandler
 */
LOCAL
void signalHandler(
	unsigned short	signal
)
{
	unsigned 	signal_la;

	signal_la = signal & NVXIADDR;

	if(MBE_EVENT_TEST(signal)){
		logMsg(	"%s: VXI event was ignored %x\n", 
			__FILE__,
			signal);
	}
	else{
		vxiMsgInt(signal_la);
	}
}
