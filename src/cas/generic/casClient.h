
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casClienth
#define casClienth

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casClienth
#   undef epicsExportSharedSymbols
#endif

#include "epicsTime.h"

#ifdef epicsExportSharedSymbols_casClienth
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif


#include "casCoreClient.h"
#include "inBuf.h"
#include "outBuf.h"

//
// casClient
//
// this class exists so that udp and tcp can share certain 
// protocol stubs but is this worth the extra complexity ????
//
class casClient : public casCoreClient, public outBufClient, 
    public inBufClient {
public:
	casClient ( caServerI &, clientBufMemoryManager &, bufSizeT ioMinSizeIn );
	virtual ~casClient ();
	virtual void show ( unsigned level ) const;
	caStatus sendErr ( const caHdrLargeArray *, ca_uint32_t cid,
            const int reportedStatus, const char *pFormat, ... );
	ca_uint16_t protocolRevision() const {return this->minor_version_number;}
	virtual void hostName ( char *pBuf, unsigned bufSize ) const = 0;
    void sendVersion ();

protected:
    inBuf in;
    outBuf out;
	ca_uint16_t minor_version_number;
    unsigned incommingBytesToDrain;
	epicsTime lastSendTS;
	epicsTime lastRecvTS;

	caStatus sendErrWithEpicsStatus ( const caHdrLargeArray *pMsg,
			ca_uint32_t cid, caStatus epicsStatus, caStatus clientStatus );

#	define logBadId(MP, DP, CACSTAT, RESID) \
	this->logBadIdWithFileAndLineno(MP, DP, CACSTAT, __FILE__, __LINE__, RESID)
	caStatus logBadIdWithFileAndLineno ( const caHdrLargeArray *mp,
			const void *dp, const int cacStat, const char *pFileName, 
			const unsigned lineno, const unsigned resId );

	caStatus processMsg();

	//
	// dump message to stderr
	//
	void dumpMsg ( const caHdrLargeArray *mp, const void *dp, 
        const char *pFormat, ... );


private:
    typedef caStatus ( casClient::*pCASMsgHandler ) ();

	//
	// one function for each CA request type
	//
	virtual caStatus uknownMessageAction () = 0;
	caStatus ignoreMsgAction ();
	virtual caStatus versionAction ();
	virtual caStatus eventAddAction ();
	virtual caStatus eventCancelAction ();
	virtual caStatus readAction ();
	virtual caStatus readNotifyAction ();
	virtual caStatus writeAction ();
	virtual caStatus searchAction ();
	virtual caStatus eventsOffAction ();
	virtual caStatus eventsOnAction ();
	virtual caStatus readSyncAction ();
	virtual caStatus clearChannelAction ();
	virtual caStatus claimChannelAction ();
	virtual caStatus writeNotifyAction ();
	virtual caStatus clientNameAction ();
	virtual caStatus hostNameAction ();
	virtual caStatus echoAction ();

	virtual void userName ( char * pBuf, unsigned bufSize ) const = 0;

	//
	// static members
	//
	static void loadProtoJumpTable();
	static pCASMsgHandler msgHandlers[CA_PROTO_LAST_CMMD+1u];
	static bool msgHandlersInit;

	casClient ( const casClient & );
	casClient & operator = ( const casClient & );
};

#endif // casClienth

