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
 * 	Revision 1.4  1996/07/01 19:56:15  jhill
 * 	one last update prior to first release
 *
 * 	Revision 1.3  1996/06/26 23:08:55  jhill
 * 	took path out of casInternal.h include
 *
 * 	Revision 1.2  1996/06/20 18:09:43  jhill
 * 	changed where casInternal comes from
 *
 * 	Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * 	ca server installation
 *
 *
 * TODO:
 * .03  document new event types for limits change etc
 * .04  certain things like native type cant be changed during
 *	pv id's life time or we will be required to have locking
 *	(doc this)
 * .08	Need a mechanism by which an error detail string can be returned
 *	to the server from a server app (in addition to the normal
 *	error constant)
 * .10 	Need a new env var in which the app specifies a set of interfaces
 *	to use - or perhaps just a list of networks that we will accept
 *	clients from.
 * .12 	Should the server have an interface so that all PV names
 *	can be obtained (even ones created after init)? This
 *	would be used to implement update of directory services and 
 * 	wild cards? Problems here with knowing PV name structure and 
 *	maximum permutations of name components.
 * .16 	go through this file and make sure that we are consistent about
 *	the use of const - use a pointer only in spots where NULL is
 *	allowed?
 * NOTES:
 * .01  When this code is used in a multi threaded situation we
 * 	must be certain that the derived class's virtual function 
 * 	are not called between derived class destruction and base
 * 	class destruction (or prevent problems if they are).
 *	Possible solutions
 *	1) in the derived classes destructor set a variable which
 * 	inhibits use of the derived classes virtual fnction.
 *	Each virtual function must check this inhibit bit and return 
 *	an error if it is set
 *	2) call a method on the base which prevents further use
 * 	of it by the server from the derived class destructor.
 *	3) some form of locking (similar to (1) above)
 */



#ifndef includecasdefh
#define includecasdefh

//
// EPICS
//
#include <alarm.h>		// EPICS alarm severity/condition 
#include <errMdef.h>		// EPICS error codes 
#include <gdd.h> 		// EPICS data descriptors 

typedef aitUint32 caStatus;

#include <casEventMask.h>	// EPICS event select class 
#include <casInternal.h>	// CA server private 

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
#define S_cas_noRead (M_cas | 19) /*read access denied*/
#define S_cas_noWrite (M_cas | 20) /*write access denied*/
#define S_cas_noEventsSelected (M_cas | 21) /*no events selected*/
#define S_cas_noFD (M_cas | 22) /*no file descriptors available*/
#define S_cas_badProtocol (M_cas | 23) /*protocol from client was invalid*/
#define S_cas_redundantPost (M_cas | 24) /*redundundant io completion post*/
#define S_cas_badPVName (M_cas | 25) /*bad PV name from server tool*/
#define S_cas_badParameter (M_cas | 26) /*bad parameter from server tool*/
#define S_cas_validRequest (M_cas | 27) /*valid request*/
#define S_cas_tooManyEvents (M_cas | 28) /*maximum simult event types exceeded*/


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
#define S_casApp_canceledAsyncIO (M_casApp | 7) /*asynchronous io canceled*/
#define S_casApp_outOfBounds (M_casApp | 8) /*operation was out of bounds*/
#define S_casApp_undefined (M_casApp | 9) /*undefined value*/


//
// casAsyncIO - Channel Access Asynchronous IO API Class
//
// The following virtual functions allow for asynchronous completion:
// 	caServer::pvExistTest()
// 	casPV::read()
// 	casPV::write()
// To initiate asynchronous completion create a casAsyncIO object 
// inside one of the above virtual functions and return the status code
// S_casApp_asyncCompletion
//
// All asynchronous completion data must be returned in the 
// gdd provided in the virtual functions parameters. 
//
// Deletion Responsibility
// -------- --------------
// o the server lib will not call "delete" directly for any
// casAsyncIO created by the server tool because we dont know 
// that "new" was called to create the object.
// o The server tool is responsible for reclaiming storage for any
// casAsyncIO it creates. The destroy virtual function will
// assist the server tool with this responsibility. The 
// virtual function casAsyncIO::destroy() does a "delete this".
// o Avoid deleting the casAsyncIO immediately after calling
// postIOCompletion(). Instead proper operation requires that
// the server tool wait for the server lib to call destroy after 
// the response is succesfully queued to the client
// o If for any reason the server tool needs to cancel an IO
// operation then it should post io completion with status
// S_casApp_canceledAsyncIO. Deleting the asyncronous io
// object prior to its being allowed to forward an IO termination 
// message to the client will result in NO IO CALL BACK TO THE
// CLIENT PROGRAM (in this situation a warning message will be printed by 
// the server lib).
//
class casAsyncIO : private casAsyncIOI {
public:
	//
	// casAsyncIO()
	//
	// Any DD ptr supplied here is used if postIOCompletion()
	// is called with a nill DD pointer
	//
        casAsyncIO(const casCtx &ctx, gdd *pValue=0) : 
		casAsyncIOI(ctx, *this,  pValue) {}

