/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
//
//      Author: Jeffrey O. Hill
//              johill@lanl.gov
//              (505) 665 1831
//      Date:   1-95
//

#ifndef includecasdefh
#define includecasdefh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casdefh
#   undef epicsExportSharedSymbols
#endif

//
// EPICS
//
#include "errMdef.h"            // EPICS error codes 
#include "gdd.h"                // EPICS data descriptors 
#include "alarm.h"              // EPICS alarm severity/condition 

#ifdef epicsExportSharedSymbols_casdefh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "caNetAddr.h"
#include "casEventMask.h"   // EPICS event select class 

typedef aitUint32 caStatus;

/*
 * ===========================================================
 * for internal use by the server library 
 * (and potentially returned to the server tool)
 * ===========================================================
 */
#define S_cas_success 0
#define S_cas_internal (M_cas| 1) /*Internal failure*/
#define S_cas_noMemory (M_cas| 2) /*Memory allocation failed*/
#define S_cas_bindFail (M_cas| 3) /*Attempt to set server's IP address/port failed*/
#define S_cas_hugeRequest (M_cas | 4) /*Requested op does not fit*/
#define S_cas_sendBlocked (M_cas | 5) /*Blocked for send q space*/
#define S_cas_badElementCount (M_cas | 6) /*Bad element count*/
#define S_cas_noConvert (M_cas | 7) /*No conversion between src & dest types*/
#define S_cas_badWriteType (M_cas | 8) /*Src type inappropriate for write*/
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
#define S_cas_noInterface (M_cas | 29) /*server isnt attached to a network*/
#define S_cas_badBounds (M_cas | 30) /*server tool changed bounds on request*/
#define S_cas_pvAlreadyAttached (M_cas | 31) /*PV attached to another server*/
#define S_cas_badRequest (M_cas | 32) /*client's request was invalid*/
#define S_cas_invalidAsynchIO (M_cas | 33) /*inappropriate asynchronous IO type*/
#define S_cas_posponeWhenNonePending (M_cas | 34) /*request postponement, none pending*/

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
#define S_casApp_asyncCompletion (M_casApp | 5) /*will complete asynchronously*/
#define S_casApp_badDimension (M_casApp | 6) /*bad matrix size in request*/
#define S_casApp_canceledAsyncIO (M_casApp | 7) /*asynchronous io canceled*/
#define S_casApp_outOfBounds (M_casApp | 8) /*operation was out of bounds*/
#define S_casApp_undefined (M_casApp | 9) /*undefined value*/
#define S_casApp_postponeAsyncIO (M_casApp | 10) /*postpone asynchronous IO*/

//
// pv exist test return
//
// If the server tool does not wish to start another simultaneous
// asynchronous IO operation or if there is not enough memory
// to do so return pverDoesNotExistHere (and the client will
// retry the request later).
//
enum pvExistReturnEnum { pverExistsHere, pverDoesNotExistHere, 
    pverAsyncCompletion };

class casPV;
class casPVI;
class casCtx;
class casChannel;

class epicsShareClass pvExistReturn { // X aCC 361
public:
    // most server tools will use this
    pvExistReturn ( pvExistReturnEnum s = pverDoesNotExistHere );
    // directory service server tools will use this (see caNetAddr.h)
    pvExistReturn ( const caNetAddr & );
    ~pvExistReturn ();
    const pvExistReturn & operator = ( pvExistReturnEnum rhs );
    const pvExistReturn & operator = ( const caNetAddr & rhs );
    pvExistReturnEnum getStatus () const;
    caNetAddr getAddr () const;
    bool addrIsValid () const;
private:
    caNetAddr address;
    pvExistReturnEnum status;
};

//
// pvAttachReturn
//
class epicsShareClass pvAttachReturn {
public:
    pvAttachReturn ();
    pvAttachReturn ( caStatus statIn );
    pvAttachReturn ( casPV & pv );
    const pvAttachReturn & operator = ( caStatus rhs );
    const pvAttachReturn & operator = ( casPV * pPVIn );
    caStatus getStatus() const ;
    casPV * getPV() const;
private:
    casPV * pPV;
    caStatus stat;
};

//
// caServer - Channel Access Server API Class
//
class caServer {
    friend class casPVI;
public:
    epicsShareFunc caServer ();
    epicsShareFunc virtual ~caServer() = 0;

