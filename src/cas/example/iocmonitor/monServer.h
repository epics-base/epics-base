#ifndef MONSERVER_H
#define MONSERVER_H

/*
 * Author: Jim Kowalkowski
 * Date: 1/97
 * 
 * $Id$
 * $Log$
 */

#include "casdef.h"
#include "tsHash.h"
#include "fdManager.h"

#include "monNode.h"

class gdd;
class monServer;

// use lower 11 bits of address as index
#define ADDR_MASK       0x0000001f
#define ADDR_TOTAL      (ADDR_MASK+1)
#define REPEATER_PORT   5065

// ---------------------- fd manager ------------------------

class monFd : public fdReg
{
public:
	monFd(const int fdIn,const fdRegType typ,monServer& s):
		fdReg(fdIn,typ),server(s) { } 
	virtual ~monFd(void);
private:
	virtual void callBack(void);
	monServer& server;
};

// ---------------------------- server -------------------------------

class monServer : public caServer
{
public:
	monServer(unsigned max_name_len,unsigned pv_count_est,unsigned max_sim_io,
		const char* pre);
	virtual ~monServer(void);

	// CAS virtual overloads
	virtual pvExistReturn pvExistTest(const casCtx& c,const char* pvname);
	virtual casPV* createPV(const casCtx& c,const char* pvname);

	// CAS application management functions
	int repeaterConnect(void);
	void checkEvent(void);
	void dataReady(void);
	void mainLoop(void);
	void report(FILE*);
	void makeADL(void);

	monNode* findNode(unsigned long addr);
    monNode* addNode(unsigned long addr);

	int appValue;
	char* prefix;
	int prefix_length;
	iocCountPV* count_pv;
private:
	tsHash<monNode> pv_list;		// client pv list
	monNode** db;
	monFd* soc_fd;
	int soc;
	char ioc_count_name[40];
	char make_screen_name[40];
};

extern int debugLevel;

/* debug macro creation */
#ifdef NODEBUG
#define monDebug(l,f,v) ;
#else
#define monDebug(l,f,v) { if(l<=debugLevel) \
   { fprintf(stderr,f,v); fflush(stderr); }}
#define monDebug0(l,f) { if(l<=debugLevel) \
   { fprintf(stderr,f); fflush(stderr); } }
#define monDebug1(l,f,v) { if(l<=debugLevel) \
   { fprintf(stderr,f,v); fflush(stderr); }}
#define monDebug2(l,f,v1,v2) { if(l<=debugLevel) \
   { fprintf(stderr,f,v1,v2); fflush(stderr); }}
#define monDebug3(l,f,v1,v2,v3) { if(l<=debugLevel) \
   { fprintf(stderr,f,v1,v2,v3); fflush(stderr); }}
#endif

#endif
