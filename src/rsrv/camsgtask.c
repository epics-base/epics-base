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

#include "osiSock.h"
#include "tsStamp.h"
#include "osiThread.h"
#include "errlog.h"
#include "ellLib.h"
#include "taskwd.h"
#include "db_access.h"
#include "serverInclude.h"
#define epicsExportSharedSymbols
#include "server.h"

/*
 *  camsgtask()
 *
 *  CA server TCP client task (one spawned for each client)
 */
void camsgtask (struct client *client)
{
    int             nchars;
    int             status;
    int             true = TRUE;
        
    client->tid = threadGetIdSelf ();

    taskwdInsert (threadGetIdSelf(), NULL, NULL);

    while (TRUE) {
        client->recv.stk = 0;
            
        nchars = recv (client->sock, &client->recv.buf[client->recv.cnt], 
                (int)(sizeof(client->recv.buf)-client->recv.cnt), 0);
        if (nchars==0){
            if (CASDEBUG>0) {
                errlogPrintf ("CAS: nill message disconnect\n");
            }
            break;
        }
        else if (nchars<0) {
            int anerrno = SOCKERRNO;

            /*
             * normal conn lost conditions
             */
            if (    (anerrno!=SOCK_ECONNABORTED&&
                    anerrno!=SOCK_ECONNRESET&&
                    anerrno!=SOCK_ETIMEDOUT)||
                    CASDEBUG>2) {
                    errlogPrintf ("CAS: client disconnect(errno=%d)\n", anerrno);
            }
            break;
        }

        tsStampGetCurrent (&client->time_at_last_recv);
        client->recv.cnt += (unsigned long) nchars;

        status = camessage (client, &client->recv);
        if (status == 0) {
            /*
             * if there is a partial message
             * align it with the start of the buffer
             */
            if (client->recv.cnt >= client->recv.stk) {
                unsigned bytes_left;
                char *pbuf;

                bytes_left = client->recv.cnt - client->recv.stk;

                pbuf = client->recv.buf;

                /*
                 * overlapping regions handled
                 * properly by memmove 
                 */
                memmove (pbuf, pbuf + client->recv.stk, bytes_left);
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
            ipAddrToA (&client->addr, buf, sizeof(buf));
            errlogPrintf ("CAS: forcing disconnect from %s\n", buf);
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
    
    destroy_client (client);
}