    //
    // pvExistTest()
    //
    // This function is called by the server library when it needs to 
    // determine if a named PV exists (or could be created) in the 
    // server tool.
    //
    // The request is allowed to complete asynchronously
    // (see Asynchronous IO Classes below).
    //
    // The server tool is encouraged to accept multiple PV name
    // aliases for the same PV here. 
    //
    // example return from this procedure:
    // return pverExistsHere;   // server has PV
    // return pverDoesNotExistHere; // server does know of this PV
    // return pverAsynchCompletion; // deferred result 
    //
    // Return pverDoesNotExistHere if too many simultaneous
    // asynchronous IO operations are pending against the server. 
    // The client library will retry the request at some time
    // in the future.
    //
    epicsShareFunc virtual pvExistReturn pvExistTest ( const casCtx & ctx, 
        const caNetAddr & clientAddress, const char * pPVAliasName );

    //
    // pvAttach() 
    //
    // This function is called _every_ time that a client attaches to the PV.
    // The name supplied here will be either a canonical PV name or an alias 
    // PV name.
    //  
    // The request is allowed to complete asynchronously
    // (see Asynchronous IO Classes below).
    //
    // IMPORTANT: 
    // It is a responsibility of the server tool to detect attempts by 
    // the server library to attach to an existing PV. If the PV does not 
    // exist then the server tool should create it. Otherwise, the server 
    // tool typically will return a pointer to the preexisting PV. 
    //
    // The server tool is encouraged to accept multiple PV name aliases 
    // for the same PV here. 
    //
    // In most situations the server tool should avoid PV duplication 
    // by returning a pointer to an existing PV if the PV alias name
    // matches a preexisting PV's name or any of its aliases.
    //
    // example return from this procedure:
    // return pPV;  // success (pass by pointer)
    // return PV;   // success (pass by ref)
    // return S_casApp_pvNotFound; // no PV by that name here
    // return S_casApp_noMemory; // no resource to create pv
    // return S_casApp_asyncCompletion; // deferred completion
    //
    // Return S_casApp_postponeAsyncIO if too many simultaneous
    // asynchronous IO operations are pending against the server. 
    // The server library will retry the request whenever an
    // asynchronous IO operation (create or exist) completes
    // against the server.
    //
    // In certain specialized rare situations the server tool may choose
    // to create client private process variables that are not shared between
    // clients. In this situation there might be several process variables
    // with the same name. One for each client. For example, a server tool
    // might be written that provides access to archival storage of the
    // historic values of a set of process variables. Each client would 
    // specify its date of interest by writing into a client private process 
    // variable. Next the client would determine the current value of its 
    // subset of named process variables at its privately specified date by 
    // attaching to additional client private process variables.
    //
    epicsShareFunc virtual pvAttachReturn pvAttach ( const casCtx &ctx,
        const char *pPVAliasName );

    //
    // obtain an event mask for a named event type
    // to be used with casPV::postEvent()
    //
    epicsShareFunc casEventMask registerEvent ( const char *pName );

    //
    // common event masks 
    // (what is currently used by the CA clients)
    //
    epicsShareFunc casEventMask valueEventMask () const; // DBE_VALUE 
    epicsShareFunc casEventMask logEventMask () const;  // DBE_LOG 
    epicsShareFunc casEventMask alarmEventMask () const; // DBE_ALARM 
    epicsShareFunc casEventMask propertyEventMask () const; // DBE_PROPERTY

    epicsShareFunc void setDebugLevel ( unsigned level );
    epicsShareFunc unsigned getDebugLevel () const;

    //
    // dump internal state of server to standard out
    //
    epicsShareFunc virtual void show ( unsigned level ) const;

    //
    // server diagnostic counters (allowed to roll over)
    //
    // subscriptionEventsPosted()
    //      - number of events posted by server tool to the event queue
    // subscriptionEventsProcessed() 
    //      - number of events removed by server library from the event queue
    //
    epicsShareFunc unsigned subscriptionEventsPosted () const;
    epicsShareFunc unsigned subscriptionEventsProcessed () const;

    epicsShareFunc class epicsTimer & createTimer ();

    epicsShareFunc void generateBeaconAnomaly ();

    // caStatus enableClients ();
    // caStatus disableClients ();

private:
    class caServerI * pCAS;

