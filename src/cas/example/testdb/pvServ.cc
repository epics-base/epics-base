
// $Id$
// $Log$
// Revision 1.3  1997/06/13 09:15:47  jhill
// connect proto changes
//
// Revision 1.2  1997/03/05 21:16:22  jbk
// Fixes cvs log id at top
//

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

	server = new serv(total_pv,rate,name,total_pv);
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

	int not_done=1;
	
	if (event_rate>0) {
		Debug1("Update every %f sec\n", inv);
		double inv=(1.0/event_rate);
		pScanTimer = new scanTimer (inv, *this);
	}

	while(not_done)
	{
		osiTime delay(10.0);
		fileDescriptorManager.process(delay);
	}

	return 0;
}

// ------------------------- server stuff ----------------------------

serv::serv(int tot,double rate,char* name,unsigned pv_count_est):
	caServer(pv_count_est),
	db_sync(NULL),pv_total(tot),event_rate(rate),prefix(name),
	pScanTimer(NULL)
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
	if (pScanTimer) delete pScanTimer;
}

pvExistReturn serv::pvExistTest(const casCtx&,const char* pvname)
{
	int val;
	dBase* node=NULL;
	pvExistReturnEnum rc=pverDoesNotExistHere;

	if(strncmp(pvname,prefix,prefix_len)==0)
	{
		// we may have this one, number is after underscore
		if(sscanf(&pvname[prefix_len+1],"%d",&val)==1)
		{
			if(val>=0 && val<pv_total)
			{
				Debug("ExistTest: I have this PV\n");
				node=&db_sync[val];
				rc=pverExistsHere;
			}
		}
	}

	return pvExistReturn(rc);
}

pvCreateReturn serv::createPV(const casCtx& in,const char* pvname)
{
	casPV* pPV=NULL;
	int val;

	Debug1("createPV: %s\n",pvname);

	if(strncmp(pvname,prefix,prefix_len)==0)
	{
		// we may have this one, number is after underscore
		if(sscanf(&pvname[prefix_len+1],"%d",&val)==1)
		{
			Debug("createPV: I am making this PV\n");
			if(val>=0 && val<pv_total)
				pPV=new servPV(*this,pvname,db_sync[val]);
		}
	}

	if (pPV) {
		return pvCreateReturn(*pPV);
	}
	else {
		return pvCreateReturn(S_casApp_pvNotFound);
	}
}

void serv::scan(void)
{
	unsigned i;
	for(i=0;i<pv_total;i++)
		db_sync[i].eventReady();
}

// -----------------------PV stuff -------------------------------

servPV::servPV(serv& m,const char* n,dBase& x):
	casPV(m),db(x),mgr(m),monitored(0)
{
	db.node=this;
	value=new gddScalar(appValue,aitEnumFloat64);
	pName=new char [strlen(n)+1];
	assert(pName);
	strcpy(pName,n);
}

servPV::~servPV(void)
{
	value->unreference();
	db.node=NULL;
	delete [] pName;
}

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

const char *servPV::getName() const
{
	return pName;
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

//
// scanTimer::expire ()
//
void scanTimer::expire ()
{
        serv.scan();
}

//
// scanTimer::again()
//
osiBool scanTimer::again() const
{
	return osiTrue;
}

//
// scanTimer::delay()
//
const osiTime scanTimer::delay() const
{
	return period;
}

