/*
 * Author: Jim Kowalkowski
 * Date: 4/97
 *
 * $Id$
 * $Log$
 */

#include <vme.h>
#include <vxWorks.h>
#include <iv.h>
#include <intLib.h>
#include <sysLib.h>
#include <semLib.h>

#define MV167 1
#include "timerLib.h"

typedef unsigned long _UL;
typedef volatile unsigned long _VUL;
typedef unsigned char _UC;
typedef volatile unsigned char _VUC;

struct _timerHandle {
	_UL (*res)(void);
	_UL (*val)(void);
	int (*comp)(_UL);
	int (*level)(int);
	int (*cmd)(timerCmds);
	INTFUNC user_func;
	void* user_parm;
	struct _timerHandle* next;
};
typedef struct _timerHandle timerHandle;

#define timerLevelValid(x) (x>=0 && x<=7)

static SEM_ID lock;
static timerHandle* freelist=NULL;
static void tick_func(void* v);

#ifdef MV167
/* ---------- pcc API ---------- */
static int pccInit(void);
static _UL pccRes(void);
static _UL pccValue1(void);
static _UL pccValue2(void);
static int pccComp1(_UL usec);
static int pccComp2(_UL usec);
static int pccLevel1(int level);
static int pccLevel2(int level);
static int pccCmd1(timerCmds cmd);
static int pccCmd2(timerCmds cmd);
#endif

#ifdef MV162
/* ---------- mcchip API -------- */
static int mcInit(void);
static _UL mcRes(void);
static _UL mcValue1(void);
static _UL mcValue2(void);
static _UL mcValue3(void);
static _UL mcValue4(void);
static int mcComp1(_UL usec);
static int mcComp2(_UL usec);
static int mcComp3(_UL usec);
static int mcComp4(_UL usec);
static int mcLevel1(int level);
static int mcLevel2(int level);
static int mcLevel3(int level);
static int mcLevel4(int level);
static int mcCmd1(timerCmds cmd);
static int mcCmd2(timerCmds cmd);
static int mcCmd3(timerCmds cmd);
static int mcCmd4(timerCmds cmd);
#endif

#if defined(MV162) || defined(MV167)
/* ---------- vmechip2 API --------- */
static int vmeInit(void);
static _UL vmeRes(void);
static _UL vmeValue1(void);
static _UL vmeValue2(void);
static int vmeComp1(_UL usec);
static int vmeComp2(_UL usec);
static int vmeLevel1(int level);
static int vmeLevel2(int level);
static int vmeCmd1(timerCmds cmd);
static int vmeCmd2(timerCmds cmd);
#endif

/* ---------- user functions --------- */
int timerSystemInit(void)
{
	if(freelist) return -1;
	lock=semBCreate(SEM_Q_PRIORITY,SEM_FULL);
#ifdef MV162
	if(mcInit()<0) return -1;
	if(vmeInit()<0) return -1;
#endif
#ifdef MV167
	if(pccInit()<0) return -1;
	if(vmeInit()<0) return -1;
#endif
	return 0;
}

void* timerAlloc(void) /* returns handle */
{
	timerHandle* rc;
	semTake(lock,WAIT_FOREVER);
	if(freelist)
	{
		rc=freelist;
		freelist=rc->next;
		rc->next=NULL;
	}
	else
		rc=NULL;
	semGive(lock);
	return (void*)rc;
}
void timerFree(void* handle)
{
	timerHandle* h=(timerHandle*)handle;
	if(h)
	{
		semTake(lock,WAIT_FOREVER);
		h->next=freelist;
		freelist=h;
		semGive(lock);
	}
}

