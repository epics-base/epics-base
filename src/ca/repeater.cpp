/*
 * $Id$
 *
 * REPEATER.C
 *
 * CA broadcast repeater
 *
 * Author:  Jeff Hill
 * Date:    3-27-90
 *
 *  Control System Software for the GTA Project
 *
 *  Copyright 1988, 1989, the Regents of the University of California.
 *
 *  This software was produced under a U.S. Government contract
 *  (W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *  operated by the University of California for the U.S. Department
 *  of Energy.
 *
 *  Developed by the Controls and Automation Group (AT-8)
 *  Accelerator Technology Division
 *  Los Alamos National Laboratory
 *
 *  Direct inqueries to:
 *  Jeff HIll, AT-8, Mail Stop H820
 *  Los Alamos National Laboratory
 *  Los Alamos, New Mexico 87545
 *  Phone: (505) 665-1831
 *  E-mail: johill@lanl.gov
 *
 *  PURPOSE:
 *  Broadcasts fan out over the LAN, but old IP kernels do not allow
 *  two processes on the same machine to get the same broadcast
 *  (and modern IP kernels do not allow two processes on the same machine
 *  to receive the same unicast).
 *
 *  This code fans out UDP messages sent to the CA repeater port
 *  to all CA client processes that have subscribed.
 *
 */

/*
 * It would be preferable to avoid using the repeater on multicast enhanced IP kernels, but
 * this is not going to work in all situations because (according to Steven's TCP/IP
 * illustrated volume I) if a broadcast is received it goes to all sockets on the same port,
 * but if a unicast is received it goes to only one of the sockets on the same port
 * (we can only guess at which one it will be).
 *   
 * I have observed this behavior under winsock II:
 * o only one of the sockets on the same port receives the message if we send to the 
 * loop back address
 * o both of the sockets on the same port receives the message if we send to the 
 * broadcast address
 * 
 */

#include    "iocinf.h"
#include    "taskwd.h"

#ifdef DEBUG
#   define debugPrintf(argsInParen) printf argsInParen
#else
#   define debugPrintf(argsInParen)
#endif

/*
 * one socket per client so we will get the ECONNREFUSED
 * error code (and then delete the client)
 */
class repeaterClient : public tsDLNode < repeaterClient > {
public:
    repeaterClient ( const osiSockAddr &from );
    bool connect ();
    bool sendConfirm ();
    bool sendMessage ( const void *pBuf, unsigned bufSize );
    void destroy ();
    bool verify ();
    bool identicalAddress ( const osiSockAddr &from );
    bool identicalPort ( const osiSockAddr &from );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
private:
    osiSockAddr from;
    SOCKET sock;
    static tsFreeList < class repeaterClient, 0x20 > freeList;
    ~repeaterClient ();
    unsigned port () const;
};

/* 
 *  these can be external since there is only one instance
 *  per machine so we dont care about reentrancy
 */
static tsDLList < repeaterClient > client_list;
tsFreeList < repeaterClient, 0x20 > repeaterClient::freeList;

static char buf [MAX_UDP_RECV]; 

static const unsigned short PORT_ANY = 0u;

typedef struct {
    SOCKET sock;
    int errNumber;
    const char *pErrStr;
} makeSocketReturn;

/*
 * makeSocket()
 */
