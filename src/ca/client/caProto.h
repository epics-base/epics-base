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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef __CAPROTO__
#define __CAPROTO__ 

#define capStrOf(A) #A
#define capStrOfX(A) capStrOf ( A )

/* 
 * CA protocol revision
 * TCP/UDP port number (bumped each major protocol change) 
 */
#define CA_MAJOR_PROTOCOL_REVISION 4
#define CA_VERSION_STRING( MINOR_REVISION ) \
( capStrOfX ( CA_MAJOR_PROTOCOL_REVISION ) "." capStrOfX ( MINOR_REVISION ) )
#define CA_UKN_MINOR_VERSION    0u /* unknown minor version */
#if CA_MAJOR_PROTOCOL_REVISION == 4u
#   define CA_V41(MINOR) ((MINOR)>=1u) 
#   define CA_V42(MINOR) ((MINOR)>=2u)
#   define CA_V43(MINOR) ((MINOR)>=3u)
#   define CA_V44(MINOR) ((MINOR)>=4u)
#   define CA_V45(MINOR) ((MINOR)>=5u)
#   define CA_V46(MINOR) ((MINOR)>=6u)
#   define CA_V47(MINOR) ((MINOR)>=7u)
#   define CA_V48(MINOR) ((MINOR)>=8u)
#   define CA_V49(MINOR) ((MINOR)>=9u)    /* large arrays, dispatch priorities */
#   define CA_V410(MINOR) ((MINOR)>=10u)  /* beacon counter */
#   define CA_V411(MINOR) ((MINOR)>=11u)  /* sequence numbers in UDP version command */
#   define CA_V412(MINOR) ((MINOR)>=12u)  /* TCP-based search requests */
#   define CA_V413(MINOR) ((MINOR)>=13u)  /* Allow zero length in requests. */
#elif CA_MAJOR_PROTOCOL_REVISION > 4u
#   define CA_V41(MINOR) ( 1u )
#   define CA_V42(MINOR) ( 1u )
#   define CA_V43(MINOR) ( 1u )
#   define CA_V44(MINOR) ( 1u )
#   define CA_V45(MINOR) ( 1u )
#   define CA_V46(MINOR) ( 1u )
#   define CA_V47(MINOR) ( 1u )
#   define CA_V48(MINOR) ( 1u )
#   define CA_V49(MINOR) ( 1u )
#   define CA_V410(MINOR) ( 1u )
#   define CA_V411(MINOR) ( 1u )
#   define CA_V412(MINOR) ( 1u )
#   define CA_V413(MINOR) ( 1u )
#else
#   define CA_V41(MINOR) ( 0u )
#   define CA_V42(MINOR) ( 0u )
#   define CA_V43(MINOR) ( 0u )
#   define CA_V44(MINOR) ( 0u )
#   define CA_V45(MINOR) ( 0u )
#   define CA_V46(MINOR) ( 0u )
#   define CA_V47(MINOR) ( 0u )
#   define CA_V48(MINOR) ( 0u )
#   define CA_V49(MINOR) ( 0u )
#   define CA_V410(MINOR) ( 0u )
#   define CA_V411(MINOR) ( 0u )
#   define CA_V412(MINOR) ( 0u )
#   define CA_V413(MINOR) ( 0u )
#endif

/*
 * These port numbers are only used if the CA repeater and 
 * CA server port numbers cant be obtained from the EPICS 
 * environment variables "EPICS_CA_REPEATER_PORT" and
 * "EPICS_CA_SERVER_PORT"
 */
#define CA_PORT_BASE            IPPORT_USERRESERVED + 56U
#define CA_SERVER_PORT          (CA_PORT_BASE+CA_MAJOR_PROTOCOL_REVISION*2u)
#define CA_REPEATER_PORT        (CA_PORT_BASE+CA_MAJOR_PROTOCOL_REVISION*2u+1u)

/* 
 * 1500 (max of ethernet and 802.{2,3} MTU) - 20(IP) - 8(UDP) 
 * (the MTU of Ethernet is currently independent of its speed varient)
 */
#define ETHERNET_MAX_UDP        ( 1500u - 20u - 8u )
#define MAX_UDP_RECV            ( 0xffff + 16u ) /* allow large frames to be received in the future */
#define MAX_UDP_SEND            1024u /* original MAX_UDP */
#define MAX_TCP                 ( 1024 * 16u ) /* so waveforms fit */
#define MAX_MSG_SIZE            ( MAX_TCP ) /* the larger of tcp and udp max */

#define CA_PROTO_PRIORITY_MIN 0u
#define CA_PROTO_PRIORITY_MAX 99u

/*
 * architecture independent types
 *
 * (so far this works on all archs we have ported to)
 */
typedef unsigned char   ca_uint8_t;
typedef unsigned short  ca_uint16_t;
typedef unsigned int    ca_uint32_t;
typedef float           ca_float32_t;
typedef ca_uint32_t     caResId;

