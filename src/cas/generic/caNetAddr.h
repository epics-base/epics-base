//
// caNetAddr
//
// special cas specific network address class so:
// 1) we dont drag BSD socket headers into 
//	the server tool
// 2) we are able to use other networking services
//	besides sockets in the future
//
// get() will assert fail if the init flag has not been
//	set
//

#ifndef caNetAddrH
#define caNetAddrH

#ifdef caNetAddrSock
#include "osiSock.h"
#include "bsdSocketResource.h"
#endif

#include "epicsAssert.h"
#include "shareLib.h"

class verifyCANetAddr {
public:
	inline void checkSize(); 
};

class epicsShareClass caNetAddr {
	friend class verifyCANetAddr;
public:
    enum caNetAddrType {casnaUDF, casnaInet};

	// 
	// clear()
	//
	inline void clear ()
	{
		this->type = casnaUDF;
	}

	//
	// caNetAddr()
	//
	inline caNetAddr ()
	{	
		this->clear();
	}

	inline bool isInet () const
	{
		return this->type == casnaInet;
	}

	inline bool isValid () const
	{
		return this->type != casnaUDF;
	}

	inline bool operator == (const caNetAddr &rhs) const
	{
        if (this->type != rhs.type) {
            return false;
        }
#	ifdef caNetAddrSock
        if (this->type==casnaInet) {
            return (this->addr.ip.sin_addr.s_addr == rhs.addr.ip.sin_addr.s_addr) && 
                (this->addr.ip.sin_port == rhs.addr.ip.sin_port);
        }
#   endif
        else {
            return false;
        }
	}

	inline bool operator != (const caNetAddr &rhs) const
    {
        return ! this->operator == (rhs);
    }

	//
	// This is specified so that compilers will not use one of 
	// the following assignment operators after converting to a 
	// sockaddr_in or a sockaddr first.
	//
	// caNetAddr caNetAddr::operator =(const struct sockaddr_in&)
	// caNetAddr caNetAddr::operator =(const struct sockaddr&)
	//
	inline caNetAddr operator = (const caNetAddr &naIn)
	{
		this->addr = naIn.addr;
		this->type = naIn.type;
		return *this;
	}	

    //
    // convert to the string equivalent of the address
    //
    void stringConvert (char *pString, unsigned stringLength) const;

	//
	// conditionally drag BSD socket headers into the server
	// and server tool
	//
	// to use this #define caNetAddrSock
	//
#	ifdef caNetAddrSock
		inline void setSockIP (unsigned long inaIn, unsigned short portIn)
		{
			this->type = casnaInet;
			this->addr.ip.sin_family = AF_INET;
			this->addr.ip.sin_addr.s_addr = inaIn;
			this->addr.ip.sin_port = portIn;
		}	

		inline void setSockIP (const struct sockaddr_in &sockIPIn)
		{
			this->type = casnaInet;
			assert (sockIPIn.sin_family == AF_INET);
			this->addr.ip = sockIPIn;
		}	

		inline void setSock (const struct sockaddr &sock)
		{
			assert (sock.sa_family == AF_INET);
			this->type = casnaInet;
			const struct sockaddr_in *psip = 
				(const struct sockaddr_in *) &sock;
			this->addr.ip = *psip;
		}	

		inline caNetAddr (const struct sockaddr_in &sockIPIn)
		{
			this->setSockIP (sockIPIn);
		}

		inline caNetAddr operator = (const struct sockaddr_in &sockIPIn)
		{
			this->setSockIP (sockIPIn);
			return *this;
		}			

		inline caNetAddr operator = (const struct sockaddr &sockIn)
		{
			this->setSock (sockIn);
			return *this;
		}		

		inline struct sockaddr_in getSockIP() const
		{
			assert (this->type==casnaInet);
			return this->addr.ip;
		}

		inline struct sockaddr getSock() const
		{
			struct sockaddr sa;
			assert (this->type==casnaInet);
			struct sockaddr_in *psain = (struct sockaddr_in *) &sa;
			*psain = this->addr.ip;

			return sa;
		}

		inline operator sockaddr_in () const
		{
			return this->getSockIP();
		}

		inline operator sockaddr () const
		{
			return this->getSock();
		}

	#	endif // caNetAddrSock

private:
	union {	
#		ifdef caNetAddrSock		
			struct sockaddr_in ip;
#		endif
		//
		// this must be as big or bigger 
		// than the largest field
		//
		// see checkDummySize() above
		//
		unsigned char dummy[16];	
	} addr;

	caNetAddrType type;
};

inline void verifyCANetAddr::checkSize()
{
	//
	// temp used because brain dead
	// ms compiler does not allow
	// use of scope res operator
	//
	caNetAddr t;
	size_t ds = sizeof (t.addr.dummy);
	size_t as = sizeof (t.addr);
	assert (ds==as);
}

//
// caNetAddr::stringConvert ()
//
inline void caNetAddr::stringConvert (char *pString, unsigned stringLength) const
{
#   ifdef caNetAddrSock		
    if (this->type==casnaInet) {
        ipAddrToA (&this->addr.ip, pString, stringLength);
        return;
    }
#   endif    
    if (stringLength) {
        strncpy (pString, "<Undefined Address>", stringLength);
        pString[stringLength-1] = '\n';
    }
}

#endif // caNetAddrH