    // deprecated interfaces (will be deleted in a future release)
    epicsShareFunc virtual class pvCreateReturn createPV ( const casCtx & ctx,
        const char * pPVAliasName );
    epicsShareFunc virtual pvExistReturn pvExistTest ( const casCtx & ctx, 
        const char * pPVAliasName );
};

//
// casPV() - Channel Access Server Process Variable API Class
//
// Objects of this type are created by the caServer::pvAttach()
// object factory virtual function.
//
// Deletion Responsibility
// -------- --------------
// o the server lib will not call "delete" directly for any
// casPV because we dont know that "new" was called to create 
// the object
// o The server tool is responsible for reclaiming storage for any
// casPV it creates. The destroy() virtual function will
// assist the server tool with this responsibility. The
// virtual function casPV::destroy() does a "delete this".
// o The virtual function "destroy()" is called by the server lib 
// each time that the last client attachment to the PV object is removed. 
// o The destructor for this object will cancel any
// client attachment to this PV (and reclaim any resources
// allocated by the server library on its behalf)
// o If the server tool needs to asynchronously delete an object 
// derived from casPV from another thread then it *must* also 
// define a specialized destroy() method that prevent race conditions 
// occurring when both the server library and the server tool attempt 
// to destroy the same casPV derived object at the same instant.
//
// NOTE: if the server tool precreates the PV during initialization
// then it may decide to provide a "destroy()" implementation in the
// derived class which is a noop.
//
class casPV {
public:
    epicsShareFunc casPV ();
    
    epicsShareFunc virtual ~casPV ();
    
    //
    // This is called for each PV in the server if
    // caServer::show() is called and the level is high 
    // enough
    //
    epicsShareFunc virtual void show ( unsigned level ) const;
    
    //
    // called by the server libary each time that it wishes to
    // subscribe for PV change notification from the server 
    // tool via postEvent() below
    //
    epicsShareFunc virtual caStatus interestRegister ();
    
    //
    // called by the server library each time that it wishes to
    // remove its subscription for PV value change events
    // from the server tool via postEvent() below
    //
    epicsShareFunc virtual void interestDelete ();
    
    //
    // called by the server library immediately before initiating
    // a transaction (PV state must not be modified during a
    // transaction)
    //
    // NOTE: there may be many read/write operations performed within
    // a single transaction if a large array is being transferred
    //
    epicsShareFunc virtual caStatus beginTransaction ();
    
    //
    // called by the server library immediately after completing
    // a tranaction (PV state modification may resume after the
    // transaction completes)
    //
    epicsShareFunc virtual void endTransaction ();
    
    //
    // read
    //
    // The request is allowed to complete asynchronously
    // (see Asynchronous IO Classes below).
    //
    // RULE: if this completes asynchronously and the server tool references
    // its data into the prototype descriptor passed in the args to read() 
    // then this data must _not_ be modified while the reference count 
    // on the prototype is greater than zero.
    //
    // Return S_casApp_postponeAsyncIO if too many simultaneous
    // asynchronous IO operations are pending aginst the PV. 
    // The server library will retry the request whenever an
    // asynchronous IO operation (read or write) completes
    // against the PV.
    //
    // NOTE:
    // o the server tool is encouraged to change the prototype GDD's
    // primitive type to whatever primitive data type is most convient
    // for the server side tool. Conversion libraries in the CA server
    // will convert as necessary to the client's primitive data type.
    //
    epicsShareFunc virtual caStatus read (const casCtx &ctx, gdd &prototype);
    
    //
    // write 
    //
    // The request is allowed to complete asynchronously
    // (see Asynchronous IO Classes below).
    //
    // Return S_casApp_postponeAsyncIO if too many simultaneous
    // asynchronous IO operations are pending aginst the PV. 
    // The server library will retry the request whenever an
    // asynchronous IO operation (read or write) completes
    // against the PV.
    //
    // NOTES:
    // o The incoming GDD with application type "value" is always 
    // converted to the PV.bestExternalType() primitive type.
    // o The time stamp in the incoming GDD is set to the time that
    // the last message was received from the client.
    // o Currently, no container type GDD's are passed here and
    // the application type is always "value". This may change.
    // o The write interface is called when the server receives 
    // ca_put request and the writeNotify interface is called 
    // when the server receives ca_put_callback request.
    // o A writeNotify request is considered complete and therefore
    // ready for asynchronous completion notification when any
    // action that it initiates, and any cascaded actions, complete.
    // o In an IOC context intermediate write requets can be discarded
    // as long as the final writeRequest is always executed. In an
    // IOC context intermediate writeNotify requests are never discarded.
    // o If the service does not implement writeNotify then 
    // the base implementation of casPV :: writeNotify calls 
    // casPV :: write thereby preserving backwards compatibility
    // with the original interface which included a virtual write
    // method but not a virtual writeNotify method.
    //
    epicsShareFunc virtual caStatus write (const casCtx &ctx, const gdd &value);
    epicsShareFunc virtual caStatus writeNotify (const casCtx &ctx, const gdd &value);

