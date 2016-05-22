/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
	these are used in the pmt (prompt) field of the record support
	ascii files.  They represent field groupings for dct tools
*/

#ifndef __gui_group_h__
#define __gui_group_h__

#define  GUI_COMMON 	1
#define  GUI_ALARMS	2
#define  GUI_BITS1	3
#define  GUI_BITS2	4
#define  GUI_CALC	5
#define  GUI_CLOCK	6
#define  GUI_COMPRESS	7
#define  GUI_CONVERT	8
#define  GUI_DISPLAY	9
#define  GUI_HIST	10
#define  GUI_INPUTS	11
#define  GUI_LINKS	12
#define  GUI_MBB	13
#define  GUI_MOTOR	14
#define  GUI_OUTPUT	15
#define  GUI_PID	16
#define  GUI_PULSE	17
#define  GUI_SELECT	18
#define  GUI_SEQ1	19
#define  GUI_SEQ2	20
#define  GUI_SEQ3	21
#define  GUI_SUB	22
#define  GUI_TIMER	23
#define  GUI_WAVE	24
#define  GUI_SCAN	25
#define GUI_NTYPES 25

typedef struct mapguiGroup{
	char	*strvalue;
	int	value;
}mapguiGroup;

#ifndef GUIGROUPS_GBLSOURCE
extern mapguiGroup pamapguiGroup[];
#else
mapguiGroup pamapguiGroup[GUI_NTYPES] = {
	{"GUI_COMMON",GUI_COMMON},
	{"GUI_ALARMS",GUI_ALARMS},
	{"GUI_BITS1",GUI_BITS1},
	{"GUI_BITS2",GUI_BITS2},
	{"GUI_CALC",GUI_CALC},
	{"GUI_CLOCK",GUI_CLOCK},
	{"GUI_COMPRESS",GUI_COMPRESS},
	{"GUI_CONVERT",GUI_CONVERT},
	{"GUI_DISPLAY",GUI_DISPLAY},
	{"GUI_HIST",GUI_HIST},
	{"GUI_INPUTS",GUI_INPUTS},
	{"GUI_LINKS",GUI_LINKS},
	{"GUI_MBB",GUI_MBB},
	{"GUI_MOTOR",GUI_MOTOR},
	{"GUI_OUTPUT",GUI_OUTPUT},
	{"GUI_PID",GUI_PID},
	{"GUI_PULSE",GUI_PULSE},
	{"GUI_SELECT",GUI_SELECT},
	{"GUI_SEQ1",GUI_SEQ1},
	{"GUI_SEQ2",GUI_SEQ2},
	{"GUI_SEQ3",GUI_SEQ3},
	{"GUI_SUB",GUI_SUB},
	{"GUI_TIMER",GUI_TIMER},
	{"GUI_WAVE",GUI_WAVE},
	{"GUI_SCAN",GUI_SCAN}
};
#endif /*GUIGROUPS_GBLSOURCE*/

#endif /*__gui_group_h__*/
