
// Author: Jim Kowalkowski
// Date: 1/97
//
// $Id$
// $Log$

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

// use lower 11 bits of address as index

#define ADDR_MASK		0x000007ff
#define ADDR_CHECK		10
#define ADDR_WARNING	20
#define ADDR_DEATH		40
#define REPEATER_PORT	5065

typedef enum { nodeAlive=0, nodeUnknown, nodeDead } nodeState;

// ----------------------------- host node ------------------------------

class netNode
{
public:
	netNode(unsigned long a,const char* pre);
	~netNode(void);

	void report(void);
	void ping(void);
	nodeState state(void);
	nodeState state(time_t);

	unsigned long addr;
	time_t last_ping;
	const char* name;
	struct netNode* next;
};

netNode::netNode(unsigned long a,const char* pre)
{
	struct in_addr& in = (struct in_addr&)a;
	struct hostent* entry;
	char *n,*x;

	addr=a;
	time(&last_ping);

	if((entry=gethostbyaddr((char*)&a,sizeof(a),AF_INET))==NULL)
		n=inet_ntoa(in);
	else
		n=entry->h_name;

	x=new char[strlen(n)+1+strlen(pre)];
	strcpy(x,pre);
	strcat(x,n);
	name=x;
	next=NULL;
}

netNode::~netNode(void)
{
	char* n = (char*)name;
	delete [] n;
}

void netNode::ping(void)
{
	time(&last_ping);
}

nodeState netNode::state(void)
{
	time_t t;
	time(&t);
	return state(t);
}

nodeState netNode::state(time_t t)
{
	nodeState s;
	time_t x;
	x=t-last_ping;
	if(x<ADDR_WARNING)
		s=nodeAlive;
	else if(x<ADDR_DEATH)
		s=nodeUnknown;
	else
		s=nodeDead;
	return s;
}

void netNode::report(void)
{
	switch(state())
	{
	case nodeDead:		printf(" IOC %s Dead\n",name); break;
	case nodeAlive:		printf(" IOC %s Alive\n",name); break;
	case nodeUnknown:	printf(" IOC %s Unknown\n",name); break;
	}
}

// -------------------------- database ---------------------------

class addrDb
{
public:
	addrDb(const char* p);
	~addrDb(void);

	void report(void);
	netNode* findNode(unsigned long addr);
	netNode* addNode(unsigned long addr);
private:
	const char* prefix;
	netNode** db;
};

addrDb::addrDb(const char* p)
{
	int i;
	const char* x = p?p:"_";
	char* y;
	y=new char[strlen(x)+1];
	strcpy(y,x);
	prefix=y;
	db=new netNode*[ADDR_MASK];
	for(i=0;i<ADDR_MASK;i++) db[i]=NULL;
}

addrDb::~addrDb(void)
{
	int i;
	netNode *c,*p;
	for(i=0;i<ADDR_MASK;i++)
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

void addrDb::report(void)
{
	int i;
	netNode* n;
	for(i=0;i<ADDR_MASK;i++)
	{
		for(n=db[i];n;n=n->next)
			n->report();
	}
}

netNode* addrDb::findNode(unsigned long addr)
{
	netNode* t;
	for(t=db[addr&ADDR_MASK];t && t->addr!=addr;t=t->next);
	return t;
}

netNode* addrDb::addNode(unsigned long addr)
{
	unsigned long i;
	netNode* t;
	if((t=findNode(addr))==NULL)
	{
		i=addr&ADDR_MASK;
		t=new netNode(addr,prefix);
		t->next=db[i];
		db[i]=t;
	}
	else
		t->ping();

	return t;
}

// ------------------------------------------------------------------------

addrDb* db;

int main(int argc, char* argv[] )
{
	caHdr msg;
	struct sockaddr_in tsin;
	struct sockaddr ssin;
	struct timeval tout;
	struct hostent* entry;
	fd_set fds;
	char* name;
	netNode* node;
	int soc,retry,done,rlen,len;
	unsigned short cmd;
	unsigned long* iaddr;
	time_t curr,last;

	if(argc<2)
	{
		fprintf(stderr,"Must enter a prefix for PVs on command line\n");
		return -1;
	}

	db=new addrDb(argv[1]);

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

		cmd=ntohs(msg.m_cmmd);

		if(cmd==REPEATER_CONFIRM)
			printf("Connected to repeater\n");
		else
		{
			printf("Cannot connect to repeater (%d)\n",(int)cmd);
			return -1;
		}
	}
	else
	{
		printf("Cannot connect to repeater\n");
		return -1;
	}

	/* ---------------- ready ---------------- */
	last=0;

	while(1)
	{
		FD_ZERO(&fds);
		FD_SET(soc,&fds);
		tout.tv_sec=ADDR_CHECK;
		tout.tv_usec=0;

		switch(select(FD_SETSIZE,&fds,NULL,NULL,&tout))
		{
		case -1: /* bad */
			perror("main select failed");
		case 0: /* timeout */
			// db->report();
			break;
		default: /* data ready */
		  {
			if((len=recvfrom(soc,(char*)&msg,sizeof(msg),0,&ssin,&rlen))<0)
			{
				perror("first recvfrom failed");
				return -1;
			}

			iaddr=(unsigned long*)&msg.m_available;
			node=db->addNode(*iaddr);

			// cmd=ntohs(msg.m_cmmd);
			// if(cmd==CA_PROTO_RSRV_IS_UP)
			// {
			// 	printf("Alive\n");
			// }
			// else
			// 	printf("Unidentified Command from 0x%8.8x\n",*iaddr);
			break;
		  }
		}
		time(&curr);
		if((curr-last)>=ADDR_CHECK)
		{
			db->report();
			last=curr;
		}
	}

	return 0;
}