    //
    // chCreate() is called each time that a PV is attached to
    // by a client. The server tool may choose not to
    // implement this routine (in which case the channel
    // will be created by the server). If the server tool
    // implements this function then it must create a casChannel object
    // (or a derived class) each time that this routine is called
    //
    epicsShareFunc virtual casChannel * createChannel ( const casCtx &ctx,
        const char * const pUserName, const char * const pHostName );
    
    //
    // tbe best type for clients to use when accessing the
    // value of the PV
    //
    epicsShareFunc virtual aitEnum bestExternalType () const;
    
    //
    // Returns the maximum bounding box for all present and
    // future data stored within the PV. 
    //
    // The virtual function "dimension()" returns the maximum
    // number of dimensions in the hypercube (0=scalar, 
    // 1=array, 2=plane, 3=cube ...}.
    //
    // The virtual function "maxBound(dimension)" returns the 
    // maximum number of elements in a particular dimension 
    // of the hypercube as follows:
    //
    // scalar - maxDimension() returns 0
    //
    // array -  maxDimension() returns 1
    //          maxBounds(0) supplies number of elements in array
    //  
    // plane -  maxDimension() returns 2
    //          maxBounds(0) supplies number of elements in X dimension
    //          maxBounds(1) supplies number of elements in Y dimension
    //
    // cube -   maxDimension() returns 3 
    //          maxBounds(0) supplies number of elements in X dimension
    //          maxBounds(1) supplies number of elements in Y dimension
    //          maxBounds(2) supplies number of elements in Z dimension
    // .
    // .
    // .
    //
    // The default (base) "dimension()" returns zero (scalar).
    // The default (base) "maxBound()" returns one (scalar bounds)
    //       for all dimensions.
    //
    // Clients will see that the PV's data is scalar if
    // these routines are not supplied in the derived class.
    //
    // If the "dimension" argument to maxBounds() is set to
    // zero then the bound on the first dimension is being
    // fetched. If the "dimension" argument to maxBound() is
    // set to one then the bound on the second dimension
    // are being fetched...
    //
    epicsShareFunc virtual unsigned maxDimension () const; // return zero if scalar
    epicsShareFunc virtual aitIndex maxBound ( unsigned dimension ) const;
    
    //
    // destroy() is called 
    // 1) each time that a PV transitions from
    // a situation where clients are attached to a situation
    // where no clients are attached.
    // 2) once for all PVs that exist when the server is deleted
    //
    // the default (base) "destroy()" executes "delete this"
    //
    epicsShareFunc virtual void destroy ();

    //
    // Server tool calls this function to post a PV event.
    //
    epicsShareFunc void postEvent ( const casEventMask & select, const gdd & event );
    
    //
    // peek at the pv name
    //
    // NOTE if there are several aliases for the same PV
    // this routine should return the canonical (base)
    // name for the PV
    //
    // pointer returned must remain valid for the life time
    // o fthe process variable
    //
//
// !! not thread safe !!
//
    epicsShareFunc virtual const char * getName () const = 0;
    
    //
    // Find the server associated with this PV
    // ****WARNING****
    // this returns NULL if the PV isnt currently installed 
    // into a server (this situation will exist if
    // the pv isnt deleted in a derived classes replacement
    // for virtual casPV::destroy() or if the PV is created
    // before the server
    // ***************
    //
    epicsShareFunc caServer * getCAS () const;

    // not to be called by the user
    void destroyRequest ();
    
private:
    class casPVI * pPVI;
    casPV & operator = ( const casPV & );

    friend class casStrmClient;
public:
    //
    // This constructor has been deprecated, and is preserved for 
    // backwards compatibility only. Please do not use it.
    //
    epicsShareFunc casPV ( caServer & );
};