	//
	// force virtual destructor 
	//
	virtual ~casAsyncIO() {}

	//
	// called by the server lib after the response message
	// is succesfully queued to the client or when the
	// IO operation is canceled (client disconnects etc).
	//
	// default destroy executes a "delete this".
	//
	virtual void destroy();

	//
	// place notification of IO completion on the event queue
	// (this function does not delete the casAsyncIO object). 
	// Only the first call to this function has any effect.
	//
	caStatus postIOCompletion(caStatus completionStatusIn, gdd *pValue=0)
	{
		return this->casAsyncIOI::postIOCompletion(completionStatusIn, pValue);
	}

	//
	// Find the server associated with this async IO 
	// ****WARNING****
	// this returns NULL if the async io isnt currently installed 
	// into a server
	// ***************
	//
	caServer *getCAS()
	{
		return this->casAsyncIOI::getCAS();
	}

	//
	// return gdd DD ptr stored in base
	// (a mechanism to avoid duplicate storage of DD ptr
	// - in the base and in the derived)
	//
	gdd *getValuePtr ()
	{
		return this->casAsyncIOI::getValuePtr();
	}

	//
	// release any data optionally attached to the asynchronous IO
	// object by the constructor - used when the asynchronous
	// io saves data but does not return datai with the post
	// (ie write)
	//
	void clrValue()
	{
		this->casAsyncIOI::clrValue();
	}
};

//
// caServer - Channel Access Server API Class
//
class caServer {
private:
	//
	// this private data member appears first so that
	// initialization of the constant event masks below
	// uses this member only after it has been intialized
	//
	// We do not use private inheritance here in order
	// to avoid os/io dependent -I during server tool compile
	//
	caServerI  *pCAS;
public:
        caServer (unsigned pvMaxNameLength, unsigned pvCountEstimate=0x3ff,
                                unsigned maxSimultaneousIO=1u);
	virtual ~caServer();

	//
	// Need VF that returns pointer to derived type ?
	//

	//caStatus enableClients ();
	//caStatus disableClients ();

        void setDebugLevel (unsigned level);
        unsigned getDebugLevel ();

	casEventMask registerEvent (const char *pName);

        //
	// show()
        //
        virtual void show (unsigned level);

        //
        // The server tool is encouraged to accept multiple PV name
        // aliases for the same PV here. However, a unique canonical name
	// must be selected for each PV.
	//
	// returns S_casApp_success and fills in canonicalPVName
	// if the PV is in this server tool
	//
	// returns S_casApp_pvNotFound if the PV does not exist in
	// the server tool
	//
	// The server tool returns the unique canonical name for 
	// the pv into the gdd. A gdd is used here becuase this 
	// operation is allowed to complete asynchronously.
        //
        virtual caStatus pvExistTest (const casCtx &ctx, const char *pPVName,
                        	gdd &canonicalPVName) = 0;

        //
        // createPV() is called each time that a PV is attached to
        // by a client for the first time. The server tool must create
        // a casPV object (or a derived class) each time that this
        // routine is called
        //
        virtual casPV *createPV (const casCtx &ctx, const char *pPVName)=0;

	//
	// common event masks 
	// (what is currently used by the CA clients)
	//
	const casEventMask	valueEventMask; // DBE_VALUE
	const casEventMask	logEventMask; 	// DBE_LOG
	const casEventMask	alarmEventMask; // DBE_ALARM
};

//
// casPV() - Channel Access Server Process Variable API Class
//
// Deletion Responsibility
// -------- --------------
// o the server lib will not call "delete" directly for any
// casPV created by the server tool because we dont know 
// that "new" was called to create the object
// o The server tool is responsible for reclaiming storage for any
// casPV it creates. The destroy() virtual function will
// assist the server tool with this responsibility. The
// virtual function casPV::destroy() does a "delete this".
// o The destructor for this object will cancel any
// client attachment to this PV (and reclaim any resources
// allocated by the server library on its behalf)
//
class casPV : private casPVI {
public:
	casPV (const casCtx &ctx, const char * const pPVName);
        virtual ~casPV ();

	//
	// This is called for each PV in the server if
	// caServer::show() is called and the level is high 
	// enough
	//
	virtual void show (unsigned level);

        //
        // The maximum number of simultaneous asynchronous IO operations
        // allowed for this PV
        //
        virtual unsigned maxSimultAsyncOps () const;

        //
        // Called by the server libary each time that it wishes to
        // subscribe for PV change notification from the server 
	// tool via postEvent() below.
	//
        virtual caStatus interestRegister ();

