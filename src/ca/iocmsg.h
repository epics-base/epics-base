#ifndef __IOCMSG__
/*
	History
	1/90	Jeff Hill	removed status field in favor of a
				seperate command- saves space on every
				successful operation
        09/24/90 Marty Kraimer  Temporily changed def for SERVER_NUM
				
*/

#define __IOCMSG__

/* TCP/UDP port number (bumped each protocol change) */
#define CA_PROTOCOL_VERSION	2
/*
#define SERVER_NUM		(IPPORT_USERRESERVED+12+CA_PROTOCOL_VERSION)
*/
#define SERVER_NUM		(1102+CA_PROTOCOL_VERSION)

#define RSERVERQ		1	   /* qid for response server queue */
#define MAX_UDP			1024
#define MAX_TCP			(MAX_UDP*16) /* so waveforms fit */
#define MAX_MSG_SIZE		(MAX_TCP) /* the larger of tcp and udp max */


		/* values for m_cmmd */

#define IOC_EVENT_ADD		2	/* add an event */
#define IOC_NOCMD		4	/* NOOP */
#define IOC_EVENT_CANCEL	5	/* cancel an event */
#define IOC_READ		7	/* read and return a channel value*/
#define IOC_WRITE		9	/* write a channel value */
#define IOC_SNAPSHOT		11	/* snapshot of the system */
#define	IOC_SEARCH		31	/* IOC channel search */
#define	IOC_BUILD		33	/* Conglomerate of IOC_SEARCH */
					/* IOC_READ */
					/* (optimizes OPI network use)	*/
#define IOC_EVENTS_OFF		13	/* flow control */ 
#define IOC_EVENTS_ON		14	/* flow control */ 
#define IOC_READ_SYNC		16	/* purge old reads */ 
#define IOC_ERROR		18	/* an operation failed */ 



	
	/* extmsg - the nonvariant part of each message sent/recv
		by the request server.
	*/


struct	extmsg {
	unsigned short	m_cmmd;		/* operation to be performed */
	unsigned short	m_postsize;	/* size of message extension */	
	unsigned short	m_type;		/* operation data type */ 
	unsigned short 	m_count;	/* operation data count */
	void		*m_paddr;	/* ptr to struct db_addr */
					/* IOC db field addr info */
	unsigned long	m_available;	/* undefined message location for use
					 * by client processes */
};


	/* for  monitor message extension */
struct  mon_info{
	float		m_lval;		/* low value to look for (deviation low
					 * for continuous monitors) */
	float		m_hval;		/* high value (high deviation 
					 * for continuous monitors) */
	float		m_toval;	/* timeout limit for returning cval */
};

struct	monops {			/* monitor req opi to ioc */
	struct extmsg	m_header;
	struct mon_info	m_info;
};

#endif