//
// casChannel - Channel Access Server - Channel API Class
//
// Objects of this type are created by the casPV::createChannel()
// object factory virtual function.
//
// A casChannel object is created each time that a CA client 
// attaches to a process variable.
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
// o If the server tool needs to asynchronously delete an object 
// derived from casChannel from another thread then it *must* also 
// define a specialized destroy() method that prevent race conditions 
// occurring when both the server library and the server tool attempt 
// to destroy the same casChannel derived object at the same instant.
//
class casChannel {
public:
    epicsShareFunc casChannel ( const casCtx & ctx );
    epicsShareFunc virtual ~casChannel ();

    //
    // Called when the user name and the host name are changed
    // for a live connection.
    //
    epicsShareFunc virtual void setOwner ( const char * const pUserName, 
        const char * const pHostName );

    //
    // the following are encouraged to change during an channel's
    // lifetime
    //
    epicsShareFunc virtual bool readAccess () const;
    epicsShareFunc virtual bool writeAccess () const;
    // return true to hint that the opi should ask the operator
    // for confirmation prior writing to this PV
    epicsShareFunc virtual bool confirmationRequested () const;

    //
    // If this function is not provided in the derived class then casPV::beginTransaction()
    // is called - see casPV::beginTransaction() for additional comments.
    //
    epicsShareFunc virtual caStatus beginTransaction ();
    
    //
    // If this function is not provided in the derived class then casPV::endTransaction()
    // is called - see casPV::endTransaction() for additional comments.
    //
    epicsShareFunc virtual void endTransaction ();

    //
    // read
    //
    // If this function is not provided in the derived class then casPV::read()
    // is called - see casPV::read() for additional comments.
    //
    epicsShareFunc virtual caStatus read (const casCtx &ctx, gdd &prototype);
    
    //
    // write 
    //
    // If this function is not provided in the derived class then casPV::write()
    // is called - see casPV::write() for additional comments.
    //
    epicsShareFunc virtual caStatus write (const casCtx &ctx, const gdd &value);

    //
    // writeNotify 
    //
    // If this function is not provided in the derived class then casPV::writeNotify()
    // is called - see casPV::writeNotify() for additional comments.
    //
    epicsShareFunc virtual caStatus writeNotify (const casCtx &ctx, const gdd &value);

    //
    // This is called for each channel in the server if
    // caServer::show() is called and the level is high 
    // enough
    //
    epicsShareFunc virtual void show ( unsigned level ) const;

    //
    // destroy() is called when 
    // 1) there is a client initiated channel delete 
    // 2) there is a server tool initiated PV delete
    //
    // the casChannel::destroy() executes a "delete this"
    //
    epicsShareFunc virtual void destroy ();

    //
    // server tool calls this to indicate change in access
    // rights has occurred
    //
    epicsShareFunc void postAccessRightsEvent ();

    //
    // Find the PV associated with this channel 
    // ****WARNING****
    // this returns NULL if the channel isnt currently installed 
    // into a PV (this situation will exist only if
    // the channel isnt deleted in a derived classes replacement
    // for virtual casChannel::destroy() 
    // ***************
    //
    epicsShareFunc casPV * getPV ();

    // not to be called by the user
    void destroyRequest ();

private:
    class casChannelI * pChanI;

    casChannel ( const casChannel & );
    casChannel & operator = ( const casChannel & );
    friend class casStrmClient;
};

//
// Asynchronous IO Classes
//
// The following virtual functions allow for asynchronous completion:
//
//  Virtual Function        Asynchronous IO Class
//  -----------------       ---------------------
//  caServer::pvExistTest() casAsyncPVExistIO
//  caServer::pvAttach()    casAsyncPVAttachIO
//  casPV::read()           casAsyncReadIO
//  casPV::write()          casAsyncWriteIO
//
// To initiate asynchronous completion create a corresponding
// asynchronous IO object from within one of the virtual 
// functions shown in the table above and return the status code
// S_casApp_asyncCompletion. Use the member function 
// "postIOCompletion()" to inform the server library that the 
// requested operation has completed.
//
//
// Deletion Responsibility
// -------- --------------
// o the server lib will not call "delete" directly for any
// casAsyncXxxIO created by the server tool because we dont know 
// that "new" was called to create the object.
// o The server tool is responsible for reclaiming storage for any
// casAsyncXxxxIO it creates. The destroy virtual function will
// assist the server tool with this responsibility. The 
// virtual function casAsyncXxxxIO::destroy() does a "delete this".
// o Avoid deleting the async IO object immediately after calling
// postIOCompletion(). Instead, proper operation requires that
// the server tool wait for the server lib to call destroy after 
// the response is successfully queued to the client
// o If for any reason the server tool needs to cancel an IO
// operation then it should post io completion with status
// S_casApp_canceledAsyncIO. Deleting the asynchronous io
// object prior to its being allowed to forward an IO termination 
// message to the client will result in NO IO CALL BACK TO THE
// CLIENT PROGRAM (in this situation a warning message will be 
// printed by the server lib).
//

