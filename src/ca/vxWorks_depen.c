/*
 *	$Id$
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *      Date:  9-93
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      Modification Log:
 *      -----------------
 *
 */

#include "iocinf.h"
#include "remLib.h"

void ca_repeater_task();


/*
 *
 * localUserName() - for vxWorks
 *
 * o Indicates failure by setting ptr to nill
 */
char *localUserName()
{
	char	*pTmp;
	int	length;
	char    pName[MAX_IDENTITY_LEN];

	remCurIdGet(pName, NULL);

	length = strlen(pName)+1;
	pTmp = malloc(length);
	if(!pTmp){
		return NULL;
	}
	strncpy(pTmp, pName, length-1);
	pTmp[length-1] = '\0';

	return pTmp;
}



/*
 * caHostFromInetAddr()
 */
#ifdef __STDC__
void caHostFromInetAddr(struct in_addr *pnet_addr, char *pBuf, unsigned size)
#else /*__STDC__*/
void caHostFromInetAddr(pnet_addr, pBuf, size)
struct in_addr  *pnet_addr;
char            *pBuf;
unsigned        size;
#endif /*__STDC__*/
{
        char    str[INET_ADDR_LEN];

	inet_ntoa_b(*pnet_addr, str);

        /*
         * force null termination
         */
        strncpy(pBuf, str, size-1);
        pBuf[size-1] = '\0';

        return;
}


/*
 * CA_IMPORT()
 *
 *
 */
#ifdef __STDC__
int ca_import(int tid)
#else
int ca_import(tid)
int             tid;
#endif
{
        int             status;
        struct ca_static *pcas;
        TVIU            *ptviu;

        status = ca_check_for_fp();
        if(status != ECA_NORMAL){
                return status;
        }

	/*
	 * just return success if they have already done
	 * a ca import for this task
	 */
        pcas = (struct ca_static *)
                taskVarGet(taskIdSelf(), (int *)&ca_static);
        if (pcas != (struct ca_static *) ERROR){
                return ECA_NORMAL;
        }

        ptviu = calloc(1, sizeof(*ptviu));
        if(!ptviu){
                return ECA_ALLOCMEM;
        }

        pcas = (struct ca_static *)
                taskVarGet(tid, (int *)&ca_static);
        if (pcas == (struct ca_static *) ERROR){
                free(ptviu);
                return ECA_NOCACTX;
        }

        ca_static = NULL;

        status = taskVarAdd(VXTHISTASKID, (int *)&ca_static);
        if (status == ERROR){
                free(ptviu);
                return ECA_ALLOCMEM;
        }

        ca_static = pcas;

        ptviu->tid = taskIdSelf();
        LOCK;
        ellAdd(&ca_static->ca_taskVarList, &ptviu->node);
        UNLOCK;

        return ECA_NORMAL;
}


/*
 * CA_IMPORT_CANCEL()
 */
#ifdef __STDC__
int ca_import_cancel(int tid)
#else /*__STDC__*/
int ca_import_cancel(tid)
int             tid;
#endif /*__STDC__*/
{
        int     status;
        TVIU    *ptviu;

        LOCK;
        ptviu = (TVIU *) ca_static->ca_taskVarList.node.next;
        while(ptviu){
                if(ptviu->tid == tid){
                        break;
                }
        }

        if(!ptviu){
                return ECA_NOCACTX;
        }

        ellDelete(&ca_static->ca_taskVarList, &ptviu->node);
        UNLOCK;

        status = taskVarDelete(tid, (void *)&ca_static);
        assert (status == OK);

        return ECA_NORMAL;
}


/*
 * ca_check_for_fp()
 */
int ca_check_for_fp()
{
        {
                int             options;

                assert(taskOptionsGet(taskIdSelf(), &options) == OK);
                if (!(options & VX_FP_TASK)) {
                        return ECA_NEEDSFP;
                }
        }
        return ECA_NORMAL;
}



/*
 *      ca_spawn_repeater()
 *
 *      Spawn the repeater task as needed
 */
void ca_spawn_repeater()
{
	int     status;

	status = taskSpawn(
                           CA_REPEATER_NAME,
                           CA_REPEATER_PRI,
                           CA_REPEATER_OPT,
                           CA_REPEATER_STACK,
			   (FUNCPTR)ca_repeater_task,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL);
	if (status < 0){
       		SEVCHK(ECA_NOREPEATER, NULL);
        }
}


/*
 * ca_repeater_task()
 */
void ca_repeater_task()
{
	taskwdInsert((int)taskIdCurrent, NULL, NULL);
        ca_repeater();
}

