
// Author: Jim Kowalkowski
// Date: 1/97
// 
// $Id$
// $Log$
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>

#include "caProto.h"
#include "gddAppTable.h"

#include "monServer.h"
#include "monNode.h"

int debugLevel=0;
int makeReport=0;
int keepGoing=1;

typedef void (*SIG_FUNC)(int);

static SIG_FUNC save_hup = NULL;
static SIG_FUNC save_int = NULL;
static SIG_FUNC save_term = NULL;
static SIG_FUNC save_usr1 = NULL;

static void sig_end(int)
{
	signal(SIGHUP,sig_end);
	signal(SIGTERM,sig_end);
	signal(SIGINT,sig_end);
	keepGoing=0;
}
static void sig_usr1(int) { makeReport=1; signal(SIGUSR1,sig_usr1); }

extern int monAdl(monNode**,char*,const char*);

// ------------------------- file descriptor servicing ----------------------

monFd::~monFd(void) { }
void monFd::callBack(void) { server.dataReady(); }

// -------------------------- server -----------------------------

monServer::monServer(unsigned namelen,unsigned pvcount,unsigned simio,
	const char* p):caServer(namelen,pvcount,simio)
{
	int i;
	count_pv=NULL;
	const char* x = p?p:"_";
	prefix=new char[strlen(x)+1];
	strcpy(prefix,x);
	prefix_length=strlen(prefix);
	soc_fd=NULL;
	db=new monNode*[ADDR_TOTAL];
	for(i=0;i<ADDR_TOTAL;i++) db[i]=NULL;

	gddApplicationTypeTable& tt = gddApplicationTypeTable::AppTable();
	appValue=tt.getApplicationType("value");

	sprintf(ioc_count_name,"%siocCount",prefix);
	sprintf(make_screen_name,"%smakeScreen",prefix);
}

monServer::~monServer(void)
{
	int i;
	monNode *c,*p;
	for(i=0;i<ADDR_TOTAL;i++)
	{
		for(c=db[i];c;)
		{
			p=c->next;
			delete c;
			c=p;
		}
	}
	delete [] db;
}

void monServer::report(FILE* fd)
{
	int i;
	monNode* n;
	for(i=0;i<ADDR_TOTAL;i++)
	{
		for(n=db[i];n;n=n->next)
			n->report(fd);
	}
}

monNode* monServer::findNode(unsigned long addr)
{
	monNode* t;
	for(t=db[addr&ADDR_MASK];t && t->addr!=addr;t=t->next);
	return t;
}

monNode* monServer::addNode(unsigned long addr)
{
	unsigned long i;
	monNode* t;
	if((t=findNode(addr))==NULL)
	{
		i=addr&ADDR_MASK;
		t=new monNode(addr,prefix);
		t->next=db[i];
		db[i]=t;
		pv_list.add(t->name,*t);
		if(count_pv) count_pv->postValue();
	}
	else
		t->ping();

	return t;
}

void monServer::checkEvent(void)
{
	// go through all the nodes and send out monitors on PV if required
	time_t t;
	int i;
	monNode* n;
	time(&t);

	for(i=0;i<ADDR_TOTAL;i++)
	{
		for(n=db[i];n;n=n->next)
			n->check(t);
	}
}

pvExistReturn monServer::pvExistTest(const casCtx& c,const char* pvname)
{
	monNode* node;

	if(strcmp(pvname,ioc_count_name)==0)
		return pvExistReturn(S_casApp_success,ioc_count_name);

	if(strcmp(pvname,make_screen_name)==0)
		return pvExistReturn(S_casApp_success,make_screen_name);

	if(pv_list.find(pvname,node)==0)
		return pvExistReturn(S_casApp_success,node->name);
	else
		return pvExistReturn(S_casApp_pvNotFound);
}

casPV* monServer::createPV(const casCtx& c,const char* pvname)
{
	monNode* node;

	if(strcmp(pvname,ioc_count_name)==0)
		return count_pv=new iocCountPV(c,pvname,*this);

	if(strcmp(pvname,make_screen_name)==0)
		return new makeScreenPV(c,pvname,*this);

	if(pv_list.find(pvname,node)==0)
		return new monPv(c,*this,node,pvname);
	else
		return NULL;
}

