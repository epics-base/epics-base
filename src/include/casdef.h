/*
 *	$Id$
 *
 *      Author: Jeffrey O. Hill
 *              johill@lanl.gov
 *              (505) 665 1831
 *      Date:   1-95
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
 * 	Modification Log:
 * 	-----------------
 * 	$Log$
 * Revision 1.3  1995/10/12  01:27:49  jhill
 * doc
 *
 * Revision 1.2  1995/09/29  21:35:33  jhill
 * API clean up
 *
 * Revision 1.1  1995/08/12  00:13:33  jhill
 * installed into cvs
 *
 * TODO:
 * .01 	Do we need lock/unlock routines so that the application
 *	can write several fields at once? If so then we will need
 *	auto lock TMO.
 * .02 	Is a bit mask the proper way to express the intrest type?
 * .03	Use special configuration notify events (ala X) to update
 *	the application when the units, limits, etc change?
 * .04  certain things like native type cant be changed during
 *	pv id's life time or we will be required to have locking
 * .05	remove "native" from routine names below since this is
 *	redundant?
 * .06	They should be able to pass in a pointer to their structure
 *	when creating the server and then this should be passed
 *	back when their entry points are called.
 */

#include <epicsTypes.h>	/* EPICS arch independent types */
#include <tsDefs.h>	/* EPICS time stamp */
#include <alarm.h>	/* EPICS alarm severity and alarm condition */
#include <errMdef.h>	/* EPICS error codes */

typedef int		caStatus;

typedef unsigned	caId;

typedef union {
	void		*ptr;
	unsigned	uint;
}appId;

typedef struct{
	unsigned long	sec;	/* seconds */
	unsigned long	nsec;	/* nano - seconds */
}caTime;

typedef struct cas_value_log {
	epicsAny		value;
	epicsAlarmSeverity	severity;
	epicsAlarmCondition	condition;
	TS_STAMP		time;
}casValueLog;

/*
 * ndim == 0 => scaler 
 * otherwise pIndexArray points to an array of ndim items
 */
#define nDimScaler 0U
typedef struct cas_io_context {
	epicsArrayIndex	*pIndexArray;	/* pointer to index array */
	unsigned	nDim;		/* number of dimensions */
}casCtx;

/*
 * ===========================================================
 * provided by the application (and called by the server via 
 * a jump table)
 *
 * All of these operations need to return without delay
 * if we are to avoid hanging a single threaded server.
 * ===========================================================
 */

/*
 * Canonical name is copied by the application into the buf pointed to by 
 * officialPVNameBuf. Copy no more than officialPVNameBufSize
 * characters and null terminate. 
 *
 * This allows for PV name aliases.
 *
 * If an operation will not complete immediately then the application is 
 * required to return S_casApp_asyncCompletion and then call 
 * caServerPVExistTestDone () when the operation actually completes.
 */
typedef caStatus pvExistTest (const char *pPVName, char *officialPVNameBuf, 
		unsigned officialPVNameBufSize, caId ioId);

/* 
 * The application supplied entry point pvAddrCreate() will
 * be called by the server library each time that
 * CA attaches a channel to a process varible for 
 * the first time.
 *
 * The application typically will allocate an application specific 
 * address structure with caMalloc() and return a pointer to it 
 * in "*pPVId". 
 */
typedef caStatus pvAddrCreate (const char *pvNameString, caId pvId,
					appId *pPVId);

/* 
 * The application supplied entry point pvAddrDelete() will
 * be called by the server library each time that
 * the number of CA attachments to a particular PV
 * decrements to zero. This routine provide an opportunity 
 * for the application to delete any resources allocated 
 * during pvAddrCreate(). Typically caFree() will be called
 * to free memmory allocated inside of pvAddrCreate() above.
 */
typedef caStatus pvAddrDelete (appId pvId);



/* 
 * The application supplied entry point pvInterestRegister ()
 * will be called each time that the server wishes to 
 * subscripe for PV value change events via caServerPostEvents() 
 */
typedef caStatus pvInterestRegister (appId pvId, unsigned select);

/* 
 * The application supplied entry point pvInterestDelete ()
 * will be called each time that the server wishes to
 * remove its subscription for PV value change events 
 * via caServerPostEvents() 
 */
typedef caStatus pvInterestDelete (appId pvId);

/* 
 * If an operation will not complete immediately
 * then the application is required to return
 * S_casApp_asyncCompletion and then call 
 * caServerPostPVIOCompletion ()
 * when the operation actually completes.
 */
typedef caStatus pvWrite (appId pvId, const casCtx *pCtx, 
		const epicsAny *pValue, caId ioId);

/*
 * If an operation will not complete immediately then the application 
 * is required to return S_casApp_asyncCompletion and then call 
 * caServerPostPVIOCompletion () when the operation actually completes.
 *
 * If it is an array operation then the array entry in epicsAny
 * in casValueLog is initialized by the application to point
 * to a segement structure (see epicsTypes.h). The segment
 * structure is initialized to point to one or more buffers
 * containing the array. The free call back function pointer
 * in the segment structure is initialized by the application
 * to point to a funtion to be called when the ca server library
 * has finished copying the data at some time in the (potentially
 * distant) future.
 */