int timerCmd(void* handle, timerCmds cmd)
{
	timerHandle* h=(timerHandle*)handle;
	return h?(*h->cmd)(cmd):-1;
}
unsigned long timerResolution(void* handle)
{
	timerHandle* h=(timerHandle*)handle;
	return h?(*h->res)():-1;
}
unsigned long timerCurrentValue(void* handle)
{
	timerHandle* h=(timerHandle*)handle;
	return h?(*h->val)():-1;
}
int timerCompareValue(void* handle,unsigned long usec)
{
	timerHandle* h=(timerHandle*)handle;
	return h?(*h->comp)(usec):-1;
}
int timerIntLevel(void* handle,int level)
{
	timerHandle* h=(timerHandle*)handle;
	return h?(*h->level)(level):-1;
}
int timerPeriodic(void* handle,unsigned long usec,int level,
	INTFUNC func,void* parm)
{
	timerHandle* h=(timerHandle*)handle;
	int rc=0;

	if(h)
	{
		rc|=(*h->cmd)(timerDisable);
		rc|=(*h->cmd)(timerClear);
		rc|=(*h->comp)(usec);
		rc|=(*h->level)(level);
		rc|=timerIntFunction(handle,func,parm);
		rc|=(*h->cmd)(timerIntClear);
		if(rc==0)
		{
			rc|=(*h->cmd)(timerIntEnable);
			rc|=(*h->cmd)(timerEnable);
		}
	}
	return rc;
}
int timerIntFunction(void* handle,INTFUNC func, void* parm)
{
	timerHandle* h=(timerHandle*)handle;
	int rc=0;
	if(h)
	{
		h->user_func=func;
		h->user_parm=parm;
	}
	else
		rc=-1;

	return rc;
}

/* -------------------- pcc functions -------------------- */
#ifdef MV167
static int pccInit(void)
{
	/*
		vxWorks uses one of the timers on the pcc as the tick timer
		so do not make this one available to the users.  It also makes the
		second one available to users with sysAuxClockConnect() so don't
		use this one either.  This leaves no timers available on the pcc
		using vxWorks.  Don't do anything here for now.
	*/
	return 0;
}
static _UL pccRes(void) { return 1000000; }
static _UL pccValue1(void) { return 0; }
static _UL pccValue2(void) { return 0; }
static int pccComp1(_UL usec) { return -1; }
static int pccComp2(_UL usec) { return -1; }
static int pccLevel1(int level) { return -1; }
static int pccLevel2(int level) { return -1; }
static int pccCmd1(timerCmds cmd) { return -1; }
static int pccCmd2(timerCmds cmd) { return -1; }
#endif
/* -------------------- mcchip2 functions -------------------- */
#ifdef MV162
/* MCCHIP_BASE = 0xfff42000 */

static int mcInit(void)
{
	/*
		First two timers are used by vxWorks, see note in pccInit().
	*/
	timerHandle* h;
	int vec,rc=0;
#if 0
	/* do not use these in version 1 */
	/* first available timer (3) */
	mcCmd3(timerDisable);
	h=(timerHandle*)malloc(sizeof(timerHandle));
	h->user_func=NULL;
	h->user_parm=NULL;
	h->res=mcRes;
	h->val=mcValue3;
	h->comp=mcComp3;
	h->cmd=mcCmd3;
	h->level=mcLevel3;
	vec=(*((_VUC*)(0xfff42003))&0xf0)+0x04; /* VBR */
	if(intConnect(INUM_TO_IVEC(vec),tick_func,(int)h)!=OK) rc=-1;
	h->next=freelist;
	freelist=h;

	/* second available timer (4) */
	mcCmd4(timerDisable);
	h=(timerHandle*)malloc(sizeof(timerHandle));
	h->user_func=NULL;
	h->user_parm=NULL;
	h->res=mcRes;
	h->val=mcValue4;
	h->comp=mcComp4;
	h->cmd=mcCmd4;
	h->level=mcLevel4;
	vec=(*((_VUC*)(0xfff42003))&0xf0)+0x03; /* VBR */
	if(intConnect(INUM_TO_IVEC(vec),tick_func,(int)h)!=OK) rc=-1;
	h->next=freelist;
	freelist=h;
#endif
	return rc;
}

static _UL mcValue1(void) { return 0; }
static _UL mcValue2(void) { return 0; }
static int mcComp1(_UL usec) { return -1; }
static int mcComp2(_UL usec) { return -1; }
static int mcLevel1(int level) { return -1; }
static int mcLevel2(int level) { return -1; }
static int mcCmd1(timerCmds cmd) { return -1; }
static int mcCmd2(timerCmds cmd) { return -1; }