LOCAL makeSocketReturn makeSocket ( unsigned short port, bool reuseAddr )
{
    int status;
    struct sockaddr_in bd;
    makeSocketReturn msr;
    int flag;

    msr.sock = socket ( AF_INET, SOCK_DGRAM, 0 );     
    if ( msr.sock == INVALID_SOCKET ) {
        msr.errNumber = SOCKERRNO;
        msr.pErrStr = SOCKERRSTR (msr.errNumber);
        return msr;
    }

    /*
     * no need to bind if unconstrained
     */
    if (port != PORT_ANY) {

        memset ( (char *) &bd, 0, sizeof (bd) );
        bd.sin_family = AF_INET;
        bd.sin_addr.s_addr = htonl (INADDR_ANY); 
        bd.sin_port = htons (port);  
        status = bind (msr.sock, (struct sockaddr *)&bd, (int)sizeof(bd));
        if ( status < 0 ) {
            msr.errNumber = SOCKERRNO;
            msr.pErrStr = SOCKERRSTR ( msr.errNumber );
            socket_close (msr.sock);
            msr.sock = INVALID_SOCKET;
            return msr;
        }
        if (reuseAddr) {
            flag = true;
            status = setsockopt ( msr.sock,  SOL_SOCKET, SO_REUSEADDR,
                        (char *) &flag, sizeof (flag) );
            if ( status < 0 ) {
                int errnoCpy = SOCKERRNO;
                ca_printf (
            "%s: set socket option failed because \"%s\"\n", 
                        __FILE__, SOCKERRSTR(errnoCpy));
            }
        }
    }

    msr.errNumber = 0;
    msr.pErrStr = "no error";
    return msr;
}

repeaterClient::repeaterClient ( const osiSockAddr &fromIn ) :
    from ( fromIn ), sock ( INVALID_SOCKET )
{
    debugPrintf ( ( "new client %u\n", ntohs ( from.ia.sin_port ) ) );
}

bool repeaterClient::connect ()
{
    int status;
    makeSocketReturn msr;

    msr = makeSocket ( PORT_ANY, false );
    if ( msr.sock == INVALID_SOCKET ) {
        ca_printf ( "%s: no client sock because %d=\"%s\"\n",
                __FILE__, msr.errNumber, msr.pErrStr );
        return false;
    }

    this->sock = msr.sock;

    status = ::connect ( this->sock, &this->from.sa, sizeof ( this->from.sa ) );
    if ( status < 0 ) {
        int errnoCpy = SOCKERRNO;

        ca_printf (
            "%s: unable to connect client sock because \"%s\"\n",
            __FILE__, SOCKERRSTR ( errnoCpy ) );
        return false;
    }

    return true;
}

bool repeaterClient::sendConfirm ()
{
    int status;

    caHdr confirm;
    memset ( (char *) &confirm, '\0', sizeof (confirm) );
    confirm.m_cmmd = htons ( REPEATER_CONFIRM );
    confirm.m_available = this->from.ia.sin_addr.s_addr;
    status = send ( this->sock, (char *) &confirm,
                    sizeof (confirm), 0 );
    if ( status >= 0 ) {
        assert ( status == sizeof ( confirm ) );
        return true;
    }
    else if ( SOCKERRNO == SOCK_ECONNREFUSED ) {
        return false;
    }
    else {
        ca_printf ( "CA Repeater: confirm err was \"%s\"\n",
                SOCKERRSTR (SOCKERRNO) );
        return false;
    }
}

bool repeaterClient::sendMessage ( const void *pBuf, unsigned bufSize )
{
    int status;

    status = send ( this->sock, (char *) pBuf, bufSize, 0 );
    if ( status >= 0 ) {
        assert ( static_cast <unsigned> ( status ) == bufSize );
        debugPrintf ( ("Sent to %u\n", ntohs ( this->from.ia.sin_port ) ) );
        return true;
    }
    else {
        int errnoCpy = SOCKERRNO;
        if ( errnoCpy == SOCK_ECONNREFUSED ) {
            debugPrintf ( ("Client refused message %u\n", ntohs ( this->from.ia.sin_port ) ) );
        }
        else {
            debugPrintf ( ( "CA Repeater: UDP send err was \"%s\"\n", SOCKERRSTR (errnoCpy) ) );
        }
        return false;
    }
}

repeaterClient::~repeaterClient ()
{
    if ( this->sock != INVALID_SOCKET ) {
        socket_close ( this->sock );
    }
    debugPrintf ( ( "Deleted client %u\n", ntohs ( this->from.ia.sin_port ) ) );
}

inline void * repeaterClient::operator new ( size_t size )
{ 
    return repeaterClient::freeList.allocate ( size );
}

inline void repeaterClient::operator delete ( void *pCadaver, size_t size )
{ 
    repeaterClient::freeList.release ( pCadaver, size );
}