//
// casAsyncReadIO 
// - for use with casPV::read()
//
// **Warning**
// The server tool must reference the gdd object
// passed in the arguments to casPV::read() if it is 
// necessary for this gdd object to continue to exist
// after the return from casPV::read(). If this
// is done then it is suggested that this gdd object
// be referenced in the constructor, and unreferenced
// in the destructor, for the class deriving from 
// casAsyncReadIO.
// **
class casAsyncReadIO {
public:

    //
    // casAsyncReadIO()
    //
    epicsShareFunc casAsyncReadIO ( const casCtx & ctx );
    epicsShareFunc virtual ~casAsyncReadIO (); 

    //
    // place notification of IO completion on the event queue
    // (this function does not delete the casAsyncReadIO object)
    //
    // only the first call to this function has any effect
    //
    epicsShareFunc caStatus postIOCompletion ( 
        caStatus completionStatusIn, const gdd & valueRead );

    //
    // Find the server associated with this async IO 
    // ****WARNING****
    // this returns NULL if the async io isnt currently installed 
    // into a server
    // ***************
    //
    epicsShareFunc caServer * getCAS () const;

private:
    class casAsyncReadIOI * pAsyncReadIOI;
    //
    // called by the server lib after the response message
    // is succesfully queued to the client or when the
    // IO operation is canceled (client disconnects etc)
    //
    // default destroy executes a "delete this"
    //
    epicsShareFunc virtual void destroy ();

    casAsyncReadIO ( const casAsyncReadIO & );
    casAsyncReadIO & operator = ( const casAsyncReadIO & );

    void serverInitiatedDestroy ();
    friend class casAsyncReadIOI;
};

//
// casAsyncWriteIO 
// - for use with casPV::write()
//
// **Warning**
// The server tool must reference the gdd object
// passed in the arguments to casPV::write() if it is 
// necessary for this gdd object to continue to exist
// after the return from casPV::write(). If this
// is done then it is suggested that this gdd object
// be referenced in the constructor, and unreferenced
// in the destructor, for the class deriving from 
// casAsyncWriteIO.
// **
//
class casAsyncWriteIO {
public:
    //
    // casAsyncWriteIO()
    //
    epicsShareFunc casAsyncWriteIO ( const casCtx & ctx );
    epicsShareFunc virtual ~casAsyncWriteIO (); 

    //
    // place notification of IO completion on the event queue
    // (this function does not delete the casAsyncWriteIO object). 
    // Only the first call to this function has any effect.
    //
    epicsShareFunc caStatus postIOCompletion ( caStatus completionStatusIn );

    //
    // Find the server associated with this async IO 
    // ****WARNING****
    // this returns NULL if the async io isnt currently installed 
    // into a server
    // ***************
    //
    epicsShareFunc caServer * getCAS () const;

private:
    class casAsyncWriteIOI * pAsyncWriteIOI;
    //
    // called by the server lib after the response message
    // is succesfully queued to the client or when the
    // IO operation is canceled (client disconnects etc)
    //
    // default destroy executes a "delete this"
    //
    epicsShareFunc virtual void destroy ();

    casAsyncWriteIO ( const casAsyncWriteIO & );
    casAsyncWriteIO & operator = ( const casAsyncWriteIO & );

    void serverInitiatedDestroy ();
    friend class casAsyncWriteIOI;
};

//
// casAsyncPVExistIO 
// - for use with caServer::pvExistTest()
//
class casAsyncPVExistIO {
public:

    //
    // casAsyncPVExistIO()
    //
    epicsShareFunc casAsyncPVExistIO ( const casCtx & ctx );
    epicsShareFunc virtual ~casAsyncPVExistIO (); 

    //
    // place notification of IO completion on the event queue
    // (this function does not delete the casAsyncPVExistIO object)
    //
    // only the first call to this function has any effect.
    //
    epicsShareFunc caStatus postIOCompletion ( const pvExistReturn & retValIn );