static _UL mcRes(void) { return 1000000; }
static _UL mcValue3(void) { return *((_VUL*)(0xfff42034)); }
static _UL mcValue4(void) { return *((_VUL*)(0xfff4203c)); }
static int mcComp3(_UL usec) { *((_VUL*)(0xfff42030))=usec; return 0; }
static int mcComp4(_UL usec) { *((_VUL*)(0xfff42038))=usec; return 0; }

static int mcLevel3(int level)
{
	_UC x=(_UC)level;
	if(!timerLevelValid(level)) return -1;
	*((_VUC*)(0xfff42019))&=0xf8;	/* intctrl3 */
	*((_VUC*)(0xfff42019))|=x;		/* intctrl3 */
	return 0;
}
static int mcLevel4(int level)
{
	_UC x=(_UC)level;
	if(!timerLevelValid(level)) return -1;
	*((_VUC*)(0xfff42018))&=0xf8;	/* intctrl4 */
	*((_VUC*)(0xfff42018))|=x;		/* intctrl4 */
	return 0;
}
static int mcCmd3(timerCmds cmd)
{
	switch(cmd)
	{
	case timerClear:		*((_VUC*)(0xfff42034))=0;     break; /* current3 */
	case timerDisable:		*((_VUC*)(0xfff4201f))&=0x02; break; /* control3 */
	case timerEnable:		*((_VUC*)(0xfff4201f))|=0x01; break; /* control3 */
	case timerResetMode:	*((_VUC*)(0xfff4201f))|=0x02; break; /* control3 */
	case timerCycleMode:	*((_VUC*)(0xfff4201f))&=0xfd; break; /* control3 */
	/* !!!!!check the interrupt enable/disable registers!!!!!*/
	case timerIntEnable:	*((_VUC*)(0xfff42019))|=0x10; break; /* intctrl3 */
	case timerIntDisable:	*((_VUC*)(0xfff42019))|=0x07; break; /* intctrl3 */
	/* !!!!!check the interrupt enable/disable registers!!!!!*/
	case timerIntClear:		*((_VUC*)(0xfff42019))|=0x08; break; /* intctrl3 */
	default: break;
	}
	return 0;
}
static int mcCmd4(timerCmds cmd)
{
	switch(cmd)
	{
	case timerClear:		*((_VUC*)(0xfff4203c))=0;     break; /* current4 */
	case timerDisable:		*((_VUC*)(0xfff4201e))&=0x02; break; /* control4 */
	case timerEnable:		*((_VUC*)(0xfff4201e))|=0x01; break; /* control4 */
	case timerResetMode:	*((_VUC*)(0xfff4201e))|=0x02; break; /* control4 */
	case timerCycleMode:	*((_VUC*)(0xfff4201e))&=0xfd; break; /* control4 */
	/* !!!!!check the interrupt enable/disable registers!!!!!*/
	case timerIntEnable:	*((_VUC*)(0xfff42018))|=0x10; break; /* intctrl4 */
	case timerIntDisable:	*((_VUC*)(0xfff42018))|=0x07; break; /* intctrl4 */
	/* !!!!!check the interrupt enable/disable registers!!!!!*/
	case timerIntClear:		*((_VUC*)(0xfff42018))|=0x08; break; /* intctrl4 */
	default: break;
	}
	return 0;
}
#endif
/* -------------------- vmechip2 functions -------------------- */
#if defined(MV162) || defined(MV167)
static int vmeInit(void)
{
	timerHandle* h;
	int vec,rc=0;

	/* first available timer (1) */
	vmeCmd1(timerDisable);
	h=(timerHandle*)malloc(sizeof(timerHandle));
	h->user_func=NULL;
	h->user_parm=NULL;
	h->res=vmeRes;
	h->val=vmeValue1;
	h->comp=vmeComp1;
	h->cmd=vmeCmd1;
	h->level=vmeLevel1;
	vec=(*((_VUC*)(0xfff4008b))&0xf0)|0x08; /* VBR */
	if(intConnect(INUM_TO_IVEC(vec),tick_func,(int)h)!=OK) rc=-1;
	h->next=freelist;
	freelist=h;

	/* second available timer (2) */
	vmeCmd2(timerDisable);
	h=(timerHandle*)malloc(sizeof(timerHandle));
	h->user_func=NULL;
	h->user_parm=NULL;
	h->res=vmeRes;
	h->val=vmeValue2;
	h->comp=vmeComp2;
	h->cmd=vmeCmd2;
	h->level=vmeLevel2;
	vec=(*((_VUC*)(0xfff4008b))&0xf0)|0x09; /* VBR */
	if(intConnect(INUM_TO_IVEC(vec),tick_func,(int)h)!=OK) rc=-1;
	h->next=freelist;
	freelist=h;

	return rc;
}

