//
// $Id$
//
// casIOD.h - Channel Access Server IO Dependent for BSD sockets 
// 
//
// Some BSD calls have crept in here
//
// $Log$
// Revision 1.1.1.1  1996/06/20 00:28:18  jhill
// ca server installation
//
//

#ifndef includeCASIODH
#define includeCASIODH 


#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifndef SUNOS4 // func proto do not have args for C++ (under sunos4)
#include <arpa/inet.h>
#include <netdb.h>
#endif

#ifdef __cplusplus
}
#endif

#include <bsdProto.h>

typedef int                     SOCKET;
#define SOCKERRNO               errno
#define socket_close(S)         close(S)
#define socket_ioctl(A,B,C)     ioctl(A,B,C)

// ca
#include <addrList.h>

void hostNameFromIPAddr (const caAddr *pAddr, 
			char *pBuf, unsigned bufSize);
#if 0
//
// assuming al posix machines are IEEE float will be wrong
//
#define ntohf(A) (A)
#define ntohd(A) (A)
#define htonf(A) (A)
#define htond(A) (A)
#endif

class caServerIO {
public:
	caServerIO() : sockState(casOffLine), sock(-1) {}
	~caServerIO() 
	{
		if (this->sock>=0 && this->sockState==casOnLine) {
			socket_close(this->sock);
		}
	}
        caStatus init(); //constructor does not return status
 
        //
        // show status of IO subsystem
        //
        void show (unsigned level);
 
        int getFD() const;
 
        void setNonBlocking();
 
        unsigned serverPortNumber() const;
 
       	// 
        // called when we expect that a virtual circuit for a
        // client can be created
       	// 
       	casMsgIO *newStreamIO() const;
 
        //
        // called to create datagram IO
        //
        casMsgIO *newDGIO() const;

private:
        casIOState              sockState;
        SOCKET                  sock;
        caAddr                  addr;
        //
        // static member data
        //
        static int              staticInitialized;
        //
        // static member func
        //
        static inline void staticInit();
};

class casStreamIO : public casMsgIO {
public:
	casStreamIO(const SOCKET s, const caAddr &a);
	caStatus init();
	~casStreamIO();

	int getFileDescriptor() const;
	void setNonBlocking();
	bufSizeT optimumBufferSize ();

	casIOState state() const;
	void hostNameFromAddr (char *pBuf, unsigned bufSize);
	
	xSendStatus osdSend (const char *pBuf,
		bufSizeT nBytesReq, bufSizeT &nBytesActual);
	xRecvStatus osdRecv (char *pBuf,
		bufSizeT nBytesReq, bufSizeT &nBytesActual);
	void osdShow (unsigned level) const;

	bufSizeT incommingBytesPresent() const;
private:
        casIOState      sockState;
        SOCKET          sock;
        caAddr          addr;
};

class casDGIO : public casMsgIO {
public:
	casDGIO();
	caStatus init();
	~casDGIO();

	int getFileDescriptor() const;
	void setNonBlocking();
	bufSizeT optimumBufferSize ();
	void sendBeacon(char &msg, bufSizeT length,
			aitUint32 &m_avail);
	casIOState state() const;
	void hostNameFromAddr (char *pBuf, unsigned bufSize);
	
	xSendStatus osdSend (const char *pBuf,
		bufSizeT nBytesReq, bufSizeT &nBytesActual);
	xRecvStatus osdRecv (char *pBuf,
		bufSizeT nBytesReq, bufSizeT &nBytesActual);
	void osdShow (unsigned level) const;
private:
        ELLLIST         destAddrList;
        caAddr          lastRecvAddr;
        SOCKET          sock;
        casIOState      sockState;
        char            lastRecvAddrInit;
};

// no additions below this line
#endif // includeCASIODH

