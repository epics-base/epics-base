/*
 * base/src/drv $Id$
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
 * 	.06 joh 072393	better test for nonexistent device during
 *			msg open
 *
 *	Improvements
 *	------------
 *	.01 joh 051992	Some work on fast handshake exists in this file.
 *			If a card with fast handshake is found to exist
 *			then this code will be tested and uncommented.
 *
 */

static char	*sccsId = "@(#)drvEpvxiMsg.c	1.15\t8/27/93";

#include <vxWorks.h>
#include <iv.h>
#include <semLib.h>
#include <stdioLib.h>
#include <stdlib.h>
#include <logLib.h>
#include <intLib.h>
#include <sysLib.h>
#include <vxLib.h>

#include <fast_lock.h>
#include <drvEpvxi.h>
#include <drvHp1404a.h>

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


#define abort(A)	taskSuspend(0)

#define epvxiPMsgConfig(LA)\
((VXIMDI *)(epvxiLibDeviceList[LA]?epvxiLibDeviceList[LA]->pMsgConfig:0))

/*
 * local functions
 */
LOCAL void 	set_la(
	unsigned	la,
	unsigned	*pla
);

LOCAL void 	vxiMsgInt(
	unsigned 	la
);

LOCAL void 	signalHandler(
	int16_t 	signal
);

LOCAL EPVXISTAT 	epvxiReadSlowHandshake(
	unsigned        la,
	char            *pbuf,
	unsigned long	count,
	unsigned long	*pread_count,
	unsigned long	option
);

#ifdef FASTHANDSHAKE
LOCAL EPVXISTAT 	epvxiReadFastHandshake(
	unsigned	la,
	char		*pbuf,
	unsigned long	count,
	unsigned long	*pread_count,
	unsigned long	option
);
#endif

LOCAL EPVXISTAT 	vxiMsgClose(
	unsigned        la
);

LOCAL EPVXISTAT 	vxiMsgOpen(
	unsigned	la
);

LOCAL void	vxiMsgSignalSetup(
	void
);

LOCAL void	vxiCPU030MsgSignalSetup(
	void
);

LOCAL void	vxiHP1404MsgSignalSetup(
	void
);

LOCAL EPVXISTAT 	vxiAttemptAsyncModeControl(
	unsigned	la,
	unsigned long	cmd
);

LOCAL EPVXISTAT 	vxiMsgSync(
	unsigned	la,
	unsigned	resp_mask,
	unsigned	resp_state,
	int		override_err
);

LOCAL EPVXISTAT 	fetch_protocol_error(
	unsigned	la
);


/*
 * should be in a header
 */
EPVXISTAT	vxi_msg_test(
	unsigned	la
);

EPVXISTAT	vxi_msg_print_id(
	unsigned	la
);

EPVXISTAT 	vxi_msg_test_protocol_error(
	unsigned	la
);


/*
 *
 * vxi_msg_test()
 *
 */
