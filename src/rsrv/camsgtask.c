/*
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   6-88
 *
 *  Experimental Physics and Industrial Control System (EPICS)
 *
 *  Copyright 1991, the Regents of the University of California,
 *  and the University of Chicago Board of Governors.
 *
 *  This software was produced under  U.S. Government contracts:
 *  (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *  and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *  Initial development by:
 *      The Controls and Automation Group (AT-8)
 *      Ground Test Accelerator
 *      Accelerator Technology Division
 *      Los Alamos National Laboratory
 *
 *  Co-developed with
 *      The Controls and Computing Group
 *      Accelerator Systems Division
 *      Advanced Photon Source
 *      Argonne National Laboratory
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dbDefs.h"
#include "osiSock.h"
#include "epicsTime.h"
#include "errlog.h"
#include "taskwd.h"
#include "db_access.h"

#include "server.h"

/*
 *  camsgtask()
 *
 *  CA server TCP client task (one spawned for each client)
 */
void camsgtask ( struct client *client )
{
    int nchars;
    int status;

    casAttachThreadToClient ( client );

    while ( TRUE ) {
        client->recv.stk = 0;
            
        assert ( client->recv.maxstk >= client->recv.cnt );
        nchars = recv ( client->sock, &client->recv.buf[client->recv.cnt], 
                (int) ( client->recv.maxstk - client->recv.cnt ), 0 );
        if ( nchars == 0 ){
            if ( CASDEBUG > 0 ) {
                errlogPrintf ( "CAS: nill message disconnect ( %u bytes request )\n",
                    sizeof ( client->recv.buf ) - client->recv.cnt );
            }
            break;
        }
        else if (nchars<0) {
            int anerrno = SOCKERRNO;

            /*
             * normal conn lost conditions
             */
            if (    ( anerrno != SOCK_ECONNABORTED &&
                    anerrno != SOCK_ECONNRESET &&
                    anerrno != SOCK_ETIMEDOUT ) ||
                    CASDEBUG > 2 ) {
                    errlogPrintf ( "CAS: client disconnect(errno=%d)\n", anerrno );
            }
            break;
        }

        epicsTimeGetCurrent ( &client->time_at_last_recv );
        client->recv.cnt += ( unsigned ) nchars;

        status = camessage ( client );
        if (status == 0) {
            /*
             * if there is a partial message
             * align it with the start of the buffer
             */
            if (client->recv.cnt > client->recv.stk) {
                unsigned bytes_left;

                bytes_left = client->recv.cnt - client->recv.stk;

                /*
                 * overlapping regions handled
                 * properly by memmove 
                 */
                memmove (client->recv.buf, 
                    &client->recv.buf[client->recv.stk], bytes_left);
                client->recv.cnt = bytes_left;
            }
            else {
                client->recv.cnt = 0ul;
            }
        }
        else {
            char buf[64];
            
            client->recv.cnt = 0ul;
            
            /*
             * disconnect when there are severe message errors
             */
            ipAddrToDottedIP (&client->addr, buf, sizeof(buf));
            epicsPrintf ("CAS: forcing disconnect from %s\n", buf);
                break;
        }
        
        /*
         * allow message to batch up if more are comming
         */
        status = socket_ioctl (client->sock, FIONREAD, &nchars);
        if (status < 0) {
            errlogPrintf("CAS: io ctl err %d\n",
                SOCKERRNO);
            cas_send_msg(client, TRUE);
        }
        else if (nchars == 0){
            cas_send_msg(client, TRUE);
        }
    }

    LOCK_CLIENTQ;
    ellDelete ( &clientQ, &client->node );
    UNLOCK_CLIENTQ;

    destroy_tcp_client ( client );
}