typedef caStatus pvRead (appId pvId, const casCtx *pCtx, 
		casValueLog *pLog, caId ioId);

/* 
 * the following are not allowed to change during an PV id's lifetime
 * (post a PV delete event if these items need to be changed)
 */
typedef epicsType pvNativeType (appId pvId);

/*
 * return zero if it is is a scaler
 */
typedef unsigned pvMaxNativeDimension (appId pvId);
/*
 * This is called only if pvMaxNativeDimension() above indicates
 * that the PV isnt a scaler quantity.
 */
typedef epicsIndex pvMaxNativeElementCount (appId pvId, unsigned dim);
typedef unsigned pvMaxSimultAsyncOps (appId pvId);
typedef	epicsType pvBestExternalType (appId pvId);	
typedef void pvUnits (appId pvId, unsigned bufByteSize, char *pStringBuf);
typedef unsigned pvEnumStateCount (appId pvId);
typedef void pvEnumStateString (appId pvId, unsigned stateNumber,
				unsigned bufByteSize, char *pStringBuf);
typedef unsigned pvPrecision (appId pvId);
typedef void pvGraphLimits (appId pvId, double *pUpperLimit, double *pLowerLimit);
typedef void pvControlLimits (appId pvId, double *pUpperLimit, double *pLowerLimit);
typedef void pvAlarmLimits (appId pvId, double *pUpperAlarmLimit, 
			double *pUpperWarningLimit, double *pLowerWarningLimit, 
			double *pLowerAlarmLimit);



/* 
 * Optional per channel interface
 * 
 * Per channel state required if the application
 * implements access control
 */
typedef caStatus chAddrCreate (appId pvId, caId chId, appId *pChanId);
typedef caStatus chAddrDelete (appId ChId);
typedef caStatus chSetOwner (appId ChId, const char *pUser, const char *pHost);
#define channelInterest_AccessRights (1<<0)
typedef caStatus chInterestRegister (appId ChId, unsigned select);
typedef caStatus chInterestDelete (appId ChId);
typedef epicsBoolean chReadAccess (appId ChId);
typedef epicsBoolean chWriteAccess (appId ChId);

/*
 * If an entry point isnt supplied by the application
 * (set to NULL) then the server lib will fill in 
 * a default action
 */
typedef struct cas_application_entry_table{
	pvExistTest		*pPVExistTest;
	pvAddrCreate		*pPVAddrCreate;
	pvAddrDelete		*pPVAddrDelete;

	pvInterestRegister	*pPVInterestRegister;
	pvInterestDelete	*pPVInterestDelete;

	pvWrite			*pPVWrite;
	pvRead			*pPVRead;

	pvNativeType		*pPVNativeType;
	pvBestExternalType	*pPVBestExternalType;
	pvMaxNativeDimension	*pPVMaxNativeDimension;
	pvMaxNativeElementCount	*pPVMaxNativeElementCount;
	pvMaxSimultAsyncOps	*pPVMaxSimultAsyncOps;
	pvUnits			*pPVUnits;
	pvPrecision		*pPVPrecision;
	pvEnumStateCount	*pPVEnumStateCount;
	pvEnumStateString	*pPVEnumStateString;
	pvGraphLimits		*pPVGraphLimits;
	pvControlLimits		*pPVControlLimits;
	pvAlarmLimits		*pPVAlarmLimits;

	chAddrCreate		*pChAddrCreate;
	chAddrDelete		*pChAddrDelete;
	chSetOwner		*pChSetOwner;
	chInterestRegister	*pChInterestRegister;
	chInterestDelete	*pChInterestDelete;
	chReadAccess		*pChReadAccess;
	chWriteAccess		*pChWriteAccess;
}casAppEntryTable;

/*
 * ===========================================================
 * called by the application
 * ===========================================================
 */

/*
 * Context used when creating a server.
 * Set appropriate flag in the "flags"
 * field for each parameter that does
 * not default.
 */

/*
 * the estimated number of proces variables
 */
#define casCreateCtx_pvCountEstimate	(1<<1)	/*default = ??? */

/*
 * the maximum number of characters in a pv name
 */
#define casCreateCtx_pvMaxNameLength	(2<<1) 	/* required */

/*
 * max number of IO ops pending simultaneously 
 * (for operations that are not directed at a particular PV)
 */
#define casCreateCtx_maxSimulIO		(3<<1) 	/* default = 1 */

typedef struct {
	unsigned 	flags;
	unsigned	pvCountEstimate;
	unsigned	pvMaxNameLength;
	unsigned	maxSimulIO;
}casCreateCtx;

void 		*caMalloc (size_t size);
void 		*caCalloc (size_t count, size_t size);
void 		caFree (const void *pBlock);