        //
        // called by the server library each time that it wishes to
        // remove its subscription for PV value change events
        // from the server tool via caServerPostEvents()
        //
        virtual void interestDelete ();

        //
        // called by the server library immediately before initiating
        // a tranaction (PV state must not be modified during a
        // transaction)
        //
	// HINT: their may be many read/write operatins performed within
	// a single transaction if a large array is being transferred
	//
        virtual caStatus beginTransaction ();

        //
        // called by the server library immediately after completing
        // a tranaction (PV state modification may resume after the
        // transaction completes)
        //
        virtual void endTransaction ();

	//
	// read
	//
	// this is allowed to complete asychronously
	//
	// RULE: if this completes asynchronously and the server tool references
	// its data into the prototype descriptor passed in the args to read() 
	// then this data must _not_ be modified while the reference count 
	// on the prototype is greater than zero.
	//
	virtual caStatus read (const casCtx &ctx, gdd &prototype);

	//
	// write 
	//
	// this is allowed to complete asychronously
	// (ie the server tool is allowed to cache the data and actually
	// complete the write operation at some time in the future)
	//
	virtual caStatus write (const casCtx &ctx, gdd &value);

        //
        // chCreate() is called each time that a PV is attached to
        // by a client. The server tool may choose not to
	// implement this routine (in which case the channel
	// will be created by the server). If the server tool
	// implements this function then it must create a casChannel object
        // (or a derived class) each time that this routine is called
        //
	virtual casChannel *createChannel (const casCtx &ctx,
		const char * const pUserName, const char * const pHostName);

        //
        // destroy() is called 
	// 1) each time that a PV transitions from
	// a situation where clients are attached to a situation
	// where no clients are attached.
	// 2) once for all PVs that exist when the server is deleted
	//
	// the default destroy() executes "delete this"
        //
        virtual void destroy ();

	//
	// tbe best type for clients to use when accessing the
	// value of the PV
	//
	virtual aitEnum bestExternalType ();

        //
        // Server tool calls this function to post a PV event.
        //
        void postEvent (const casEventMask &select, gdd &event);

	//
	// peek at the pv name
	//
	//inline char *getName() const 

	//
	// Find the server associated with this PV
	// ****WARNING****
	// this returns NULL if the PV isnt currently installed 
	// into a server (this situation will exist only if
	// the pv isnt deleted in a derived classes replacement
	// for virtual casPV::destroy() 
	// ***************
	//
	caServer *getCAS();
};

//
// casChannel - Channel Access Server - Channel API Class
//
// Deletion Responsibility
// -------- --------------
// o the server lib will not call "delete" directly for any
// casChannel created by the server tool because we dont know 
// that "new" was called to create the object
// o The server tool is responsible for reclaiming storage for any
// casChannel it creates. The destroy() virtual function will
// assist the server tool with this responsibility. The
// virtual function casChannel::destroy() does a "delete this".
// o The destructor for this object will cancel any
// client attachment to this channel (and reclaim any resources
// allocated by the server library on its behalf)
//
class casChannel : private casPVListChan {
public:
	casChannel(const casCtx &ctx);
        virtual ~casChannel();

	//
	// Called when the user name and the host name are changed
	// for a live connection.
	//
	virtual void setOwner(const char * const pUserName, 
			const char * const pHostName);

        //
	// called when the first client begins to monitor the PV
        //
        virtual caStatus interestRegister(); 

	//
	// called when the last client stops monitoring the PV
	//
        virtual void interestDelete();

	//
	// the following are encouraged to change during an channel's
	// lifetime
	//
        virtual aitBool readAccess () const;
        virtual aitBool writeAccess () const;
	// return true to hint that the opi should ask the operator
	// for confirmation prior writing to this PV
        virtual aitBool confirmationRequested () const;

	//
	// This is called for each channel in the server if
	// caServer::show() is called and the level is high 
	// enough
	//
	virtual void show(unsigned level);

        //
        // destroy() is called when 
	// 1) there is a client initiated channel delete 
	// 2) there is a server tool initiaed PV delete
	// 3) there is a server tool initiated server delete
        //
	// the casChannel::destroy() executes a "delete this"
	//
	virtual void destroy();

	//
	// server tool calls this to indicate change of channel state
	// (ie access rights changed)
	//
        void postEvent (const casEventMask &select, gdd &event);

	//
	// Find the PV associated with this channel 
	// ****WARNING****
	// this returns NULL if the channel isnt currently installed 
	// into a PV (this situation will exist only if
	// the channel isnt deleted in a derived classes replacement
	// for virtual casChannel::destroy() 
	// ***************
	//
	casPV *getPV();
};

#endif /* ifdef includecasdefh (this must be the last line in this file) */

