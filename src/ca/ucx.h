/* 
 *
 *	U C X . H
 *	UNIX ioctl structures and defines used for VAX/UCX
 *      Author: Gerhard Grygiel (GeG)
 *
 *      GeG	09-DEC-1992	initial edit
 *      CJM     13-Jul-1994     add fd_set etc for R3.12
 *      CJM     09-Dec-1994     define fd_set etc. so it will compile for
 *                              both DEC C and Vax C
 *      CJM     19-Nov-1995     use memset instead of bzero following advice
 *                              from Jeff Hill and add a definition of struct
 *                              timezone to support gettimeofday
 *
 */
#ifndef _UCX_H_
#define _UCX_H_ 
#ifdef UCX

#define	IFF_UP		0x1		/* interface is up */
#define	IFF_BROADCAST	0x2		/* broadcast address valid */
#define	IFF_LOOPBACK	0x8		/* is a loopback net */
#define IFF_POINTOPOINT 0x10            /* interface is point to point */
/*
 * Interface request structure used for socket
 * ioctl's.  All interface ioctl's must have parameter
 * definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct	ifreq {
#define	IFNAMSIZ	16
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		short	ifru_flags;
		int	ifru_metric;
		caddr_t	ifru_data;
	} ifr_ifru;
#define	ifr_addr	ifr_ifru.ifru_addr	/* address */
#define	ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-to-p link */
#define	ifr_broadaddr	ifr_ifru.ifru_broadaddr	/* broadcast address */
#define	ifr_flags	ifr_ifru.ifru_flags	/* flags */
#define ifr_metric	ifr_ifru.ifru_metric	/* metric */
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface */
};

/* Structure used in SIOCGIFCONF request.
 * Used to retrieve interface configuration
 * for machine (useful for programs which
 * must know all networks accessible).
 */
struct	ifconf {
	int	ifc_len;		/* size of associated buffer */
	union {
		caddr_t	ifcu_buf;
		struct	ifreq *ifcu_req;
	} ifc_ifcu;
#define	ifc_buf	ifc_ifcu.ifcu_buf	/* buffer address */
#define	ifc_req	ifc_ifcu.ifcu_req	/* array of structures returned */
};

#ifndef NBBY
#define NBBY 8
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE    256
#endif

typedef long fd_mask ;
#define NFDBITS (sizeof (fd_mask) * NBBY )       /* bits per mask */
#ifndef howmany
#define howmany(x, y)  (((x)+((y)-1))/(y))
#endif

/*
 * Both DEC C and VAX C only allow 32 fd's at once
 */
typedef int fd_set ;

#define FD_SET(n, p)    (*(p) |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p)    (*(p) &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)  (*(p) & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)      memset((char *)(p), 0, sizeof (*(p)))

#include <iodef.h>
#define IO$_RECEIVE	(IO$_WRITEVBLK)

struct timezone {
        int      tz_minuteswest ;  /* minutes west of Greenwich */
        int      tz_dsttime ;      /* type of dst correction */
};

#define TWOPOWER32 4294967296.0
#define TWOPOWER31 2147483648.0
#define UNIX_EPOCH_AS_MJD 40587.0
#endif
#endif