/*
 * NOTE: Always force casAppEntryTable to zero prior to initializatiion
 * so that if new entries are added to the bottom of the structure 
 * existing code will specify nill entries for any new entry points
 * and therefore take the default.
 */
typedef const void  *caServerId;
caStatus 	caServerCreate (const casAppEntryTable *pTable, 
			const casCreateCtx *pCtx, caServerId *pId);
caStatus 	caServerDelete (caServerId id);
caStatus	caServerProcess (caServerId id, const caTime *pDelay);
typedef unsigned 	caServerTimerId;
caStatus	caServerAddTimeout (caServerId id, const caTime *pDelay,
			void (*pFunc)(void *pParam), void *pParam, 
			caServerTimerId *pAlarmId);
caStatus	caServerDeleteTimeout (caServerId id, caServerTimerId alarmId);

caStatus 	caServerSetDebugLevel (caServerId id, unsigned level);
caStatus	caServerShow (caServerId id, unsigned level);	

/*
 * Most application will call this function when a PV's value is modified.
 * (pvId is in the ctx structure when the pv is created)
 */
caStatus caServerPostMonitorEvent (caServerId casid, caId pvId, 
		const casValueLog *pLog, unsigned select);

/*
 * Some application will call this function to request that
 * a PV address is to be deleted.
 * (pvId is in the ctx structure when the pv is created)
 */
caStatus caServerPostDeleteEvent (caServerId casid, caId pvId);

/*
 * Some application will call this function to asynchronously complete
 * a PV exist test operation.
 * (ioId is in the ctx structure when the opertion is initiated by
 * the server)
 */
caStatus caServerPVExistTestDone (caServerId casid, caId ioId, 
		const char *pOfficialName, caStatus status);

/*
 * Some application will call this function to asynchronously complete
 * a PV address create operation.
 * (ioId is in the ctx structure when the opertion is initiated by
 * the server)
 */
caStatus caServerPVAddrCreateDone (caServerId casid, caId ioId, 
		const void *pPVAddr, caStatus status);

/*
 * Some application will call this function to asynchronously complete
 * a PV address delete operation.
 * (ioId is in the ctx structure when the opertion is initiated by
 * the server)
 */
caStatus caServerPVAddrDeleteDone (caServerId casid, caId ioId, caStatus status);

/*
 * Some application will call this function to asynchronously complete
 * a PV write operation.
 * (ioId is in the ctx structure when the opertion is initiated by
 * the server)
 */
caStatus caServerPVWriteDone (caServerId casid, caId ioId, caStatus status);

/*
 * Some application will call this function to asynchronously complete
 * a PV read operation.
 * (ioId is in the ctx structure when the opertion is initiated by
 * the server)
 */
caStatus caServerPVReadDone (caServerId casid, caId ioId, const casValueLog *pLog, 
					caStatus status);

/*
 * ===========================================================
 * for internal use by the server library 
 * (and potentially returned to the server application)
 * ===========================================================
 */
#define S_cas_success 0
#define S_cas_internal (M_cas| 1) /*Internal failure*/
#define S_cas_noMemory (M_cas| 2) /*Memory allocation failed*/
#define S_cas_portInUse (M_cas| 3) /*IP port already in use*/
#define S_cas_hugeRequest (M_cas | 4) /*Requested op does not fit*/
#define S_cas_sendBlocked (M_cas | 5) /*Blocked for send q space*/
#define S_cas_badElementCount (M_cas | 6) /*Bad element count*/
#define S_cas_noConvert (M_cas | 7) /*No conversion between src & dest types*/
#define S_cas_badWriteType (M_cas | 8) /*Src type inappropriate for write*/
#define S_cas_ioBlocked (M_cas | 9) /*Blocked for io completion*/
#define S_cas_partialMessage (M_cas | 10) /*Partial message*/
#define S_cas_noContext (M_cas | 11) /*Context parameter is required*/
#define S_cas_disconnect (M_cas | 12) /*Lost connection to server*/
#define S_cas_recvBlocked (M_cas | 13) /*Recv blocked*/
#define S_cas_badType (M_cas | 14) /*Bad data type*/
#define S_cas_timerDoesNotExist (M_cas | 15) /*Timer does not exist*/
#define S_cas_badEventType (M_cas | 16) /*Bad event type*/
#define S_cas_badResourceId (M_cas | 17) /*Bad resource identifier*/
#define S_cas_chanCreateFailed (M_cas | 18) /*Unable to create channel*/


/*
 * ===========================================================
 * returned by the application (to the server library)
 * ===========================================================
 */
#define S_casApp_success 0 
#define S_casApp_noMemory (M_casApp | 1) /*Memory allocation failed*/
#define S_casApp_pvNotFound (M_casApp | 2) /*PV not found*/
#define S_casApp_badPVId (M_casApp | 3) /*Unknown PV identifier*/
#define S_casApp_noSupport (M_casApp | 4) /*No application support for op*/
#define S_casApp_asyncCompletion (M_casApp | 5) /*Operation will complete asynchronously*/
#define S_casApp_badDimension (M_casApp | 6) /*bad matrix size in request*/