int monServer::repeaterConnect(void)
{
	caHdr msg;
	struct sockaddr_in tsin;
	struct sockaddr ssin;
	struct timeval tout;
	fd_set fds;
	int retry,done,rlen,len;

	tsin.sin_port=htons(0);
	tsin.sin_family=AF_INET;
	tsin.sin_addr.s_addr=htonl(INADDR_ANY);

	if((soc=socket(AF_INET,SOCK_DGRAM,17))<0)
	{
		perror("open socket failed");
		return -1;
	}

	if((bind(soc,(struct sockaddr*)&tsin,sizeof(tsin)))<0)
	{
		perror("local bind failed to soc failed");
		close(soc);
		return -1;
	}

	memset((char*)&msg,0,sizeof(msg));
	msg.m_cmmd = htons(REPEATER_REGISTER);
	msg.m_available = tsin.sin_addr.s_addr;
	tsin.sin_port=htons(REPEATER_PORT);

	for(done=0,retry=0;done==0 && retry<3;retry++)
	{
		if(sendto(soc,(char*)&msg,sizeof(msg),0,
			(struct sockaddr*)&tsin,sizeof(tsin))<0)
		{
			perror("sendto failed");
			close(soc);
			return -1;
		}

		FD_ZERO(&fds);
		FD_SET(soc,&fds);
		tout.tv_sec=0;
		tout.tv_usec=500000;

		switch(select(FD_SETSIZE,&fds,NULL,NULL,&tout))
		{
		case -1: /* bad */
			perror("first select failed");
			return -1;
		case 0: /* timeout */
			break;
		default: /* data ready */
			done=1;
		}
	}

	if(done==1)
	{
		rlen=0;
		if((len=recvfrom(soc,(char*)&msg,sizeof(msg),0,&ssin,&rlen))<0)
		{
			perror("first recvfrom failed");
			return -1;
		}

		if(ntohs(msg.m_cmmd)==REPEATER_CONFIRM)
			printf("Connected to repeater\n");
		else
		{
			printf("Cannot connect to repeater (%d)\n",(int)ntohs(msg.m_cmmd));
			return -1;
		}
	}
	else
	{
		printf("Cannot connect to repeater\n");
		return -1;
	}

	soc_fd=new monFd(soc,fdrRead,*this);

	return 0;
}

void monServer::dataReady(void)
{
	caHdr msg;
	struct sockaddr ssin;
	monNode* node;
	int rlen,len;
	unsigned long* iaddr;

	rlen=0;
	if((len=recvfrom(soc,(char*)&msg,sizeof(msg),0,&ssin,&rlen))<0)
	{
		perror("first recvfrom failed");
	}
	else if(ntohs(msg.m_cmmd)==CA_PROTO_RSRV_IS_UP)
	{
		iaddr=(unsigned long*)&msg.m_available;
		node=addNode(*iaddr);
	}
}

void monServer::mainLoop(void)
{
	osiTime delay(ADDR_CHECK,0u);
	time_t curr,prev;

	if(repeaterConnect()<0) return;

	prev=0;
	while(keepGoing)
	{
		fileDescriptorManager.process(delay);
		time(&curr);
		if((curr-prev)>=ADDR_CHECK)
		{
			checkEvent();
			prev=curr;
		}

		if(makeReport)
		{
			FILE* fd;
			if((fd=fopen("PV_REPORT","w")))
			{
				report(fd);
				fclose(fd);
			}
			makeADL();
			makeReport=0;
		}
	}
}

void monServer::makeADL(void) { monAdl(db,"ioc_status.adl",prefix); }

// ------------------------------------------------------------------------

int main(int argc, char* argv[])
{
	if(argc<2)
	{
		fprintf(stderr,"Must enter a prefix for PVs on command line\n");
		return -1;
	}

	if(argc==3) sscanf(argv[2],"%d",&debugLevel);

	// disassociate from parent
	switch(fork())
	{
	case -1: // error
		perror("Cannot create gateway processes");
		return -1;
	case 0: // child
#if defined linux || defined SOLARIS
		setpgrp();
#else
		setpgrp(0,0);
#endif
		setsid();
		break;
	default: // parent
		return 0;
		break;
	}

	save_hup=signal(SIGHUP,sig_end);
	save_term=signal(SIGTERM,sig_end);
	save_int=signal(SIGINT,sig_end);
	save_usr1=signal(SIGUSR1,sig_usr1);

	monServer* ms = new monServer(32u,5u,2000u,argv[1]);
	ms->mainLoop();
	delete ms;
	return 0;
}

