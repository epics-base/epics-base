
#include <stdio.h>
#include "pvServ.h"
#include "fdManager.h"
#include "gdd.h"
#include "gddAppTable.h"
#include "alarm.h"

static int appValue;

void dBase::eventReady(void)
{
	if(counter+1.0 > 100.0)
		writeData(0.0);
	else
		writeData(counter+1.0);
}

void dBase::writeData(double x)
{
	counter=x;
	time((time_t*)&ts.tv_sec);

	if(counter>hihi)
		{ sevr=MAJOR_ALARM; stat=HIHI_ALARM; }
	else if(counter<lolo)
		{ sevr=MAJOR_ALARM; stat=LOLO_ALARM; }
	else if(counter>high)
		{ sevr=MINOR_ALARM; stat=HIGH_ALARM; }
	else if(counter<low)
		{ sevr=MINOR_ALARM; stat=LOW_ALARM; }
	else
		{ sevr=NO_ALARM; stat=0; }

	if(node) node->eventReady();
}

int main(int argc, char* argv[])
{
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
	int rc;
	int total_pv;
	double rate;
	serv* server;
	char* name;

	if(argc<3)
	{
		fprintf(stderr,"Usage %s pv_total monitor_rate optional_pv_prefix\n",
			argv[0]);
		fprintf(stderr,"  pv_total is the number of PVs to generate (int)\n");
		fprintf(stderr,"  monitor_rate is events per second (float)\n");
		fprintf(stderr,"  optional_pv_prefix defaults to your login name\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"  PVs are named <PREFIX>_#\n");
		fprintf(stderr,"  where # is an integer is the range [0,pv_total)\n");

		return -1;
	}

	if(sscanf(argv[1],"%d",&total_pv)!=1)
	{
		fprintf(stderr,"Failed to convert pv_total argument to number\n");
		return -1;
	}
	if(sscanf(argv[2],"%lf",&rate)!=1)
	{
		fprintf(stderr,"Failed to convert monitor_rate argument to number\n");
		return -1;
	}
	if(argc<=4)
		name=argv[3];
	else
		name=NULL;

	appValue=table.getApplicationType("value");

	Debug3("total=%d,rate=%lf,prefix=%s\n",total_pv,rate,name);

	server = new serv(total_pv,rate,name,40u,total_pv,total_pv);
	rc=server->Main();
	delete server;
	return rc;
}

int serv::InitDB(void)
{
	unsigned i;

	db_sync=new dBase[pv_total];

	for(i=0;i<pv_total;i++)
		sprintf(db_sync[i].pvname,"%s_%d",prefix,i);

	return 0;
}

int serv::Main(void)
{
	unsigned i;
	int not_done=1;

	double inv=(1.0/event_rate);
	double num=(unsigned long)inv;
	double fract=inv-num;
	unsigned long lfract=fract?(unsigned long)(1.0/fract):0;
	unsigned long nsec = lfract?1000000000u/lfract:0;
	unsigned long sec = (unsigned long)num;
	struct timeval tv_curr,tv_prev;

	osiTime delay(sec,nsec);

	tv_prev.tv_sec=0;
	tv_prev.tv_usec=0;

	Debug2("Update every sec=%lu nsec=%lu\n",sec,nsec);

	while(not_done)
	{
		fileDescriptorManager.process(delay);

		gettimeofday(&tv_curr,NULL);

		if(tv_curr.tv_sec-tv_prev.tv_sec >= sec && 
			tv_curr.tv_usec-tv_prev.tv_usec >= (nsec/1000))
		{
			for(i=0;i<pv_total;i++)
				db_sync[i].eventReady();
		}

		tv_prev=tv_curr;
	}

	return 0;
}

// ------------------------- server stuff ----------------------------

serv::serv(int tot,double rate,char* name,
		unsigned max_name_len,unsigned pv_count_est,unsigned max_sim_io):
	caServer(max_name_len, pv_count_est, max_sim_io),
	db_sync(NULL),pv_total(tot),event_rate(rate),prefix(name)
{
	event_mask|=(alarmEventMask|valueEventMask|logEventMask);

	if(name)
		prefix=name;
	else if((prefix=getenv("LOGNAME"))==NULL)
		prefix="noname";

	prefix_len=strlen(prefix);

	InitDB();
}

serv::~serv(void)
{
	delete [] db_sync;
}

pvExistReturn serv::pvExistTest(const casCtx&,const char* pvname)
{
	int val;
	dBase* node=NULL;
	caStatus rc=S_casApp_pvNotFound;

	if(strncmp(pvname,prefix,prefix_len)==0)
	{
		// we may have this one, number is after underscore
		if(sscanf(&pvname[prefix_len+1],"%d",&val)==1)
		{
			if(val>=0 && val<pv_total)
			{
				Debug("ExistTest: I have this PV\n");
				node=&db_sync[val];
				rc=S_casApp_success;
			}
		}
	}

	return (rc==S_casApp_success)?pvExistReturn(rc,node->pvname):
		pvExistReturn(rc);
}

casPV* serv::createPV(const casCtx& in,const char* pvname)
{
	casPV* rc=NULL;
	int val;

	Debug1("createPV: %s\n",pvname);

	if(strncmp(pvname,prefix,prefix_len)==0)
	{
		// we may have this one, number is after underscore
		if(sscanf(&pvname[prefix_len+1],"%d",&val)==1)
		{
			Debug("createPV: I am making this PV\n");
			if(val>=0 && val<pv_total)
				rc=new servPV(in,*this,pvname,db_sync[val]);
		}
	}

	return rc;
}

// -----------------------PV stuff -------------------------------

servPV::servPV(const casCtx& c,serv& m,const char* n,dBase& x):
	casPV(c,n),db(x),mgr(m),monitored(0)
{
	db.node=this;
	value=new gddScalar(appValue,aitEnumFloat64);
}

servPV::~servPV(void)
{
	value->unreference();
	db.node=NULL;
}

unsigned servPV::maxSimultAsyncOps(void) const { return 100000u; }

caStatus servPV::interestRegister()
{
	if(!monitored) monitored=1;
	return S_casApp_success;
}

void servPV::interestDelete()
{
	if(monitored) monitored=0;
}

aitEnum servPV::bestExternalType() const
{
	return aitEnumFloat64;
}

caStatus servPV::read(const casCtx&, gdd &dd)
{
	Debug1("read: %s\n",db.pvname);
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

	// this is a cheesy way to do this
	value->put(db.counter);
	value->setStatSevr(db.stat,db.sevr);
	value->setTimeStamp(&db.ts);

	table.smartCopy(&dd,value);
	return S_casApp_success;
}

caStatus servPV::write(const casCtx&, gdd &dd)
{
	Debug1("write: %s\n",db.pvname);
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

	// this is also cheesy
	table.smartCopy(value,&dd);
	db.writeData((double)*value);
	return S_casApp_success;
}

void servPV::destroy()
{
	casPV::destroy();
}

void servPV::eventReady(void)
{
	value->put(db.counter);
	value->setStatSevr(db.stat,db.sevr);
	value->setTimeStamp(&db.ts);
	postEvent(mgr.event_mask,*value);
}