    //
    // Find the server associated with this async IO 
    // ****WARNING****
    // this returns NULL if the async io isnt currently installed 
    // into a server
    // ***************
    //
    epicsShareFunc caServer * getCAS () const;

private:
    class casAsyncPVExistIOI * pAsyncPVExistIOI;

    //
    // called by the server lib after the response message
    // is succesfully queued to the client or when the
    // IO operation is canceled (client disconnects etc)
    //
    // default destroy executes a "delete this"
    //
    epicsShareFunc virtual void destroy ();
 
    casAsyncPVExistIO ( const casAsyncPVExistIO & );
    casAsyncPVExistIO & operator = ( const casAsyncPVExistIO & );

    friend class casAsyncPVExistIOI;
    void serverInitiatedDestroy ();
};

//
// casAsyncPVAttachIO 
// - for use with caServer::pvAttach()
//
class casAsyncPVAttachIO {
public:
    //
    // casAsyncPVAttachIO()
    //
    epicsShareFunc casAsyncPVAttachIO ( const casCtx & ctx );
    epicsShareFunc virtual ~casAsyncPVAttachIO (); 

    //
    // place notification of IO completion on the event queue
    // (this function does not delete the casAsyncPVAttachIO object). 
    // Only the first call to this function has any effect.
    //
    epicsShareFunc caStatus postIOCompletion ( const pvAttachReturn & retValIn );

    //
    // Find the server associated with this async IO 
    // ****WARNING****
    // this returns NULL if the async io isnt currently installed 
    // into a server
    // ***************
    //
    epicsShareFunc caServer * getCAS () const;

private:
    class casAsyncPVAttachIOI * pAsyncPVAttachIOI;

    //
    // called by the server lib after the response message
    // is succesfully queued to the client or when the
    // IO operation is canceled (client disconnects etc)
    //
    // default destroy executes a "delete this"
    //
    epicsShareFunc virtual void destroy ();

    casAsyncPVAttachIO ( const casAsyncPVAttachIO & );
    casAsyncPVAttachIO & operator = ( const casAsyncPVAttachIO & );

    friend class casAsyncPVAttachIOI;
    void serverInitiatedDestroy ();
};

//
// casAsyncPVCreateIO (deprecated)
// (this class will be deleted in a future release)
//
class casAsyncPVCreateIO : private casAsyncPVAttachIO {
public:
    epicsShareFunc casAsyncPVCreateIO ( const casCtx & ctx );
    epicsShareFunc virtual ~casAsyncPVCreateIO ();
private:
    casAsyncPVCreateIO ( const casAsyncPVCreateIO & );
    casAsyncPVCreateIO & operator = ( const casAsyncPVCreateIO & );
};

//
// pvCreateReturn (deprecated)
// (the class "pvCreateReturn" will be deleted in a future release)
//
class epicsShareClass pvCreateReturn : public pvAttachReturn {
public:
    pvCreateReturn ( caStatus statIn ) : pvAttachReturn ( statIn ) {};
    pvCreateReturn ( casPV & pvIn ) : pvAttachReturn ( pvIn ) {};
};

// TODO:
// .03  document new event types for limits change etc
// .04  certain things like native type cant be changed during
//  pv id's life time or we will be required to have locking
//  (doc this)
// .08  Need a mechanism by which an error detail string can be returned
//  to the server from a server app (in addition to the normal
//  error constant)
// .12  Should the server have an interface so that all PV names
//  can be obtained (even ones created after init)? This
//  would be used to implement update of directory services and 
//  wild cards? Problems here with knowing PV name structure and 
//  maximum permutations of name components.
// .16  go through this file and make sure that we are consistent about
//  the use of const - use a pointer only in spots where NULL is
//  allowed?
// NOTES:
// .01  When this code is used in a multi threaded situation we
//  must be certain that the derived class's virtual function 
//  are not called between derived class destruction and base
//  class destruction (or prevent problems if they are).
//  Possible solutions
//  1) in the derived classes destructor set a variable which
//  inhibits use of the derived classes virtual function.
//  Each virtual function must check this inhibit bit and return 
//  an error if it is set
//  2) call a method on the base which prevents further use
//  of it by the server from the derived class destructor.
//  3) some form of locking (similar to (1) above)
//

#endif // ifdef includecasdefh (this must be the last line in this file) 