static _UL vmeRes(void) { return 1000000; }
static _UL vmeValue1(void) { return *((_VUL*)(0xfff40054)); }
static _UL vmeValue2(void) { return *((_VUL*)(0xfff4005c)); }
static int vmeComp1(_UL usec) { *((_VUL*)(0xfff40050))=usec; return 0; }
static int vmeComp2(_UL usec) { *((_VUL*)(0xfff40058))=usec; return 0; }

static int vmeLevel1(int level)
{
	_UC x=(_UC)level;
	if(!timerLevelValid(level)) return -1;
	*((_VUC*)(0xfff40078))&=0xf8;	/* int level */
	*((_VUC*)(0xfff40078))|=x;		/* int level */
	return 0;
}
static int vmeLevel2(int level)
{
	_UC x=(_UC)level;
	if(!timerLevelValid(level)) return -1;
	*((_VUC*)(0xfff40078))&=0x8f;	/* int level */
	*((_VUC*)(0xfff40078))|=(x<<4);	/* int level */
	return 0;
}

static int vmeCmd1(timerCmds cmd)
{
	switch(cmd)
	{
	case timerClear:		*((_VUC*)(0xfff40054))=0;     break; /* current1 */
	case timerDisable:		*((_VUC*)(0xfff40060))&=0x02; break; /* control1 */
	case timerEnable:		*((_VUC*)(0xfff40060))|=0x01; break; /* control1 */
	case timerResetMode:	*((_VUC*)(0xfff40060))|=0x02; break; /* control1 */
	case timerCycleMode:	*((_VUC*)(0xfff40060))&=0xfd; break; /* control1 */
	case timerIntEnable:	*((_VUC*)(0xfff4006f))|=0x01; break; /* intctrl1 */
	case timerIntDisable:	*((_VUC*)(0xfff4006f))&=0xfe; break; /* intctrl1 */
	case timerIntClear:		*((_VUC*)(0xfff40077))|=0x01; break; /* intclear1 */
	default: break;
	}
	return 0;
}
static int vmeCmd2(timerCmds cmd)
{
	switch(cmd)
	{
	case timerClear:		*((_VUC*)(0xfff4005c))=0;     break; /* current2 */
	case timerDisable:		*((_VUC*)(0xfff40061))&=0x02; break; /* control2 */
	case timerEnable:		*((_VUC*)(0xfff40061))|=0x01; break; /* control2 */
	case timerResetMode:	*((_VUC*)(0xfff40061))|=0x02; break; /* control2 */
	case timerCycleMode:	*((_VUC*)(0xfff40061))&=0xfd; break; /* control2 */
	case timerIntEnable:	*((_VUC*)(0xfff4006f))|=0x02; break; /* intctrl2 */
	case timerIntDisable:	*((_VUC*)(0xfff4006f))&=0xfd; break; /* intctrl2 */
	case timerIntClear:		*((_VUC*)(0xfff40077))|=0x02; break; /* intclear2 */
	default: break;
	}
	return 0;
}
#endif

/* ------------------------ shared functions ------------------------ */

static void tick_func(void* v)
{
	timerHandle* h = (timerHandle*)v;
	(*h->cmd)(timerIntClear);
	if(h->user_func) (*h->user_func)(h->user_parm);
}