inline void repeaterClient::destroy ()
{
    delete this;
}

inline unsigned repeaterClient::port () const
{
    return ntohs ( this->from.ia.sin_port );
}

inline bool repeaterClient::identicalAddress ( const osiSockAddr &fromIn )
{
    if ( fromIn.sa.sa_family == this->from.sa.sa_family ) {
        if ( fromIn.ia.sin_port == this->from.ia.sin_port) {
            if ( fromIn.ia.sin_addr.s_addr == this->from.ia.sin_addr.s_addr ) {
                return true;
            }
        }
    }
    return false;
}

inline bool repeaterClient::identicalPort ( const osiSockAddr &fromIn )
{
    if ( fromIn.sa.sa_family == this->from.sa.sa_family ) {
        if ( fromIn.ia.sin_port == this->from.ia.sin_port) {
            return true;
        }
    }
    return false;
}

bool repeaterClient::verify ()
{
    makeSocketReturn msr;
    msr = makeSocket ( this->port (), false );
    if ( msr.sock != INVALID_SOCKET ) {
        socket_close ( msr.sock );
        return false;
    }
    else {
        /*
         * win sock does not set SOCKERRNO when this fails
         */
        if ( msr.errNumber != SOCK_EADDRINUSE ) {
            ca_printf (
"CA Repeater: bind test err was %d=\"%s\"\n", 
                msr.errNumber, msr.pErrStr );
        }
        return true;
    }
}


/*
 * verifyClients()
 * (this is required because solaris has a half baked version of sockets)
 */
LOCAL void verifyClients()
{
    static tsDLList < repeaterClient > theClients;
    repeaterClient *pclient;

    while ( ( pclient = client_list.get () ) ) {
        if ( pclient->verify () ) {
            theClients.add ( *pclient );
        }
        else {
            pclient->destroy ();
        }
    }
    client_list.add ( theClients );
}

/*
 * fanOut()
 */
LOCAL void fanOut ( const osiSockAddr &from, const void *pMsg, unsigned msgSize )
{
    static tsDLList < repeaterClient > theClients;
    repeaterClient *pclient;

    while ( ( pclient = client_list.get () ) ) {
        theClients.add ( *pclient );
        /* Dont reflect back to sender */
        if ( pclient->identicalAddress ( from ) ) {
            continue;
        }

        if ( ! pclient->sendMessage ( pMsg, msgSize ) ) {
            if ( ! pclient->verify () ) {
                theClients.remove ( *pclient );
                pclient->destroy ();
            }
        }
    }

    client_list.add ( theClients );
}

/*
 * register_new_client()
 */
