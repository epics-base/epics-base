/* $Id$ */
#ifndef EPICS_EGDEFS_H
#define EPICS_EGDEFS_H

typedef long (*EG_PROC_FUN)(struct egRecord *prec);
typedef long (*EG_INIT_REC_FUN)(struct egRecord *prec);
typedef long (*EG_INIT_DEV_FUN)(int pass);
typedef struct
{
  long	number;
  DEVSUPFUN       	report;
  EG_INIT_DEV_FUN       init;
  EG_INIT_REC_FUN       initRec;
  DEVSUPFUN       	get_ioint_info;
  EG_PROC_FUN		proc;
} EgDsetStruct;

#define EG_SEQ_RAM_SIZE                 (1024*32)
 
#define EG_SEQ_RAM_EVENT_NULL                   0
#define EG_SEQ_RAM_EVENT_END                    127
#define EG_SEQ_RAM_EVENT_FREEZE                 126
#define EG_SEQ_RAM_EVENT_RESET_TIME             125
#define EG_SEQ_RAM_EVENT_TICK_TIME              124
#define EG_SEQ_RAM_EVENT_RESET_PRESCALE         123
#define EG_SEQ_RAM_EVENT_HEARTBEAT              122

#define REC_EGEVENT_UNIT_TICKS  	(0)
#define REC_EGEVENT_UNIT_FORTNIGHTS	(1)
#define REC_EGEVENT_UNIT_WEEKS		(2)
#define REC_EGEVENT_UNIT_DAYS		(3)
#define REC_EGEVENT_UNIT_HOURS		(4)
#define REC_EGEVENT_UNIT_MINUITES	(5)
#define REC_EGEVENT_UNIT_SECONDS	(6)
#define REC_EGEVENT_UNIT_MILLISECONDS	(7)
#define REC_EGEVENT_UNIT_MICROSECONDS	(8)
#define REC_EGEVENT_UNIT_NANOSECONDS	(9)

#endif