#define ca_uint32_max 0xffffffff

        /* values for m_cmmd */
#define CA_PROTO_VERSION        0u  /* set minor version and priority (used to be NOOP cmd) */
#define CA_PROTO_EVENT_ADD      1u  /* add an event */
#define CA_PROTO_EVENT_CANCEL   2u  /* cancel an event */
#define CA_PROTO_READ           3u  /* read and return a channel value*/
#define CA_PROTO_WRITE          4u  /* write a channel value */
#define CA_PROTO_SNAPSHOT       5u  /* snapshot of the system */
#define CA_PROTO_SEARCH         6u  /* IOC channel search */
#define CA_PROTO_BUILD          7u  /* build - obsolete */
#define CA_PROTO_EVENTS_OFF     8u  /* flow control */ 
#define CA_PROTO_EVENTS_ON      9u  /* flow control */ 
#define CA_PROTO_READ_SYNC      10u /* purge old reads */ 
#define CA_PROTO_ERROR          11u /* an operation failed */ 
#define CA_PROTO_CLEAR_CHANNEL  12u /* free chan resources */
#define CA_PROTO_RSRV_IS_UP     13u /* CA server has joined the net */
#define CA_PROTO_NOT_FOUND      14u /* channel not found */
#define CA_PROTO_READ_NOTIFY    15u /* add a one shot event */
#define CA_PROTO_READ_BUILD     16u /* read and build - obsolete */
#define REPEATER_CONFIRM        17u /* registration confirmation */
#define CA_PROTO_CREATE_CHAN    18u /* client creates channel in server */
#define CA_PROTO_WRITE_NOTIFY   19u /* notify after write chan value */
#define CA_PROTO_CLIENT_NAME    20u /* CA V4.1 identify client */
#define CA_PROTO_HOST_NAME      21u /* CA V4.1 identify client */
#define CA_PROTO_ACCESS_RIGHTS  22u /* CA V4.2 asynch access rights chg */
#define CA_PROTO_ECHO           23u /* CA V4.3 connection verify */
#define REPEATER_REGISTER       24u /* register for repeater fan out */
#define CA_PROTO_SIGNAL         25u /* knock the server out of select */
#define CA_PROTO_CREATE_CH_FAIL 26u /* unable to create chan resource in server */
#define CA_PROTO_SERVER_DISCONN 27u /* server deletes PV (or channel) */

#define CA_PROTO_LAST_CMMD CA_PROTO_SERVER_DISCONN

/*
 * for use with search and not_found (if search fails and
 * its not a broadcast tell the client to look elesewhere)
 */
#define DOREPLY     10u
#define DONTREPLY   5u

/*
 * for use with the m_dataType field in UDP messages emitted by servers
 */
#define sequenceNoIsValid 1

/* size of object in bytes rounded up to nearest oct word */
#define OCT_ROUND(A)    (((A)+7)/8)
#define OCT_SIZEOF(A)   (OCT_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest long word */
#define QUAD_ROUND(A)   ((A)+3)/4)
#define QUAD_SIZEOF(A)  (QUAD_ROUND(sizeof(A)))

/* size of object in bytes rounded up to nearest short word */
#define BI_ROUND(A)     (((A)+1)/2)
#define BI_SIZEOF(A)    (BI_ROUND(sizeof(A)))

/*
 * For communicating access rights to the clients
 *
 * (placed in m_available hdr field of CA_PROTO_ACCESS_RIGHTS cmmd
 */
#define CA_PROTO_ACCESS_RIGHT_READ  (1u<<0u)
#define CA_PROTO_ACCESS_RIGHT_WRITE (1u<<1u)

/*
 * All structures passed in the protocol must have individual
 * fields aligned on natural boundaries.
 *
 * NOTE: all structures declared in this file must have a
 * byte count which is evenly divisible by 8 matching
 * the largest atomic data type in db_access.h.
 */
#define CA_MESSAGE_ALIGN(A) (OCT_ROUND(A)<<3u)

/*
 * the common part of each message sent/recv by the
 * CA server.
 */
typedef struct ca_hdr {
    ca_uint16_t m_cmmd;         /* operation to be performed */
    ca_uint16_t m_postsize;     /* size of payload */
    ca_uint16_t m_dataType;     /* operation data type */
    ca_uint16_t m_count;        /* operation data count */
    ca_uint32_t m_cid;          /* channel identifier */
    ca_uint32_t m_available;    /* protocol stub dependent */
} caHdr;

/*
 * for  monitor (event) message extension
 */
struct  mon_info {
    ca_float32_t    m_lval;     /* low delta */
    ca_float32_t    m_hval;     /* high delta */ 
    ca_float32_t    m_toval;    /* period btween samples */
    ca_uint16_t     m_mask;     /* event select mask */
    ca_uint16_t     m_pad;      /* extend to 32 bits */
};

/*
 * PV names greater than this length assumed to be invalid
 */
#define unreasonablePVNameSize 500u

#endif /* __CAPROTO__ */