LOCAL void register_new_client ( osiSockAddr &from )
{
    int status;
    bool newClient = false;
    makeSocketReturn msr;

    if ( from.sa.sa_family != AF_INET ) {
        return;
    }

    /*
     * the repeater and its clients must be on the same host
     */
    if ( htonl ( INADDR_LOOPBACK ) != from.ia.sin_addr.s_addr ) {
        static SOCKET testSock = INVALID_SOCKET;
        static bool init = false;

        if ( ! init ) {
            msr = makeSocket ( PORT_ANY, true );
            if ( msr.sock == INVALID_SOCKET ) {
                ca_printf ( "%s: Unable to create repeater bind test socket because %d=\"%s\"\n",
                    __FILE__, msr.errNumber, msr.pErrStr );
            }
            else {
                testSock = msr.sock;
            }
            init = true;
        }

        /*
         * Unfortunately on 3.13 beta 11 and before the
         * repeater would not always allow the loopback address
         * as a local client address so current clients alternate
         * between the address of the first non-loopback interface
         * found and the loopback addresss when subscribing with 
         * the CA repeater until all CA repeaters have been updated
         * to current code.
         */
        if ( testSock != INVALID_SOCKET ) {
            osiSockAddr addr;

            addr = from;
            addr.ia.sin_port = PORT_ANY;

            /* we can only bind to a local address */
            status = bind ( testSock, &addr.sa, sizeof ( addr ) );
            if ( status ) {
                return;
            }
        }
        else {
            return;
        }
    }

    tsDLIterBD < repeaterClient > pclient = client_list.first ();
    while ( pclient.valid () ) {
        if ( pclient->identicalPort ( from ) ) {
            break;
        }
        pclient = pclient.itemAfter ();
    }       

    if ( ! pclient.valid () ) {
        pclient = new repeaterClient ( from );
        if ( ! pclient.valid () ) {
            ca_printf ( "%s: no memory for new client\n", __FILE__ );
            return;
        }

        if ( ! pclient->connect () ) {
            pclient->destroy ();
            return;
        }

        client_list.add ( *pclient ); 
        newClient = true;
    }

    if ( ! pclient->sendConfirm () ) {
        client_list.remove (*pclient );
        pclient->destroy ();
        debugPrintf ( ( "Deleted repeater client=%u (error while sending ack)\n",
                    ntohs (from.ia.sin_port) ) );
    }

    /*
     * send a noop message to all other clients so that we dont 
     * accumulate sockets when there are no beacons
     */
    caHdr noop;
    memset ( (char *) &noop, '\0', sizeof ( noop ) );
    noop.m_cmmd = htons ( CA_PROTO_NOOP );
    fanOut ( from, &noop, sizeof ( noop ) );

    if ( newClient ) {
        /*
         * on solaris we need to verify that the clients
         * have not gone away (because ICMP does not
         * get through to send()
         *
         * this is done each time that a new client is 
         * created
         *
         * this is done here in order to avoid deleting
         * a client prior to sending its confirm message
         */
        verifyClients ();
    }
}


/*
 *  ca_repeater()
 */
void epicsShareAPI ca_repeater ()
{
    int size;
    SOCKET sock;
    osiSockAddr from;
    unsigned short port;
    makeSocketReturn msr;

    assert ( osiSockAttach() );

    port = envGetInetPortConfigParam ( &EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT );

    msr = makeSocket ( port, true );
    if ( msr.sock == INVALID_SOCKET ) {
        /*
         * test for server was already started
         */
        if ( msr.errNumber == SOCK_EADDRINUSE ) {
            osiSockRelease ();
            debugPrintf ( ( "CA Repeater: exiting because a repeater is already running\n" ) );
            exit (0);
        }
        ca_printf("%s: Unable to create repeater socket because %d=\"%s\" - fatal\n",
            __FILE__, msr.errNumber, msr.pErrStr);
        osiSockRelease ();
        exit(0);
    }

    sock = msr.sock;

   debugPrintf ( ( "CA Repeater: Attached and initialized\n" ) );

   while ( true ) {
        osiSocklen_t from_size = sizeof ( from );
        size = recvfrom ( sock, buf, sizeof (buf), 0,
                    &from.sa, &from_size );
        if ( size < 0 ) {
            int errnoCpy = SOCKERRNO;
#           ifdef linux
                /*
                 * Avoid spurious ECONNREFUSED bug
                 * in linux
                 */
                if ( errnoCpy == SOCK_ECONNREFUSED ) {
                    continue;
                }
#           endif
            ca_printf ("CA Repeater: unexpected UDP recv err: %s\n",
                SOCKERRSTR(errnoCpy));
            continue;
        }

        caHdr *pMsg = (caHdr *) buf;

        /*
         * both zero length message and a registration message
         * will register a new client
         */
        if ( ( (size_t) size) >= sizeof (*pMsg) ) {
            if ( ntohs ( pMsg->m_cmmd ) == REPEATER_REGISTER ) {
                register_new_client ( from );

                /*
                 * strip register client message
                 */
                pMsg++;
                size -= sizeof ( *pMsg );
                if ( size==0 ) {
                    continue;
                }
            }
        }
        else if ( size == 0 ) {
            register_new_client ( from );
            continue;
        }

        fanOut ( from, pMsg, size );
    }
}

/*
 * caRepeaterThread ()
 */
void caRepeaterThread ( void * /* pDummy */ )
{
    taskwdInsert ( threadGetIdSelf(), NULL, NULL );
    ca_repeater ();
}


