
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "gddAppTable.h"
#include "monServer.h"
#include "monNode.h"

// --------------------------- monPv ----------------------------

monPv::monPv(const casCtx& c,monServer& s,monNode* n,const char* name):
	casPV(c,name),node(n),mrg(s)
{
	monDebug1(5,"monPV: Creating PV for %s\n",name);
	data=NULL;
	markNotInterested();
	node->setPv(this);
}

monPv::~monPv(void)
{
	node->clearPv();
	if(data) data->unreference();
	monDebug1(5,"monPV: Deleting PV for %s\n",node->name);
}

unsigned monPv::maxSimultAsyncOps(void) const { return 5000u; }
void monPv::interestDelete(void) { markNotInterested(); }
aitEnum monPv::bestExternalType(void) const { return aitEnumString; }
unsigned monPv::maxDimension(void) const { return 0; }
aitIndex monPv::maxBound(unsigned dim) const { return 0; }
void monPv::destroy(void) { casPV::destroy(); }

caStatus monPv::interestRegister(void)
{
	markInterested();
	return S_casApp_success;
}

caStatus monPv::read(const casCtx& ctx, gdd& dd)
{
	monDebug1(5,"monPV: Read PV data=0x%8.8x\n",(int)data);
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
	if(data) table.smartCopy(&dd,data);
	return S_casApp_success;
}

caStatus monPv::write(const casCtx& /*ctx*/, gdd& /*dd*/)
{
	// cannot write to these PVs
	return S_casApp_success;
}

void monPv::eventData(void)
{
	struct tm* t;
	char val[40];
	char str[10];
	aitTimeStamp stamp(node->state_ping,0);

	// prepare a new gdd used for posting and reads
	if(data) data->unreference();
	data=new gddScalar(mrg.appValue,aitEnumString);
	t=localtime(&node->state_ping);

	switch(node->state())
	{
	case monAlive:
		strcpy(str,"Up");
		data->setStatSevr(0,NO_ALARM);
		break;
	case monUnknown:
		strcpy(str,"?");
		data->setStatSevr(TIMEOUT_ALARM,MINOR_ALARM);
		break;
	case monDead:
		strcpy(str,"Down");
		data->setStatSevr(COMM_ALARM,MAJOR_ALARM);
		break;
	}

	/*
	sprintf(val,"(%s %s %2.2d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d)",
		&node->name[mrg.prefix_length],str,
		t->tm_mon+1,t->tm_mday,t->tm_year,t->tm_hour,t->tm_min,t->tm_sec);
	*/
	sprintf(val,"(%2.2d/%2.2d %2.2d:%2.2d:%2.2d %s)",
		t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec,
		&node->name[mrg.prefix_length]);

	data->reference();
	data->setTimeStamp(&stamp);
	data->put(val);

	monDebug1(5,"monPV: eventData data=0x%8.8x\n",(int)data);
	if(debugLevel>6) data->dump();
	
	if(isMonitored())
	{
		monDebug0(5,"monPV: PV monitored\n");
	    casEventMask select(
	     mrg.alarmEventMask|mrg.valueEventMask|mrg.logEventMask);
		postEvent(select,*data);
	}
}

// --------------------------- monNode ----------------------------

monNode::monNode(unsigned long a,const char* prefix)
{
	struct in_addr& in = (struct in_addr&)a;
	struct hostent* entry;
	char *n,*x;

	addr=a;
	time(&last_ping);
	state_ping=last_ping;

	if((entry=gethostbyaddr((char*)&a,sizeof(a),AF_INET))==NULL)
		n=inet_ntoa(in);
	else
		n=entry->h_name;

	x=new char[strlen(n)+1+strlen(prefix)];
	strcpy(x,prefix);
	strcat(x,n);
	name=x;
	next=NULL;
	pv=NULL;
	last_state=monAlive;
	++monNode::total;
	monDebug1(5,"monNode: Created node %s\n",name);
}

monNode::~monNode(void)
{
	monDebug1(5,"monNode: Deleted node %s\n",name);
	char* n = (char*)name;
	delete [] n;
}

int monNode::total=0;

void monNode::ping(void)
{
	time(&last_ping);
}

monState monNode::state(void)
{
	return last_state;
}

monState monNode::state(time_t t)
{
	monState s;
	time_t x;
	x=t-last_ping;
	if(x<ADDR_WARNING)
		s=monAlive;
	else if(x<ADDR_DEATH)
		s=monUnknown;
	else
		s=monDead;
	return s;
}

void monNode::check(time_t t)
{
	monState s = state(t);
	monDebug1(9,"monNode: Checking node %s\n",name);

	if(s!=last_state)
	{
		last_state=s;
		// only change boot time if going from/to dead
		if(s==monDead || last_state==monDead) state_ping=t;
		if(pv) pv->eventData();
	}
}

void monNode::setPv(monPv* p)
{
	int need;
	monDebug1(9,"monNode: Setting PV for %s\n",name);

	if(pv==NULL)
		need=1;
	else
		need=0;

	pv=p;
	if(need) pv->eventData();
}

void monNode::clearPv(void) { pv=NULL; }

void monNode::report(FILE* fd)
{
	struct in_addr* ia = (struct in_addr*)&addr;
	switch(state())
	{
	case monDead:
		fprintf(fd,"%s %s Dead\n",name,inet_ntoa(*ia));
		break;
	case monAlive:
		fprintf(fd,"%s %s Alive\n",name,inet_ntoa(*ia));
		break;
	case monUnknown:
		fprintf(fd,"%s %s Unknown\n",name,inet_ntoa(*ia));
		break;
	}
}

// -------------------- status PVs -----------------------

makeScreenPV::~makeScreenPV(void) { }
aitEnum makeScreenPV::bestExternalType(void) const { return aitEnumInt32; }

caStatus makeScreenPV::read(const casCtx& /*ctx*/, gdd& /*dd*/)
{
	return S_casApp_success;
}

caStatus makeScreenPV::write(const casCtx& /*ctx*/, gdd& /*dd*/)
{
	mrg.makeADL();
	return S_casApp_success; // cannot write to these PVs
}

// -------------

iocCountPV::~iocCountPV(void) { mrg.count_pv=NULL; }
void iocCountPV::interestDelete(void) { monitored=0; }

aitEnum iocCountPV::bestExternalType(void) const { return aitEnumInt32; }

void iocCountPV::postValue(void)
{
	gdd* value=new gddScalar(mrg.appValue,aitEnumInt32);
	value->put(monNode::total);

	if(monitored)
	{
		casEventMask select(
			mrg.alarmEventMask|mrg.valueEventMask|mrg.logEventMask);
		postEvent(select,*value);
	}
}

caStatus iocCountPV::interestRegister(void)
{
	monitored=1;
	return S_casApp_success;
}


caStatus iocCountPV::read(const casCtx& ctx, gdd& dd)
{
	// this is bad, should check for dd to be scalar, if it is not a scalar,
	// then find the value gdd within the container
	dd.put(monNode::total);
	return S_casApp_success;
}

caStatus iocCountPV::write(const casCtx& /*ctx*/, gdd& /*dd*/)
{
	return S_casApp_success; // cannot write to these PVs
}

