/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*  
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
static const double CA_ECHO_TIMEOUT = 5.0; /* (sec) disconn no echo reply tmo */ 
static const double CA_CONN_VERIFY_PERIOD = 30.0; /* (sec) how often to request echo */

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
#define genLocalExcep( CBGUARD, GUARD, CAC, STAT, PCTX ) \
(CAC).exception ( CBGUARD, GUARD, STAT, PCTX, __FILE__, __LINE__ )

#endif // ifdef INCiocinfh 
