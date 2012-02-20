/*caMonitor.c*/

/* This example accepts the name of a file containing a list of pvs to monitor.
 * It prints a message for all ca events: connection, access rights and monitor.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cadef.h"
#include "dbDefs.h"
#include "epicsString.h"
#include "cantProceed.h"

#define MAX_PV 1000
#define MAX_PV_NAME_LEN 40

typedef struct{
    char		value[20];
    chid		mychid;
    evid		myevid;
} MYNODE;


static void printChidInfo(chid chid, char *message)
{
    printf("\n%s\n",message);
    printf("pv: %s  type(%d) nelements(%ld) host(%s)",
	ca_name(chid),ca_field_type(chid),ca_element_count(chid),
	ca_host_name(chid));
    printf(" read(%d) write(%d) state(%d)\n",
	ca_read_access(chid),ca_write_access(chid),ca_state(chid));
}

static void exceptionCallback(struct exception_handler_args args)
{
    chid	chid = args.chid;
    long	stat = args.stat; /* Channel access status code*/
    const char  *channel;
    static char *noname = "unknown";

    channel = (chid ? ca_name(chid) : noname);


    if(chid) printChidInfo(chid,"exceptionCallback");
    printf("exceptionCallback stat %s channel %s\n",
        ca_message(stat),channel);
}

static void connectionCallback(struct connection_handler_args args)
{
    chid	chid = args.chid;

    printChidInfo(chid,"connectionCallback");
}

static void accessRightsCallback(struct access_rights_handler_args args)
{
    chid	chid = args.chid;

    printChidInfo(chid,"accessRightsCallback");
}
static void eventCallback(struct event_handler_args eha)
{
    chid	chid = eha.chid;

    if(eha.status!=ECA_NORMAL) {
	printChidInfo(chid,"eventCallback");
    } else {
	char	*pdata = (char *)eha.dbr;
	printf("Event Callback: %s = %s\n",ca_name(eha.chid),pdata);
    }
}

int main(int argc,char **argv)
{
    char *filename;
    int		npv = 0; 
    MYNODE	*pmynode[MAX_PV];
    char	*pname[MAX_PV];
    int		i;
    char	tempStr[MAX_PV_NAME_LEN];
    char	*pstr;
    FILE	*fp;

    if (argc != 2) {
        fprintf(stderr,"usage: caMonitor filename\n");
        exit(1);
    }
    filename = argv[1];
    fp = fopen(filename,"r");
    if (!fp) {
        perror("fopen failed");
        return(1);
    }
    while (npv < MAX_PV) {
        size_t len;

        pstr = fgets(tempStr, MAX_PV_NAME_LEN, fp);
        if (!pstr) break;

        len = strlen(pstr);
        if (len <= 1) continue;

        pstr[len - 1] = '\0'; /* Strip newline */
        pname[npv] = epicsStrDup(pstr);
        pmynode[npv] = callocMustSucceed(1, sizeof(MYNODE), "caMonitor");
        npv++;
    }
    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create");
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL),
	"ca_add_exception_event");
    for (i=0; i<npv; i++) {
	SEVCHK(ca_create_channel(pname[i],connectionCallback,
		pmynode[i],20,&pmynode[i]->mychid),
		"ca_create_channel");
	SEVCHK(ca_replace_access_rights_event(pmynode[i]->mychid,
		accessRightsCallback),
		"ca_replace_access_rights_event");
	SEVCHK(ca_create_subscription(DBR_STRING,1,pmynode[i]->mychid,
		DBE_VALUE,eventCallback,pmynode[i],&pmynode[i]->myevid),
		"ca_create_subscription");
    }
    /*Should never return from following call*/
    SEVCHK(ca_pend_event(0.0),"ca_pend_event");
    return 0;
}
