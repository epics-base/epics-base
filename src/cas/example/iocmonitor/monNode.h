#ifndef MONNODE_H
#define MONNODE_H

#include <time.h>

#include "aitTypes.h"
#include "casdef.h"

typedef enum { monAlive=0, monUnknown, monDead } monState;

#define ADDR_CHECK      10
#define ADDR_WARNING    20
#define ADDR_DEATH      40

class monNode;
class monServer;
class gdd;

class monPv : public casPV
{
public:
	monPv(const casCtx&,monServer&,monNode*,const char* pv_name);
	virtual ~monPv(void);

	// CA server interface functions
	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual aitEnum bestExternalType(void) const;
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, gdd &value);
	virtual void destroy(void);
	virtual unsigned maxSimultAsyncOps(void) const;
	virtual unsigned maxDimension(void) const;
	virtual aitIndex maxBound(unsigned dim) const;

	void eventData(void);

	void markInterested(void)		{ monitor=1; }
    void markNotInterested(void)	{ monitor=0; }
    int isMonitored(void)			{ return monitor; }

	monServer& mrg;
	monNode* node;
	int monitor;
	gdd* data;
};

class monNode
{
public:
	monNode(unsigned long a, const char* prefix);
	~monNode(void);

	void report(FILE*);
	void ping(void);
	void check(time_t);
	monState state(void);
	monState state(time_t);
	void setPv(monPv*);
	void clearPv(void);

	unsigned long addr;
	time_t last_ping;
	time_t state_ping;
	const char* name;
	monPv* pv;
	monNode* next;
	monState last_state;
	static int total;
};

// ---------------------------- status PVs --------------------------

class makeScreenPV : public casPV
{
public:
	makeScreenPV(const casCtx& c,const char* pv_name,monServer& s):
		casPV(c,pv_name),mrg(s){}
	virtual ~makeScreenPV(void);

	// CA server interface functions
	virtual aitEnum bestExternalType(void) const;
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, gdd &value);

	monServer& mrg;
};

class iocCountPV : public casPV
{
public:
	iocCountPV(const casCtx& c,const char* pv_name,monServer& s):
		casPV(c,pv_name),mrg(s),monitored(0) {}
	virtual ~iocCountPV(void);

	// CA server interface functions
	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual aitEnum bestExternalType(void) const;
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, gdd &value);

	void postValue(void);

	int monitored;
	monServer& mrg;
};

#endif
