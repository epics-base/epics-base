
/*
 * $Id$
 * $Log$
 * Revision 1.2  1997/03/05 21:16:23  jbk
 * Fixes cvs log id at top
 *
 */

#include "casdef.h"
#include "osiTimer.h"

class gdd;
class servPV;

#ifdef PVDEBUG
#define Debug(str) { fprintf(stderr,str); }
#define Debug1(str,val) { fprintf(stderr,str,val); }
#define Debug2(str,val1,val2) { fprintf(stderr,str,val1,val2); }
#define Debug3(str,val1,val2,val3) { fprintf(stderr,str,val1,val2,val3); }
#else
#define Debug(str) ;
#define Debug1(str,val) ;
#define Debug2(str,val1,val2) ;
#define Debug3(str,val1,val2,val3) ;
#endif

class dBase
{
public:
	dBase(void)
	{
		pvname[0]='\0';counter=0;
		hihi=90.0;high=80.0;low=20.0;lolo=10.0;
		ts.tv_sec=0; ts.tv_nsec=0;
		stat=0; sevr=0;
		node=NULL;
	}

	void eventReady(void);
	void writeData(double);

	char pvname[50];
	double counter;
	double hihi,high,low,lolo;
	aitTimeStamp ts;
	aitInt16 stat,sevr;
	servPV* node;
};

class serv;

//
// scanTimer
//
class scanTimer : public osiTimer {
public:
	scanTimer (double delayIn, serv &servIn) : 
		osiTimer(delayIn), serv(servIn),
		period(delayIn) {}
	void expire ();
	osiBool again() const;
	const osiTime delay() const;
private:
	serv	&serv;
	double period;
};

class serv : public caServer
{
public:
	serv(int totpv,double rate,char* prefix,unsigned pvtotalest);
	virtual ~serv(void);

	virtual pvExistReturn pvExistTest(const casCtx& c,const char* pvname);
	virtual pvCreateReturn createPV(const casCtx& c,const char* pvname);

	int InitDB(void);
	int Main(void);

	void scan();

	// sloppy
	char* prefix;
	int prefix_len;
	int pv_total;
	double event_rate;
	casEventMask event_mask;
	dBase* db_sync;
	scanTimer *pScanTimer;
};

class servPV : public casPV
{
public:
	servPV(serv&,const char* pvname,dBase&);
	virtual ~servPV(void);

	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual aitEnum bestExternalType(void) const;
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, gdd &value);
	virtual void destroy(void);
	virtual const char *getName() const;

	void eventReady(void);

private:
	serv& mgr;
	dBase& db;
	gdd* value;
	int monitored;
	char *pName;
};