EPVXISTAT vxi_msg_test(
	unsigned	la
)
{
	char		buf[512];
	unsigned long	count;
	EPVXISTAT	status;

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
EPVXISTAT	vxi_msg_print_id(
	unsigned	la
)
{
        char    	buf[32];
        unsigned long	count;
	char		*pcmd = "*IDN?";
	EPVXISTAT	status;

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
EPVXISTAT 	vxi_msg_test_protocol_error(
	unsigned	la
)
{
	int		i;
	EPVXISTAT	status;

	for(i=0;i<1000;i++){
		status = epvxiCmd(la, MBC_READ_PROTOCOL);
		if(status){
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
EPVXISTAT epvxiCmd(
unsigned	la,
unsigned long	cmd
)
{
        struct vxi_csr		*pcsr;
	VXIMDI			*pvximdi;
	EPVXISTAT		status;

#	ifdef DEBUG
		printf("cmd to be sent %4x (la=%d)\n", cmd, la);
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
       		         status = S_epvxi_unreadData;
       	 	}
	}

	FASTUNLOCK(&pvximdi->lck);

	if(status == S_epvxi_protocolError){
		return fetch_protocol_error(la);
	}

	if(pvximdi->trace){
		printf( "VXI Trace: (la=0X%X) Cmd   -> %x\n",
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
EPVXISTAT epvxiQuery(
unsigned	la,
unsigned long	*presp
)
{
        struct vxi_csr		*pcsr;
	VXIMDI			*pvximdi;
	EPVXISTAT		status;

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
	if(status==VXI_SUCCESS){
		*presp = pcsr->dir.r.dd.msg.dlow;
	}

	FASTUNLOCK(&pvximdi->lck);

#	ifdef DEBUG
		printf("resp returned %4x (la=%d)\n", *presp, la);
#	endif

	if(status == S_epvxi_protocolError){
		return fetch_protocol_error(la);
	}

	if(pvximdi->trace){
		printf( "VXI Trace: (la=0X%X) Query -> %x\n",
			la,
			*presp);
	}

	return status;
}


/*
 * epvxiCmdQuery()
 */
EPVXISTAT epvxiCmdQuery(
unsigned	la,
unsigned long	cmd,
unsigned long 	*presp 
)
{
	EPVXISTAT	status;

	status = epvxiCmd(la, cmd);
	if(status){
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
EPVXISTAT epvxiRead(
unsigned	la,
char		*pbuf,
unsigned long	count,
unsigned long	*pread_count,
unsigned long	option
)
{
	VXIMDI			*pvximdi;
	EPVXISTAT		status;

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
		printf( "VXI Trace: (la=0X%X) Read -> %*s\n",
			la,
			(int)count,
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
LOCAL EPVXISTAT 	epvxiReadFastHandshake(
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
	EPVXISTAT		status;
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
				if(status){
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
				if(status){
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

	if(status == S_epvxi_protocolError){
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
EPVXISTAT 	epvxiReadSlowHandshake(
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
        EPVXISTAT		status;
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
		return S_epvxi_bufferFull;
	}

	FASTLOCK(&pvximdi->lck);

	/*
	 * always leave room to write a NULL termination
	 */
	function_status = S_epvxi_bufferFull;
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
		if(status){
	        	if(pcsr->dir.r.dd.msg.response&VXIREADREADYMASK){
				function_status = S_epvxi_unreadData;
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
		if(status){
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

	if(function_status == S_epvxi_protocolError){
		return fetch_protocol_error(la);
	}

	return function_status;
}


/*
 * epvxiWrite()
 * (set the end bit on the last byte sent)
 */
EPVXISTAT epvxiWrite(
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
	EPVXISTAT		status;
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
		if(status){
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

	if(status == S_epvxi_protocolError){
		return fetch_protocol_error(la);
	}

	if(pvximdi->trace){
		printf( "VXI Trace: (la=0X%X) Write -> %*s\n",
			la,
			(int)count,
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
EPVXISTAT epvxiSetTimeout(
unsigned 	la,
unsigned long	timeout
)
{
	VXIMDI		*pvximdi;
	EPVXISTAT	status;

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
		return S_epvxi_timeoutToLarge;
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
EPVXISTAT epvxiSetTraceEnable(la, enable)
unsigned	la;
int		enable;
{
	VXIMDI		*pvximdi;
	EPVXISTAT	status;

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
EPVXISTAT vxiMsgClose(
unsigned        la
)
{
	EPVXISTAT	status;
	VXIMDI		*pvximdi;

	pvximdi = epvxiPMsgConfig(la);
	if(!pvximdi){
		return S_epvxi_notOpen;
	}

	status = semDelete(pvximdi->syncSem);
	if(status){
		errMessage(
			S_epvxi_internal,
			"vxiMsgClose(): bad sem id");
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
EPVXISTAT 	vxiMsgOpen(
	unsigned	la
)
{
	EPVXISTAT 	status;
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
	if(pvxidi){
		if(pvxidi->pMsgConfig){
			return VXI_SUCCESS;
		}
	}

	/*
	 * standard verification of unknown LA
	 */
	status = epvxiDeviceVerify(la);
	if(status){
		return status;
	}

	pcsr = VXIBASE(la);
	if(VXICLASS(pcsr) != VXI_MESSAGE_DEVICE){
		return S_epvxi_notMsgDevice;
	}

	pvximdi = (VXIMDI *) calloc(1, sizeof(*pvximdi));
	if(!pvximdi){
		return S_epvxi_noMemory;
	}

	pvxidi->pMsgConfig = (void *) pvximdi;

	vxiMsgSignalSetup();

	pvximdi->syncSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if(!pvximdi->syncSem){
		return S_epvxi_noMemory;
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
	if(status){
		/*
		 * All devices are required by the VXI standard 
		 * to accept this command while in the
		 * configure state or in the normal operation
		 * state. Some dont.
		 *
		 */
		errPrintf(
			status,
			__FILE__,
			__LINE__,
			"Device rejected MBC_READ_PROTOCOL (la=0X%X)",
			la);
		return VXI_SUCCESS;
	}

return VXI_SUCCESS;

	if(!MBR_RP_RG(read_proto_resp)){
		return VXI_SUCCESS;
	}

printf("mb device has response gen\n");

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
			printf(	"%s: mb device has int sync!\n",
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
			printf(	"%s: mb device has signal sync!\n",
				__FILE__);
			signalSync = TRUE;
		}
	}

	if(!intSync && !signalSync){
		errMessage(
			S_epvxi_msgDeviceFailure,
			"mb responder failed to configure");
		return VXI_SUCCESS;
	}

	cmd = 	MBC_CONTROL_RESPONSE;
	status = epvxiCmdQuery(
			la,
			cmd,
			&resp);
	if(status){
		errMessage(
			status,
			"Control response rejected by responder");
		vxiMsgClose(la);
		return status;
	}
	if(	MBR_STATUS(resp) != MBR_STATUS_SUCCESS ||
		(resp^cmd)&MBR_CR_CONFIRM_MASK){
		errPrintf(
			S_epvxi_msgDeviceFailure,
			__FILE__,
			__LINE__,
			"Control Response Failed %x", 
			resp);
		return VXI_SUCCESS;
	}
printf("sent ctrl resp (la=%d) (cmd=%x)\n", la, cmd);

printf("synchronized msg based device is ready!\n");

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
		errMessage(
			S_epvxi_noCmdr,
			NULL);
	}
}


/*
 *
 * vxiCPU030MsgSignalSetup 
 *
 *
 */
LOCAL void	vxiCPU030MsgSignalSetup(
	void
)
{
	int		niMsgLA;
	EPVXISTAT	status;

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
	if(status){
		return;
	}

#	define UKN_DEVICE (-2)
	status = (*pnivxi_func[(unsigned)e_SetSignalHandler])(
				UKN_DEVICE, 
				signalHandler);
	if(status){
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
	EPVXISTAT			status;

	dsp.flags = VXI_DSP_make | VXI_DSP_model;
	dsp.make = VXI_MAKE_HP;
	dsp.model = VXI_HP_MODEL_E1404_MSG;
	status = epvxiLookupLA(&dsp, set_la, (void *)&hpMsgLA);
	if(status){
		return;
	}
	if(hpMsgLA<0){
		return;
	}
	dsp.flags = VXI_DSP_make | VXI_DSP_slot;
	dsp.make = VXI_MAKE_HP;
	dsp.slot = epvxiLibDeviceList[hpMsgLA]->slot;
	status = epvxiLookupLA(&dsp, set_la, (void *)&hpRegLA);
	if(status){
		return;
	}

	if(hpRegLA<0){
		return;
	}

	msgCommanderLA = hpMsgLA;
	status = hpE1404SignalConnect(hpRegLA, signalHandler);
	if(status){
		errMessage(status, NULL);
	}

	return;
}


/*
 *
 * set_la
 *
 *
 */
LOCAL void 	set_la(
unsigned	la,
unsigned	*pla
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
LOCAL EPVXISTAT 	vxiAttemptAsyncModeControl(
	unsigned	la,
	unsigned long	cmd
)
{
	EPVXISTAT 	status;
	unsigned long	resp;
	unsigned long   tmpcmd;

	if(msgCommanderLA<0 && cmd&MBC_AMC_RESP_SIGNAL_ENABLE){
		return S_epvxi_badConfig;
	}

	/*
	 * this step tells the device what la to signal at
	 */
	if(cmd & MBC_AMC_RESP_SIGNAL_ENABLE){
		tmpcmd = MBC_IDENTIFY_COMMANDER | msgCommanderLA;
		status = epvxiCmd(
				la,
				tmpcmd);
		if(status){
			errPrintf(
				status,
				__FILE__,
				__LINE__,
				"IDENTIFY_COMMANDER rejected (la=0X%X)",
				la);
			return status;
		}
printf("sent id cmdr (la=0X%X) (cmd=%x)\n", la, tmpcmd);
	}

	status = epvxiCmdQuery(
			la,
			cmd,
			&resp);
	if(status){
		errPrintf(
			status,
			__FILE__,
			__LINE__,
			"Async mode control rejected (la=0X%X)",
			la);
		return status;
	}
	if(	MBR_STATUS(resp) != MBR_STATUS_SUCCESS ||
		(resp^cmd)&MBR_AMC_CONFIRM_MASK){
		status = S_epvxi_msgDeviceFailure;
		errPrintf(
			status,
			__FILE__,
			__LINE__,
			"async mode ctrl failure (la=0X%X,cmd=%x,resp=%x)",
			la,
			cmd,
			resp);
		return status;
	}
printf("sent asynch mode control (la=%d) (cmd=%x)\n",la,cmd);


	if(cmd & MBC_AMC_RESP_INT_ENABLE){
		intConnect(
			INUM_TO_IVEC(la),	
			vxiMsgInt,
			la);	
printf("connected to interrupt (la=%d)\n", la);
	}

	return VXI_SUCCESS;
}


/*
 *
 * vxiMsgSync()
 *
 *
 */
LOCAL EPVXISTAT 	vxiMsgSync(
	unsigned	la,
	unsigned	resp_mask,
	unsigned	resp_state,
	int		override_err
)
{
	VXIMDI		*pvximdi;
	struct vxi_csr	*pcsr;
	EPVXISTAT	status;
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
		printf(	"Syncing to resp mask %4x, request %4x (la=%d)\n",
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
				return S_epvxi_protocolError;
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
			if(status){
				timeout -= VXIMSGSYNCDELAY;
			}
		}
	}
	while(timeout>0);

	/*
	 * sync timed out if we got here
	 */
	status = S_epvxi_deviceTMO;
	errPrintf(
		status,
		__FILE__,
		__LINE__,
		"msg dev timed out after %d sec", 
		(pvximdi->timeout-timeout) / sysClkRateGet());
	errPrintf(
		status,
		__FILE__,
		__LINE__,
		"resp mask %4x, request %4x, actual %4x",
		resp_mask,
		resp_state,
		resp);
	return status;
}


/*
 *
 * fetch_protocol_error
 *
 */
LOCAL EPVXISTAT 	fetch_protocol_error(
	unsigned	la
)
{
	VXIMDI		*pvximdi;
	unsigned long	error;
	struct vxi_csr	*pcsr;
	unsigned short	resp;
	EPVXISTAT	status;

	pvximdi = epvxiPMsgConfig(la);
	if(!pvximdi){
		return S_epvxi_errFetchFailed;
	}

	status = epvxiCmdQuery(	
			la,
			(unsigned long)MBC_READ_PROTOCOL_ERROR,
			&error);
	if(status){
		errMessage(status, "serial protocol error fetch");
		return S_epvxi_errFetchFailed;
	}

	pcsr = VXIBASE(la);
	resp = pcsr->dir.r.dd.msg.response;

	if(resp & VXIERRNOTMASK){
		pvximdi->err = FALSE;
	}
	else{
		errPrintf(
			S_epvxi_msgDeviceFailure,
			__FILE__,
			__LINE__,
			"Device failed to clear its ERR bit (la=0X%X)",
			la);
	}

	switch(error){
	case MBE_MULTIPLE_QUERIES:
		status = S_epvxi_multipleQueries;
		break;
	case MBE_UNSUPPORTED_CMD:
		status = S_epvxi_unsupportedCmd;
		break;
	case MBE_DIR_VIOLATION:
		status = S_epvxi_dirViolation;
		break;
	case MBE_DOR_VIOLATION:
		status = S_epvxi_dorViolation;
		break;
	case MBE_RR_VIOLATION:
		status = S_epvxi_rrViolation;
		break;
	case MBE_WR_VIOLATION:
		status = S_epvxi_wrViolation;
		break;
	case MBE_NO_ERROR:
	default:
		status = S_epvxi_errFetchFailed;
		break;
	}

	errMessage(status, "serial protocol error");
	return status;
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
		semGive(pvximdi->syncSem);
	}
	else{
		logMsg(	
	"%s: vxiMsgInt(): msg int to ukn or closed dev\n",
			(int)__FILE__,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}
}



/*
 * signalHandler
 */
LOCAL
void signalHandler(
int16_t	signal
)
{
	unsigned 	signal_la;

	signal_la = signal & VXIADDRMASK;

	if(MBE_EVENT_TEST(signal)){
		logMsg(	"%s: VXI event was ignored %x\n", 
			(int)__FILE__,
			signal,
			NULL,
			NULL,
			NULL,
			NULL);
	}
	else{
		vxiMsgInt(signal_la);
	}
}
