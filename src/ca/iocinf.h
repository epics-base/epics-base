/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef INCiocinfh  
#define INCiocinfh

#ifdef DEBUG
#   define debugPrintf(argsInParen) ::printf argsInParen
#else
#   define debugPrintf(argsInParen)
#endif

#ifndef NELEMENTS
#   define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
#endif

#define MSEC_PER_SEC    1000L
#define USEC_PER_SEC    1000000L

#if defined ( CLOCKS_PER_SEC )
#   define CAC_SIGNIFICANT_DELAY ( 1.0 / CLOCKS_PER_SEC )
#else
#   define CAC_SIGNIFICANT_DELAY (1.0 / 1000000u)
#endif

/*
 * these two control the period of connection verifies
 * (echo requests) - CA_CONN_VERIFY_PERIOD - and how
 * long we will wait for an echo reply before we
 * give up and flag the connection for disconnect
 * - CA_ECHO_TIMEOUT.
 *
 * CA_CONN_VERIFY_PERIOD is normally obtained from an
 * EPICS environment variable.
 */
const static double CA_ECHO_TIMEOUT = 5.0; /* (sec) disconn no echo reply tmo */ 
const static double CA_CONN_VERIFY_PERIOD = 30.0; /* (sec) how often to request echo */

/*
 * this determines the number of messages received
 * without a delay in between before we go into 
 * monitor flow control
 *
 * turning this down effects maximum throughput
 * because we dont get an optimal number of bytes 
 * per network frame 
 */
static const unsigned contiguousMsgCountWhichTriggersFlowControl = 10u;

class caErrorCode { 
public:
    caErrorCode ( int status ) : code ( status ) {};
private:
    int code;
};

/*
 * CA internal functions
 */
#define genLocalExcep( GUARD, CAC, STAT, PCTX ) \
(CAC).exception ( GUARD, STAT, PCTX, __FILE__, __LINE__ )

#endif // ifdef INCiocinfh 
