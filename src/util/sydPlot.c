/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	12-04-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991-92, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 *  .01 12-04-90 rac	initial version
 *  .02 06-30-91 rac	installed in SCCS
 *  .03 09-06-91 rac	change pprAreaErase to pprRegionErase; add
 *		 	documentation
 *  .04 10-01-91 rac	properly handle channels which aren't connected yet
 *  .05 10-20-91 rac	avoid an abort on printing a plot to
 *		 	PostScript; for non-user-window plotting,
 *		 	defer the pprWinIsMono call until after
 *		 	the window is mapped
 *  .06 12-03-91 rac	add some intelligence to auto ranging
 *  .07 01-23-92 rac	don't take log10 of zero value
 *  .08 03-31-92 rac	fix plotting of filled channels; set time axis
 *			scaling based on firstData to lastData rather
 *			than 0 to sampleCount; handle plotting mean and
 *			standard deviation; handle plotting restricted set
 *			of data; better annotate time axes; support a time
 *			cursor on a plot; add sydPlotChan_event to determine
 *			which channel a mouse event applies to;
 *  .09 08-27-92 rac	add user-specified range for AutoRange; add more
 *			smarts to AutoRange; have only one X-axis annotation;
 *			add strip chart; discontinue use of special malloc;
 *
 * make options
 *	-DXWINDOWS	makes a version for X11
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code
 */
/*+/mod***********************************************************************
* TITLE	sydPlot.c - plotting for synchronous data
*
* DESCRIPTION
*	These routines provide high-level plotting capability in conjunction
*	with the sydSubr.c routines.  The data acquired by the sydSubr
*	routines are accepted directly by these plotting routines.
*
*	These routines support plotting in either batch or incremental mode.
*	In batch mode, all the samples exist at the time of plotting; for
*	incremental mode, only part (or none) of the samples exist when
*	plotting starts, and additional samples are to be plotted as they
*	arrive.
*
*	Some windowing events, such as expose and resize, are transparently
*	handled by these routines.  Hard copy of plots to a PostScript
*	printer is easily available.
*
* QUICK REFERENCE
*
* #include <sydDefs.h>
* #include <sydPlotDefs.h>
*
*         void  sydPlotAxisAutoRange(pSlave, minVal, maxVal)
*         long  sydPlotAxisSetAttr(pSlave, attr, value, pArg)
*			attr = SYD_PLATTR_{GC}
* SYD_PL_SLAVE *sydPlotChanAdd(pMstr, pSChan)
* SYD_PL_SLAVE *sydPlotChan_event(pMstr, x, y)
*         long  sydPlotDone(pMstr, quitFlag)
*         long  sydPlotEraseSamples(pMstr)
*         long  sydPlotInit(pMstr, pSspec, winType, dispName, winTitle,
*							fullInit)
*         long  sydPlotInitUW(pMstr, pSspec, pDisp, window, gc)
*         long  sydPlotSamples(pMstr, begin, end, incrFlag)
*         long  sydPlotSetAttr(pMstr, attr, value, pArg)
*                  attr = SYD_PLATTR_{FG1,FG2,INCR,LINE,MARK,MONO,POINT,SHOW,
*                                     UNDER,WRAP}
*         long  sydPlotSetTitles(pMstr, top, left, bottom, right) 
*         long  sydPlotTimeCursor(pMstr, pStamp, pSlave)
*         long  sydPlotWinLoop(pMstr)
*         long  sydPlotWinReplot(pMstr)
*
* DESCRIPTION, continued
*	These routines generally work with the concepts of `plot master'
*	and `plot slave'.  The plot master structure roughly corresponds
*	to a plotting surface (i.e., X11 window or PostScript sheet) and
*	contains most of the information necessary to perform plotting.
*	The plot master contains a list of plot slave structures, each of
*	which is analogous to a data channel.  A plot slave structure
*	contains channel specific information, including data.  Time
*	stamp information is provided via the plot master, through the
*	use of its connection to synchronous sample structures.
*
* BUGS
* o	sydPlotInitUW doesn't support SunView; some other routines have
*	questionable support
* o	there is no counterpart for sydTimeCursor for other than time-based
*	plots
*   
*-***************************************************************************/
#include <genDefs.h>
#include <sydDefs.h>
#include <tsDefs.h>
#ifdef XWINDOWS
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#endif
#define SYD_PLOT_PRIVATE
#include <sydPlotDefs.h>

#ifdef vxWorks
/*----------------------------------------------------------------------------
*    includes and defines for VxWorks compile
*---------------------------------------------------------------------------*/
#   include <vxWorks.h>
#   include <stdioLib.h>
#   include <strLib.h>
#   include <ctype.h>
#else
/*----------------------------------------------------------------------------
*    includes and defines for Sun compile
*---------------------------------------------------------------------------*/
#   include <stdio.h>
#   include <string.h>
#   include <strings.h>
#   include <ctype.h>
#   include <math.h>
#endif

/*-----------------------------------------------------------------------------
*    prototypes
*----------------------------------------------------------------------------*/
void sydPlot_SmithPlot();
void sydPlot_SmithGrid();
void sydPlot_SmithSamples();
void sydPlot_SmithStats();
void sydPlot_StripYPlot();
void sydPlot_StripYGrid();
void sydPlot_StripYRedisplay();
void sydPlot_StripYSamples();
void sydPlot_StripYYPlot();
void sydPlot_StripYYGrid();
void sydPlot_StripYYRedisplay();
void sydPlot_StripYYSamples();
void sydPlot_TYPlot();
void sydPlot_TYGrid();
void sydPlot_TYSamples();
void sydPlot_TYStats();
void sydPlot_TYYPlot();
void sydPlot_TYYGrid();
void sydPlot_XYPlot();
void sydPlot_XYGrid();
void sydPlot_XYSamples();
void sydPlot_XYStats();
void sydPlot_XYarray();
void sydPlot_XYYPlot();
void sydPlot_XYYGrid();
void sydPlot_YPlot();
void sydPlot_YGrid();
void sydPlot_YSamples();
void sydPlot_YStats();
void sydPlot_Yarray();
void sydPlot_YYPlot();
void sydPlot_YYGrid();

/*+/internal******************************************************************
* NAME	sydAnnotVal - generate an array of value axis annotations
*
*-*/
static void
sydAnnotVal(pSlave, pAnnot, annot)
SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
char	*pAnnot[];
char	annot[20][28];
{
    int		i;
    double	val, tick;

    assert(pSlave->nInt < 20);

    tick = (pSlave->extentVal - pSlave->originVal) / pSlave->nInt;
    
    for (i=0, val=pSlave->originVal; i<=pSlave->nInt; i++, val+=tick) {
	if (i == pSlave->nInt)
	    val = pSlave->extentVal;
	cvtDblToTxt(annot[i], 6, val, pSlave->decPl);
	pAnnot[i] = annot[i];
    }
}


/*+/internal******************************************************************
* NAME	logGetOK - get log of minimum allowable tick size
*
* DESCRIPTION
*	Find the minimum tick size based on a field width of 6 using
*	cvtDblToTxt using  for printing.
*
*-*/
static int logGetOK(val)
double	val;
{
    int		logVal, logTickVal;
    logVal = val != 0. ? nint(floor(log10(fabs(val)))) : -10;
    if (val >= 0.) {
	if (1 <= logVal && logVal <= 4)		logTickVal = logVal - 4;
	else if (logVal == 5)			logTickVal = 0;
	else if (logVal == -1)			logTickVal = -4;
	else					logTickVal = logVal - 2;
    }
    else {
	if (1 <= logVal && logVal <= 3)		logTickVal = logVal - 3;
	else if (logVal == 4)			logTickVal = 0;
	else if (logVal == 0)			logTickVal = -3;
	else if (logVal == -1)			logTickVal = -4;
	else					logTickVal = logVal - 1;
    }
    return logTickVal;
}

/*+/internal******************************************************************
* NAME	moveVal - move a value to it's tick mark
*
*-*/
static double moveVal(val, tick, minFlag)
double	val, tick;
int	minFlag;
{
    double	moved;
    int		awayFrom=0;
    double	temp;

    if (val < 0 && minFlag)		awayFrom = 1;
    else if (val > 0 && !minFlag)	awayFrom = 1;
    if (val > -1. * tick && val < tick && awayFrom == 0)
	return 0.;
    temp = fmod(val, tick);
    moved = val - temp;
    if (awayFrom && temp != 0.) {
	if (val <= 0.) { if (moved > val) moved -= tick; }
	else { if (moved < val) moved += tick; }
    }
    return moved;
}

/*+/internal******************************************************************
* NAME	autoRange - compute auto range for an axis
*
*-*/
static void
autoRange(pOrigin, pExtent, pNTicks, pTick, pDecPl)
double	*pOrigin, *pExtent, *pTick;
int	*pNTicks, *pDecPl;
{
    double	origin, extent;
    double	min, min1, max, max1, logD, tick;
    int		logTick, logTickMin, logTickMax, logTickOK;
    double	maxVal;
    int		nTicks, decPl;

#define MIN(a,b) (a<=b?a:b)
#define MAX(a,b) (a<=b?b:a)

    min = MIN(*pOrigin, *pExtent);
    max = MAX(*pOrigin, *pExtent);
    if (min == max)
	min -= .001, max += .001;
    min1 = min; max1 = max;
/*-----------------------------------------------------------------------------
*	First, determine tick size and number of intervals without regard
*	to the number of divisions which would result.  Round the min
*	and max to tick boundaries.
*----------------------------------------------------------------------------*/
    logD = floor(log10(max-min));
    tick = exp10(logD);
    min = moveVal(min, tick, 1);
    max = moveVal(max, tick, 0);

    nTicks = nint((max - min) / tick);
    if (nTicks == 10) nTicks = 5, tick *= 2.;
    else if (nTicks == 1) nTicks = 5, tick /= 5.;
    else if (nTicks == 2) nTicks = 4, tick /= 2.;
    else if (nTicks == 8) nTicks = 4, tick *= 2.;
    else if (nTicks == 9) nTicks = 3, tick *= 3.;
/*-----------------------------------------------------------------------------
*	Next, make sure the tick size is consistent with printing in a
*	six character field with 3 significant digits.  For example, if the
*	max is 1000000, it will print as 100E4, allowing a minimum tick
*	size of 10000.
*----------------------------------------------------------------------------*/
    logTick = nint(floor(log10(tick)));
    logTickMax = logGetOK(max);
    logTickMin = logGetOK(min);
    logTickOK = MAX(logTickMin, logTickMax);
    if (logTickOK > logTick) {
	tick = exp10((double)logTickOK);
	min = moveVal(min1, tick, 1);
	max = moveVal(max1, tick, 0);
	nTicks = 1;
    }
/*-----------------------------------------------------------------------------
*	Now, figure how many decimal places can be printed, based on the
*	biggest endpoint, assuming a field width of 6 characters.
*----------------------------------------------------------------------------*/
    maxVal = fabs(max) > fabs(min) ? max : min;
    if (maxVal >= 10000. || maxVal <= -1000.)		decPl = 0;
    else if (maxVal >= 1000. || maxVal <= -100.)	decPl = 1;
    else if (maxVal >= 100. || maxVal <= -10.)		decPl = 2;
    else if (maxVal >= 10. || maxVal <= -1.)		decPl = 3;
    else						decPl = 4;
    if (*pOrigin <= *pExtent) {
	*pTick = tick;
	*pOrigin = min;
	*pExtent = max;
    }
    else {
	*pOrigin = max;
	*pExtent = min;
	*pTick = 0. - tick;
    }
    *pNTicks = nTicks;
    *pDecPl = decPl;
}

/*+/subr**********************************************************************
* NAME	sydPlotAxisAutoRange - set axis ends to min and max data values
*
* DESCRIPTION
*	Sets the endpoints of the axis for the plot slave structure to
*	`nice' values based on the minimum and maximum of the data for the
*	slave.  The number of intervals to divide the axis is also set.
*
*	If the minimum and maximum values specified in the call aren't equal,
*	they are used for scaling, rather than the min and max of the data.
*
* RETURNS
*	void
*
* SEE ALSO
*
* EXAMPLE
*
*	
*-*/
void
sydPlotAxisAutoRange(pSlave, minVal, maxVal)
SYD_PL_SLAVE *pSlave;	/* I pointer to plot slave structure */
double	minVal;		/* I minimum value, for setting scaling */
double	maxVal;		/* I maximum value, for setting scaling */
{
    SYD_CHAN	*pSChan=pSlave->pSChan;
    double	origin=pSlave->pSChan->restrictMinDataVal;
    double	extent=pSlave->pSChan->restrictMaxDataVal;
    int		nInt, decPl;
    double	tick;

    if (pSChan->dbrType == DBR_TIME_ENUM)
	return;
    if (minVal != maxVal) {
	origin = minVal;
	extent = maxVal;
    }

    autoRange(&origin, &extent, &nInt, &tick, &decPl);
    pSlave->originVal = origin;
    pSlave->extentVal = extent;
    pSlave->nInt = nInt;
    pSlave->decPl = decPl;
}

/*+/subr**********************************************************************
* NAME	sydPlotAxisSetAttr - set plot axis attributes
*
* DESCRIPTION
*	Setting an attribute doesn't automatically reset other related
*	attributes.
*
*	Declare a slave to be used as the x-axis channel when x vs. y
*	plotting is done:
*	    sydPlotAxisSetAttr(pSlave, SYD_PLATTR_XCHAN, {0,1}, NULL)
*
*	Set the background and/or foreground pixel values for X11 for
*	a slave:
*	    sydPlotAxisSetAttr(pSlave, SYD_PLATTR_BG, 0, pBgPixelValue)
*	    sydPlotAxisSetAttr(pSlave, SYD_PLATTR_FG, 0, pFgPixelValue)
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydPlotAxisSetAttr(pSlave, attr, value, pArg)
SYD_PL_SLAVE *pSlave;	/* I pointer to plot slave structure */
SYD_PLATTR attr;	/* I attribute selector--one of SYD_PLATTR_xxx */
int	value;		/* I value for attribute */
void	*pArg;		/* I pointer for value for attribute */
{
    if      (attr == SYD_PLATTR_XCHAN)  pSlave->xChan = value;
#ifdef XWINDOWS
    else if (attr == SYD_PLATTR_BG)
	pSlave->bg = *(unsigned long *)pArg;
    else if (attr == SYD_PLATTR_FG)
	pSlave->fg = *(unsigned long *)pArg;
#endif
    else assertAlways(0);

    return S_syd_OK;
}

/*+/internal******************************************************************
* NAME	sydPlotAxisSetup - set up axis information for a channel
*
* DESCRIPTION
*	Set up axis information.
*	o   the axis endpoints are set to LOPR and HOPR.  If LOPR==HOPR,
*	    then minimum and maximum data values are used for endpoints.
*	    If min==max, then arbitrary values are used.
*	o   number of major intervals is set to 5
*
* RETURNS
*	void
*
* BUGS
* o	in pathological cases, the setup is overly arbitrary
* o	number of intervals is fixed at 5
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
void
sydPlotAxisSetup(pSlave)
SYD_PL_SLAVE *pSlave;	/* I pointer to plot slave structure */
{
    SYD_CHAN	*pSChan=pSlave->pSChan;
    double	originVal, extentVal;
    int		nInt=5;

    if (pSChan->dbrType == DBR_TIME_FLOAT) {
	originVal = pSChan->grBuf.gfltval.lower_disp_limit;
	extentVal = pSChan->grBuf.gfltval.upper_disp_limit;
    }
    else if (pSChan->dbrType == DBR_TIME_SHORT) {
	originVal = pSChan->grBuf.gshrtval.lower_disp_limit;
	extentVal = pSChan->grBuf.gshrtval.upper_disp_limit;
    }
    else if (pSChan->dbrType == DBR_TIME_DOUBLE) {
	originVal = pSChan->grBuf.gdblval.lower_disp_limit;
	extentVal = pSChan->grBuf.gdblval.upper_disp_limit;
    }
    else if (pSChan->dbrType == DBR_TIME_LONG) {
	originVal = pSChan->grBuf.glngval.lower_disp_limit;
	extentVal = pSChan->grBuf.glngval.upper_disp_limit;
    }
    else if (pSChan->dbrType == DBR_TIME_CHAR) {
	originVal = pSChan->grBuf.gchrval.lower_disp_limit;
	extentVal = pSChan->grBuf.gchrval.upper_disp_limit;
    }
    else if (pSChan->dbrType == DBR_TIME_ENUM) {
	nInt = pSChan->grBuf.genmval.no_str-1;
	originVal = 0;
	extentVal = nInt;
	if (originVal >= extentVal) {
	    if (nInt < 0)
		(void)strcpy(pSChan->grBuf.genmval.strs[0], " ");
	    extentVal = nInt = 1;
	    pSChan->grBuf.genmval.no_str = 2;
	    (void)strcpy(pSChan->grBuf.genmval.strs[1], " ");
	}
    }
    if (originVal == extentVal) {
	originVal = pSlave->pSChan->minDataVal;
	extentVal = pSlave->pSChan->maxDataVal;
    }
    if (originVal == extentVal) {
	if (originVal == 0.)
	    extentVal = 10.;
	else if (originVal < 0.)
	    extentVal = 0;
	else
	    originVal = 0.;
    }
    pSlave->originVal = originVal;
    pSlave->extentVal = extentVal;
    pSlave->nInt = nInt;
    pSlave->nSubInt = 0;
    pSlave->decPl = 3;
}

/*+/subr**********************************************************************
* NAME	sydPlotChanAdd - add a plot slave
*
* DESCRIPTION
*	Adds a slave to a master plot structure.  Some of the items needed
*	for actual plotting are set up by this routine:
*
*	o   endpoints for plotting.  If HOPR and LOPR are present for the
*	    channel, they are used as the plotting endpoints.  If they
*	    aren't present (or if they are equal), then some relatively
*	    arbitrary endpoints are picked.  For DBF_ENUM channels, the
*	    endpoints are determined by the states.
*	o   number of major tick intervals.  For DBF_ENUM channels, the
*	    number of states determines the number of intervals.
*	o   a default plot mark to be used for mark plotting
*	o   a default line key to be used for monochrome plotting.  This
*	    line key also establishes a default color to be used for
*	    color plotting; the default color can be overridden using
*	    sydPlotAxisSetAttr.
*
* RETURNS
*	SYD_PL_SLAVE *, or
*	NULL
*
* BUGS
* o	the scheme for establishing mark and key numbers won't work if
*	deleting and re-adding plot channels is allowed
*
*-*/
SYD_PL_SLAVE *
sydPlotChanAdd(pMstr, pSChan)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master */
SYD_CHAN *pSChan;	/* I pointer to synchronous data channel structure,
				as from sydChanOpen */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to slave structure */
    int		i;

    assert(pMstr != NULL);

    if (dbr_type_is_STRING(pSChan->dbrType)) {
	(void)printf("sydPlotChanAdd: can't plot DBF_STRING values\n");
	return NULL;
    }
    if ((pSlave = (SYD_PL_SLAVE *)malloc(sizeof(SYD_PL_SLAVE))) == NULL) {
	(void)printf("sydPlotChanAdd: can't get memory\n");
	return NULL;
    }

    pSlave->pSChan = pSChan;
    pMstr->nSlaves++;
    pSlave->markNum = pMstr->nSlaves - 1;
    pSlave->lineKey = pMstr->nSlaves;
    pSlave->xChan = 0;
    pSlave->pArea = NULL;
    pSlave->fg = pSlave->bg = 0;
    pSlave->first = 1;
    pSlave->xFracLeft = pSlave->xFracRight = 0.;
    pSlave->yFracBot = pSlave->yFracTop = 0.;
    pSlave->annotXFL = pSlave->annotXFR = 0.;
    pSlave->annotYFB = pSlave->annotYFT = 0.;

    DoubleListAppend(pSlave, pMstr->pHead, pMstr->pTail);
    sydPlotAxisSetup(pSlave);
    if (dbr_type_is_ENUM(pSChan->dbrType)) {
	for (i=0; i<=pSlave->extentVal; i++)
	    pSlave->pAnnot[i] = pSChan->grBuf.genmval.strs[i];
	pSlave->ppAnnot = pSlave->pAnnot;
    }
    else
	pSlave->ppAnnot = NULL;

    return pSlave;
}

/*+/subr**********************************************************************
* NAME	sydPlotChan_event - find the plot slave corresponding to an event
*
* DESCRIPTION
*	The x and y position for the event are used to find the corresponding
*	plot slave.  This search examines the bounding box for the grid and
*	the annotation area.
*
*	For a shared grid plot, the result will be ambiguous.
*
* RETURNS
*	SYD_PL_SLAVE *, or
*	NULL
*
*-*/
SYD_PL_SLAVE *
sydPlotChan_event(pMstr, x, y)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master */
int	x, y;		/* I the x and y position from the event */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to slave structure */
    int		i;
    double	xFrac, yFrac;

    assert(pMstr != NULL);

    xFrac = PprPixXToXFrac(pMstr->pWin, x);
    yFrac = PprPixYToYFrac(pMstr->pWin, y);
    for (pSlave=pMstr->pHead; pSlave!=NULL; pSlave=pSlave->pNext) {
	if (xFrac >= pSlave->xFracLeft && xFrac <= pSlave->xFracRight &&
		yFrac >= pSlave->yFracBot && yFrac <= pSlave->yFracTop)
	    return pSlave;
	if (xFrac >= pSlave->annotXFL && xFrac <= pSlave->annotXFR &&
		yFrac >= pSlave->annotYFB && yFrac <= pSlave->annotYFT)
	    return pSlave;
    }
    return NULL;
}

/*+/subr**********************************************************************
* NAME	sydPlotDone - plotting rundown
*
* DESCRIPTION
*	Wrap up processing for a plot.  Each slave is closed.  The present
*	size and position of the plot window are saved in the plot master
*	structure.
*
* RETURNS
*	S_syd_OK
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydPlotDone(pMstr, quitFlag)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	quitFlag;	/* I use 0 for replot, 1 for total rundown */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    SYD_PL_SLAVE *pSlave1;	/* pointer to individual slave struct */

    assert(pMstr != NULL);

    if (quitFlag) {
	pSlave = pMstr->pHead;
	while (pSlave != NULL) {
	    pMstr->nSlaves--;
	    DoubleListRemove(pSlave, pMstr->pHead, pMstr->pTail);
	    if (pSlave->pArea != NULL)
		pprAreaClose(pSlave->pArea);
	    pSlave1 = pSlave;
	    pSlave = pSlave->pNext;
	    free((char *)pSlave1);
	}
	pprWinInfo(pMstr->pWin, &pMstr->x, &pMstr->y,
						&pMstr->width, &pMstr->height);
	pprWinClose(pMstr->pWin);
    }

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydPlotEraseSamples - erase samples from the screen
*
* DESCRIPTION
*	Erase the plot areas for the plot master.
*
* RETURNS
*	S_syd_OK
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydPlotEraseSamples(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */

    assert(pMstr != NULL);

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
	pSlave->first = 1;
	if (pSlave->pArea != NULL) {
	    pprRegionErase(pSlave->pArea, 1., 1., -1., -1.);
	}
	pSlave = pSlave->pNext;
    }

    return S_syd_OK;
}

static void sydPlotInit_common();
/*+/subr**********************************************************************
* NAME	sydPlotInit - plotting initialization
*
* DESCRIPTION
*	Initialize for plotting, using an automatically created window:
*	o   the window is created
*	o   if full initialization is requested, then the default window
*	    size and position are used; otherwise, the size and position
*	    in the plot master (as saved by sydPlotDone) are used.
*
*	This routine doesn't perform any plotting--sydPlotWinLoop must
*	be called to do the actual plotting.
*
*	The type of plotting which is done depends both on the window type
*	specified in the call to this routine and on the way that the plot
*	master is set up at the time of the call to sydPlotWinLoop.
*
* RETURNS
*	S_syd_OK, or
*	S_syd_ERROR if initialization can't be completed
*
* BUGS
* o	need an sdrXxx call to initialize a master with caller-supplied
*	or default size and position
*
* SEE ALSO
*	sydPlotInitUW, sydPlotDone, sydPlotWinLoop
*	sydPlotSetAttr
*
* EXAMPLE
*
*-*/
long
sydPlotInit(pMstr, pSspec, winType, dispName, winTitle, fullInit)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master structure */
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
PPR_WIN_TY winType;	/* I type of "plot window": PPR_WIN_xxx
			    PPR_WIN_SCREEN for an X11 window
			    PPR_WIN_POSTSCRIPT for a PostScript file
			    PPR_WIN_EPS for an Encapsulated PostScript file
char	*dispName;	/* I name of "plot window"--display, PostScript
			    file, EPS file, or NULL */
char	*winTitle;	/* I title for window title bar and icon */
int	fullInit;	/* I 0 or 1 to do partial or full initialization */
{
    if (fullInit) {
	pMstr->pWin = pprWinOpen(winType, dispName, winTitle, 0,0,0,0);
	if (pMstr->pWin == NULL)
	    return S_syd_ERROR;
	pprWinInfo(pMstr->pWin, &pMstr->x, &pMstr->y, &pMstr->width,
							&pMstr->height);
    }
    else {
	pMstr->pWin = pprWinOpen(winType, dispName, winTitle, 
			pMstr->x, pMstr->y, pMstr->width, pMstr->height);
	if (pMstr->pWin == NULL)
	    return S_syd_ERROR;
    }

    pMstr->winType = winType;
    pMstr->plotAxis = SYD_PLAX_UNDEF;
    pMstr->pSspec = pSspec;
    sydPlotInit_common(pMstr);
    return S_syd_OK;
}
/*+/internal******************************************************************
* NAME	sydPlotInit_common
*
*-*/
static void
sydPlotInit_common(pMstr)
SYD_PL_MSTR *pMstr;
{
    pMstr->linePlot = 1;
    pMstr->pointPlot = 0;
    pMstr->markPlot = 0;
    pMstr->showStat = 0;
    pMstr->fillUnder = 0;
    pMstr->noColor = 0;
    pMstr->errBar = 0;
#ifdef XWINDOWS
    pMstr->pDisp = NULL;
    pMstr->window = NULL;
    pMstr->bg = 0;
    pMstr->fg = 0;
    pMstr->altPixel1 = 0;
    pMstr->altPixel2 = 0;
#endif
    pMstr->label[0] = '\0';
    pMstr->title[0] = '\0';
    pMstr->lTitle[0] = '\0';
    pMstr->bTitle[0] = '\0';
    pMstr->rTitle[0] = '\0';
    pMstr->pHead = NULL;
    pMstr->pTail = NULL;
    pMstr->nSlaves = 0;
    pMstr->originVal = 0.;
    pMstr->extentVal = 0.;
    pMstr->nInt = 1;
    pMstr->nSubInt = 0;
    pMstr->stripIncr = .2;
    pMstr->wrapX = 0;
    if (pMstr->pSspec->sampleCount >= 1) {
	pMstr->originVal =
		pMstr->pSspec->pDeltaSec[pMstr->pSspec->restrictFirstData] -
		pMstr->pSspec->restrictDeltaSecSubtract;
	pMstr->extentVal =
		pMstr->pSspec->pDeltaSec[pMstr->pSspec->restrictLastData] -
		pMstr->pSspec->restrictDeltaSecSubtract;
    }
}

#ifdef XWINDOWS
/*+/subr**********************************************************************
* NAME	sydPlotInitUW - plotting initialization, using User Window
*
* DESCRIPTION
*	Initialize for plotting, using a user-supplied window.
*
*	This routine doesn't perform any actual plotting.  When an expose
*	or resize event occurs (or when additional samples are received
*	when plotting in incremental mode), one of the following routines
*	must be called to perform plotting:
*
*	sydPlotWinReplot--plots all the samples in the synchronous data
*	set.
*
*	sydPlotSamples--plots the indicated subset of the samples in the
*	synchronous data set.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	available only for X11
*
* SEE ALSO
*	sydPlotInit, sydPlotDone, sydPlotWinReplot, sydPlotSamples
*	sydPlotSetAttr
*
* EXAMPLE
*
*-*/
long
sydPlotInitUW(pMstr, pSspec, pDisp, window, gc)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master structure */
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
Display	*pDisp;		/* I X11 display pointer */
Window	window;		/* I X11 window handle */
GC	gc;		/* I X11 gc handle */
{
    long	stat;

    pMstr->pWin = pprWinOpenUW(&pDisp, &window, &gc, NULL);
    assertAlways(pMstr->pWin != NULL);

    pMstr->winType = PPR_WIN_SCREEN;
    pMstr->plotAxis = SYD_PLAX_UNDEF;
    pMstr->pSspec = pSspec;
    sydPlotInit_common(pMstr);
    if (pprWinIsMono(pMstr->pWin))
	pMstr->noColor = 1;
    else
	pMstr->noColor = 0;
    pMstr->pDisp = pDisp;
    pMstr->window = window;

    return S_syd_OK;
}
#endif XWINDOWS

/*+/subr**********************************************************************
* NAME	sydPlotSamples - plot one or more sync samples
*
* DESCRIPTION
*	Plot one or more samples in the synchronous sample set (whose
*	handle is held by the plot master).  The sample range is specified
*	as sample numbers within the sync sample set.
*
*	This routine is for use only with sydPlotInitUW.  When this routine
*	is called, the sydPlot_xxx routine indicated by the .plotAxis
*	member of the plot master structure is called.
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*	sydPlotInitUW, sydPlotSetAttr, sydPlot_xxx
*
* NOTES
* 1.	The `incrFlag' argument allows plotting in either batch or
*	incremental mode.  If incrFlag is 1, then this set of samples will
*	be treated as a continuation of a prior set of samples.  This is
*	important primarily for line plots.  Both sydPlotChanAdd and
*	sydPlotEraseSamples set the flag (for one or all slaves, respectively)
*	indicating there was no prior set of samples; this might be used
*	to avoid having to change incrFlag back and forth.
*
*-*/
void
sydPlotSamples(pMstr, begin, end, incrFlag)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin sample to plot */
int	end;		/* I number of end sample to plot */
int	incrFlag;	/* I 0,1 for batch,incremental plotting */
{
    if (pMstr->plotAxis == SYD_PLAX_TY)
	sydPlot_TYSamples(pMstr, begin, end, incrFlag);
    else if (pMstr->plotAxis == SYD_PLAX_TYY)
	sydPlot_TYSamples(pMstr, begin, end, incrFlag);
    else if (pMstr->plotAxis == SYD_PLAX_XY)
	sydPlot_XYSamples(pMstr, begin, end, incrFlag);
    else if (pMstr->plotAxis == SYD_PLAX_XYY)
	sydPlot_XYSamples(pMstr, begin, end, incrFlag);
    else if (pMstr->plotAxis == SYD_PLAX_Y)
	sydPlot_YSamples(pMstr, begin, end, incrFlag);
    else if (pMstr->plotAxis == SYD_PLAX_YY)
	sydPlot_YSamples(pMstr, begin, end, incrFlag);
    else if (pMstr->plotAxis == SYD_PLAX_STRIP_Y)
	sydPlot_StripYSamples(pMstr, begin, end, incrFlag);
    else if (pMstr->plotAxis == SYD_PLAX_STRIP_YY)
	sydPlot_StripYSamples(pMstr, begin, end, incrFlag);
    else if (pMstr->plotAxis == SYD_PLAX_SMITH_IMP ||
    			pMstr->plotAxis == SYD_PLAX_SMITH_ADM ||
    			pMstr->plotAxis == SYD_PLAX_SMITH_IMM)
	sydPlot_SmithSamples(pMstr, begin, end, incrFlag);
    else
	assertAlways(0);
}

/*+/subr**********************************************************************
* NAME	sydPlotSetAttr - set plot attributes
*
* DESCRIPTION
*	Setting an attribute doesn't automatically reset other related
*	attributes.
*
*	sydPlotSetAttr(pMstr, SYD_PLATTR_FG1, 0, &fgPixVal)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_FG2, 0, &fgPixVal)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_INCR, 0, &stripIncr_sec)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_LINE, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_MARK, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_MONO, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_POINT, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_SHOW, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_STDDEV, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_UNDER, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_WRAP, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_XLAB, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_XANN, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_YLAB, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_YANN, {0,1}, NULL)
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	there should be a SYD_PLATTR_AXIS_TYPE, rather than having to
*	explicitly set the .plotAxis member of the plot master structure
*
* SEE ALSO
*	sydPlotSetTitles, sydPlotInit, sydPlotInitUW
*
* EXAMPLE
*
*-*/
long
sydPlotSetAttr(pMstr, attr, value, pArg)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master structure */
SYD_PLATTR attr;	/* I attribute selector--one of SYD_PLATTR_xxx */
int	value;		/* I value for attribute */
void	*pArg;		/* I pointer for value for attribute */
{
#ifdef XWINDOWS
    if      (attr == SYD_PLATTR_FG1) pMstr->altPixel1 = *(unsigned long *)pArg;
    else if (attr == SYD_PLATTR_FG2) pMstr->altPixel2 = *(unsigned long *)pArg;
    else
#endif
    if (attr == SYD_PLATTR_LINE)   pMstr->linePlot = value;
    else if (attr == SYD_PLATTR_MARK)   pMstr->markPlot = value;
    else if (attr == SYD_PLATTR_POINT)  pMstr->pointPlot = value;
    else if (attr == SYD_PLATTR_SHOW)   pMstr->showStat = value;
    else if (attr == SYD_PLATTR_UNDER)  pMstr->fillUnder = value;
    else if (attr == SYD_PLATTR_WRAP)   pMstr->wrapX = value;
    else if (attr == SYD_PLATTR_XLAB)   pMstr->useXlabel = value;
    else if (attr == SYD_PLATTR_XANN)   pMstr->useXannot = value;
    else if (attr == SYD_PLATTR_YLAB)   pMstr->useYlabel = value;
    else if (attr == SYD_PLATTR_YANN)   pMstr->useYannot = value;
    else if (attr == SYD_PLATTR_MONO)   pMstr->noColor = value;
    else if (attr == SYD_PLATTR_STDDEV) {
	if (value == 1)
	    pMstr->errBar = SYD_PLATTR_STDDEV;
	else
	    pMstr->errBar = 0;
    }
    else if (attr == SYD_PLATTR_INCR)   pMstr->stripIncr = *(float *)pArg;
    else assertAlways(0);

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydPlotSetTitles - establish or change titles for a plot
*
* DESCRIPTION
*	Set the titles for a plot.
*
*	By default, there are no titles for a plot.  If the argument
*	for a particular title isn't NULL, then that title is changed.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	text
*
* SEE ALSO
*	sydPlot_setup, sydPlotSetAttr
*
* EXAMPLE
*
*-*/
long
sydPlotSetTitles(pMstr, top, left, bottom, right)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master structure */
char	*top;		/* I title for top of plot, or NULL */
char	*left;		/* I title for left of plot, or NULL */
char	*bottom;	/* I title for bottom of plot, or NULL */
char	*right;		/* I title for right of plot, or NULL */
{
    if (top != NULL) {
	assert(strlen(top) < sizeof(pMstr->title));
	strcpy(pMstr->title, top);
    }
    if (left != NULL) {
	assert(strlen(left) < sizeof(pMstr->lTitle));
	strcpy(pMstr->lTitle, left);
    }
    if (bottom != NULL) {
	assert(strlen(bottom) < sizeof(pMstr->bTitle));
	strcpy(pMstr->bTitle, bottom);
    }
    if (right != NULL) {
	assert(strlen(right) < sizeof(pMstr->rTitle));
	strcpy(pMstr->rTitle, right);
    }

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydPlotTimeCursor - plot a time `cursor'
*
* DESCRIPTION
*	Draws or erases a vertical line corresponding to the specified
*	time stamp.  If a plot slave is specified, its annotation area
*	is outlined (or the outline is erased).
*
*	The drawing is done using the X GXxor function, so that the first
*	call for a time stamp draws a line and the next call for the
*	same time stamp erases the line.
*
* RETURNS
*	S_syd_OK
*
* NOTES
* 1.	Since alternate calls to this routine draw and erase, speciall
*	handling is needed by the caller who wants to draw several time
*	cursors and also outline the annotation area for a plot slave.
*	For this case, the pointer to the plot slave should be used in only
*	_one_ of the calls to this routine.
*
*-*/
long
sydPlotTimeCursor(pMstr, pStamp, pSlave)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master structure */
TS_STAMP *pStamp;	/* I time stamp to position cursor */
SYD_PL_SLAVE *pSlave;	/* I pointer to plot slave structure, or NULL */
{
    SYD_SPEC	*pSspec = pMstr->pSspec;
    double	diff;		/* offset of stamp from reference stamp */
    PPR_AREA	*pArea;

    if (pMstr->plotAxis != SYD_PLAX_TY && pMstr->plotAxis != SYD_PLAX_TYY)
	return S_syd_OK;

    TsDiffAsDouble(&diff, pStamp, &pSspec->restrictRefTs);
    if (diff < pMstr->originVal || diff > pMstr->extentVal)
	return S_syd_OK;
    pArea = pprAreaOpen(pMstr->pWin,
		pMstr->originFrac, 0., pMstr->extentFrac, 1.,
		pMstr->originVal, 0., pMstr->extentVal, 1.,
		1, 1, 0.);
    assertAlways(pArea != NULL);
    XSetFunction(pArea->pWin->pDisp, pArea->attr.gc, GXinvert);
    XSetPlaneMask(pArea->pWin->pDisp, pArea->attr.gc, 1);
    pprLineSegD(pArea, diff, 0., diff, 1.);
    pprAreaClose(pArea);
    pArea = pprAreaOpen(pMstr->pWin, 0.,0.,1.,1.,  0.,0.,1.,1.,  1, 1, 0.);
    assertAlways(pArea != NULL);
    XSetFunction(pArea->pWin->pDisp, pArea->attr.gc, GXinvert);
    XSetPlaneMask(pArea->pWin->pDisp, pArea->attr.gc, 1);
    if (pSlave != NULL) {
	double	inX, inY;		/* insets into the annot area */
	inX = (pSlave->annotXFR - pSlave->annotXFL) * .05;
	inY = (pSlave->annotYFT - pSlave->annotYFB) * .05;
	pprMoveD(pArea, pSlave->annotXFL+inX, pSlave->annotYFB+inY, 0);
	pprMoveD(pArea, pSlave->annotXFL+inX, pSlave->annotYFT-inY, 1);
	pprMoveD(pArea, pSlave->annotXFR-inX, pSlave->annotYFT-inY, 1);
	pprMoveD(pArea, pSlave->annotXFR-inX, pSlave->annotYFB+inY, 1);
	pprMoveD(pArea, pSlave->annotXFL+inX, pSlave->annotYFB+inY, 1);
    }
    pprAreaClose(pArea);

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydPlotWinLoop - do the actual plotting
*
* DESCRIPTION
*	Perform the actual plotting for a plot master which was set up
*	using sydPlotInit.
*
*	This routine is for use only with sydPlotInit.  When this routine
*	is called, the sydPlot_xxx routine indicated by the .plotAxis
*	member of the plot master structure is called.
*
*	This routine creates and maps a window and draws the plot.  This
*	routine retains control (for processing expose and resize events)
*	until the mouse pointer is placed in the plot window and the right
*	button is clicked.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	text
*
* SEE ALSO
*	sydPlotInit, sydPlotSetAttr, sydPlot_xxx
*
* EXAMPLE
*
*-*/
long
sydPlotWinLoop(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master structure */
{
    SYD_PLAX pltTy = pMstr->plotAxis;		/* type of plot desired */
    long	stat;
    int		npts, beg, end;
    SYD_SPEC	*pSspec = pMstr->pSspec;

    npts = pSspec->sampleCount;
    beg = pSspec->restrictFirstData;
    end = pSspec->restrictLastData;
    if (npts <= 1 || pMstr->originVal == pMstr->extentVal) {
	pMstr->originVal = 0.;
	pMstr->extentVal = 100.;
	strcpy(pMstr->label, "elapsed seconds");
	pMstr->nInt = 5;
    }

    if (pprWinMap(pMstr->pWin) != 0)
	return S_syd_ERROR;
    if (pprWinIsMono(pMstr->pWin))
	pMstr->noColor = 1;
    else
	pMstr->noColor = 0;
    stat = pprWinLoop(pMstr->pWin, sydPlot, pMstr);
    if (stat != OK)
	return S_syd_ERROR;

    pprWinInfo(pMstr->pWin, &pMstr->x, &pMstr->y,&pMstr->width,&pMstr->height);

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydPlotWinReplot - do the actual plotting
*
* DESCRIPTION
*	Perform the actual plotting for a plot master which was set up
*	using sydPlotInitUW.  This routine calls the sydPlot_xxx routine
*	indicated by the .plotAxis member of the plot master structure.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	text
*
* SEE ALSO
*	sydPlotInitUW, sydPlotSamples, sydPlot_xxx
*
* EXAMPLE
*
*-*/
long
sydPlotWinReplot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to plot master structure */
{
    int		npts, beg, end;
    SYD_SPEC	*pSspec = pMstr->pSspec;

    npts = pSspec->sampleCount;
    beg = pSspec->restrictFirstData;
    end = pSspec->restrictLastData;
    if (npts <= 1 || pMstr->originVal == pMstr->extentVal) {
	pMstr->originVal = 0.;
	pMstr->extentVal = 100.;
	strcpy(pMstr->label, "elapsed seconds");
	pMstr->nInt = 5;
    }

    pprWinErase(pMstr->pWin);
    pprWinReplot(pMstr->pWin, sydPlot, pMstr);

    return S_syd_OK;
}

/*+/subr**********************************************************************
* NAME	sydPlot - call the plot routine appropriate for plot type
*
* DESCRIPTION
*	Provides a generic interface for doing the actual plotting.  This
*	routine calls the specific plotting routine as dictated by the
*	set up for the plot master.  That routine will draw the grid(s)
*	and plot the data.
*
*	Prior to calling this routine, the caller must set several values
*	in the plot master structure to control how plotting is done.  Except
*	for the .plotAxis member, the preferred method for setting the
*	values is with the sydPlotSetAttr routine.
*
*	.plotAxis--the type of axis used in plotting:
*	    SYD_PLAX_TY		value vs time, separate grids
*	    SYD_PLAX_TYY	value vs time, shared grid
*	    SYD_PLAX_XY		value vs value, separate grids
*	    SYD_PLAX_XYY	value vs value, shared grid
*	    SYD_PLAX_Y		value vs bin number, separate grids
*	    SYD_PLAX_YY		value vs bin number, shared grid
*	    SYD_PLAX_STRIP_Y	value vs delta time, separate grids
*	    SYD_PLAX_STRIP_YY	value vs delta time, shared grid
*	    SYD_PLAX_SMITH_IMP	value vs value, with Smith impedance overlay
*	    SYD_PLAX_SMITH_ADM	value vs value, with Smith admittance overlay
*	    SYD_PLAX_SMITH_IMM	value vs value, with Smith immitance overlay
*
*	.linePlot--1 to connect data points with lines, else 0
*	.markPlot--1 to plot a mark at each data point, else 0
*	.pointPlot--1 to plot a point at each data point, else 0
*	.showStat--1 to plot a status indicator at each data point, else 0
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*	sydPlotSetAttr, specific sydPlot_xxx routines
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot(pWin, pMstr)
PPR_WIN	*pWin;		/* I pointer to plot window structure */
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    pprWinInfo(pWin, &pMstr->x, &pMstr->y, &pMstr->width, &pMstr->height);
    if (pMstr->plotAxis == SYD_PLAX_TY)
	sydPlot_TYPlot(pMstr);
    else if (pMstr->plotAxis == SYD_PLAX_TYY)
	sydPlot_TYYPlot(pMstr);
    else if (pMstr->plotAxis == SYD_PLAX_XY)
	sydPlot_XYPlot(pMstr);
    else if (pMstr->plotAxis == SYD_PLAX_XYY)
	sydPlot_XYYPlot(pMstr);
    else if (pMstr->plotAxis == SYD_PLAX_Y)
	sydPlot_YPlot(pMstr);
    else if (pMstr->plotAxis == SYD_PLAX_YY)
	sydPlot_YYPlot(pMstr);
    else if (pMstr->plotAxis == SYD_PLAX_STRIP_Y)
	sydPlot_StripYPlot(pMstr);
    else if (pMstr->plotAxis == SYD_PLAX_STRIP_YY)
	sydPlot_StripYYPlot(pMstr);
    else if (pMstr->plotAxis == SYD_PLAX_SMITH_IMP ||
    			pMstr->plotAxis == SYD_PLAX_SMITH_ADM ||
    			pMstr->plotAxis == SYD_PLAX_SMITH_IMM)
	sydPlot_SmithPlot(pMstr);
    else
	assertAlways(0);
}

/*/macro-----------------------------------------------------------------------
* NAME FetchIthValInto - space saver for getting value as a double
*
*----------------------------------------------------------------------------*/
#define FetchIthValInto(pSChan, dbl) \
    if (dbr_type_is_FLOAT(pSChan->dbrType)) \
	dbl = (double)((float *)pSChan->pData)[i]; \
    else if (dbr_type_is_SHORT(pSChan->dbrType)) { \
	if (pSChan->isRVAL) \
	    dbl = (double)((unsigned short *)pSChan->pData)[i]; \
	else \
	    dbl = (double)((short *)pSChan->pData)[i]; \
    } \
    else if (dbr_type_is_DOUBLE(pSChan->dbrType)) \
	dbl = (double)((double *)pSChan->pData)[i]; \
    else if (dbr_type_is_LONG(pSChan->dbrType)) { \
	if (pSChan->isRVAL) \
	    dbl = (double)((unsigned long *)pSChan->pData)[i]; \
	else \
	    dbl = (double)((long *)pSChan->pData)[i]; \
    } \
    else if (dbr_type_is_CHAR(pSChan->dbrType)) \
	dbl = (double)((char *)pSChan->pData)[i]; \
    else if (dbr_type_is_ENUM(pSChan->dbrType)) \
	dbl = (double)((short *)pSChan->pData)[i]; \
    else \
	assertAlways(0);
#if 0
	dbl = (double)((short *)pSChan->pData)[i];
#endif


/*+/internal******************************************************************
* NAME	sydPlot_setup - set up titles and margins for a plot window
*
* DESCRIPTION
*	Plots whatever titles are present in the plot master, reserving
*	an appropriate margin when necessary.
*
*	All slaves can be plotted in a shared grid, or separate grids
*	can be used.  This is controlled by the `nGrids' argument.
*
*	This routine returns information to allow easily intermixing calls
*	to the pprPlot routines with calls to sydPlot routines.
*
*	If plotting is for PostScript, date and time are plotted in the
*	upper right corner of the window.
*
* RETURNS
*	void
*
* BUGS
* o	handles only vertical subdividing of the plot window (i.e., into
*	long horizontal strips)
*
*-*/
static void
sydPlot_setup(pMstr, nGrids, pXlo,pYlo,pXhi,pYhi, pYpart, pCh,pChX,pChTY,pChTX)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	nGrids;		/* I number of grids */
double	*pXlo;		/* O pointer to X position of lower left corner of
				first grid, as fraction of window width */
double	*pYlo;		/* O pointer to Y position of lower left corner of
				first grid, as fraction of window height */
double	*pXhi;		/* O pointer to X position of upper right corner of
				first grid, as fraction of window width */
double	*pYhi;		/* O pointer to Y position of upper right corner of
				first grid, as fraction of window height */
double	*pYpart;	/* O pointer to fraction of window height occupied
				by a single grid (all grids are equal) */
double	*pCh;		/* O pointer to character height, as fraction
				of window height */
double	*pChX;		/* O pointer to character height for rotated text,
				as fraction of window width */
double	*pChTY;		/* O pointer to title character height, as fraction
				of window height */
double	*pChTX;		/* O pointer to title character height for rotated
				text, as fraction of window width */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    double	xlo, ylo, xhi, yhi, yPart, charHtTY, charHtTX, charHt, charHtX;
    PPR_AREA	*pArea;
    TS_STAMP	now;
    char	nowText[32];

    pWin = pMstr->pWin;
    xlo = 0.;
    xhi = .98;
    ylo = 0.;
    yhi = .98;
    charHtTY = .012;
    charHtTX = pprYFracToXFrac(pWin, charHtTY);
/*-----------------------------------------------------------------------------
*    initialize a plot area covering the whole window, with diagonal corners
*    of 0,0 1,1 for plotting the titles
*
*    for PostScript, plot current date and time
*    plot the titles which aren't empty
*----------------------------------------------------------------------------*/
    pArea = pprAreaOpen(pWin, 0.,0., 1.,1., 0.,0., 1.,1., 1, 1, 0.);
    assertAlways(pArea != NULL);
    if (pMstr->winType == PPR_WIN_POSTSCRIPT) {
	(void)tsLocalTime(&now);
	(void)tsStampToText(&now, TS_TEXT_MONDDYYYY, nowText);
	pprText(pArea, .98, .995, nowText, PPR_TXT_RJ, .008, 0.);
    }
    if (strlen(pMstr->title) > 0) {
	yhi = 1. - charHtTY;
	pprText(pArea, .5, yhi, pMstr->title, PPR_TXT_CEN, charHtTY, 0.);
	yhi -= 2. * charHtTY;
    }
    if (strlen(pMstr->lTitle) > 0) {
	xlo = 2. * charHtTX;
	pprText(pArea, xlo, .5, pMstr->lTitle, PPR_TXT_CEN, charHtTX, 90.);
	xlo += 2. * charHtTX;
    }
    if (strlen(pMstr->bTitle) > 0) {
	ylo = 2. * charHtTY;
	pprText(pArea, .5, ylo, pMstr->bTitle, PPR_TXT_CEN, charHtTY, 0.);
	ylo += 2. * charHtTY;
    }
    if (strlen(pMstr->rTitle) > 0) {
	xhi = 1. - 2. * charHtTX;
	pprText(pArea, xhi, .5, pMstr->rTitle, PPR_TXT_CEN, charHtTX, 90.);
	xhi -= 2. * charHtTX;
    }
    pprAreaClose(pArea);

    ylo += 4. * charHtTY;	/* allow for x axis label and annotation */

    yPart = (yhi - ylo)/(double)nGrids;
    yhi = yPart + ylo;
    charHt = PprDfltCharHt(ylo, yhi);
    charHtX = pprYFracToXFrac(pWin, charHt);

    *pXlo = xlo;
    *pXhi = xhi;
    *pYlo = ylo;
    *pYhi = yhi;
    *pYpart = yPart;
    *pCh = charHt;
    *pChX = charHtX;
    *pChTY = charHtTY;
    *pChTX = charHtTX;
}

/*-----------------------------------------------------------------------------
*	structure for annotating time axis
*----------------------------------------------------------------------------*/
static struct {
    unsigned long threshold;	/* seconds for start of partition */
    unsigned long modForEnds;	/* rounding interval for axis ends */
    short	firstForEnds;	/* index in mm/dd/yy hh:mm:ss.msec */
    short	nCharForEnds;	/* # char from mm/dd/... for end label */
    unsigned long modForTicks;	/* rounding interval for axis tick marks */
    short	nSubInt;	/* number of subintervals */
    short	firstForTicks;	/* index in mm/dd/yy hh:mm:ss.msec */
    short	nCharForTicks;	/* # char from mm/dd/... for tick label */
} timeCal[]={
    7*86400+1, 86400, 0, 5, 86400, 24, 0, 5,	/* more than 7 days */
    43201,     86400, 0, 5, 43200, 12, 9, 5,	/* more than 12 hours */
    3601,      3600, 9, 5, 3600, 6, 9, 5,	/* more than 1 hour */
    601,       600, 9, 5, 600, 10, 9, 5,	/* more than 10 minutes */
    61,        60, 12, 5, 60, 6, 12, 5,		/* more than 60 seconds */
    11,        10, 12, 5, 10, 10, 12, 5,	/* more than 10 seconds */
    0,         1, 15, 6, 1, 0, 15, 6		/* <= 10 seconds */
};
/*+/internal******************************************************************
* NAME	sydPlot_setupTime - handle time axis
*
*-*/
sydPlot_setupTime(pMstr, pAnnot, annot)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
char	*pAnnot[20], annot[20][28];
{
    SYD_SPEC	*pSspec = pMstr->pSspec;
    unsigned long xminUL, xmaxUL, elapsedUL, xTickDeltaUL;
    TS_STAMP	stamp, refStamp, xminTs, xmaxTs, elapsedTs;
    char	xminText[28], xmaxText[28], stampText[28];
    int		xNint;
    double	xmin, xmax, ymin, ymax;
    int		i, calNum;
/*-----------------------------------------------------------------------------
*	get start and end times (as set by sydPlotInit) referenced to the
*	restrictRefTs.  Round them to something `sane' based on the total
*	time range, then build the table of annotation labels.  The rounding
*	is based on local time, rather than UTC.
*----------------------------------------------------------------------------*/
    tsAddDouble(&xminTs, &pSspec->restrictRefTs, pMstr->originVal);
    xminTs.nsec = 0;
    tsAddDouble(&xmaxTs, &pSspec->restrictRefTs, pMstr->extentVal);
    if (xmaxTs.nsec > 0) {
	xmaxTs.secPastEpoch++;
	xmaxTs.nsec = 0;
    }
    TsDiffAsStamp(&elapsedTs, &xmaxTs, &xminTs);
    for (calNum=0; ; calNum++) {
	if (timeCal[calNum].threshold <= elapsedTs.secPastEpoch)
	    break;
    }
    tsRoundDownLocal(&xminTs, timeCal[calNum].modForEnds);
    tsRoundUpLocal(&xmaxTs, timeCal[calNum].modForEnds);

    if (xminTs.secPastEpoch == xmaxTs.secPastEpoch)
	xmaxTs.secPastEpoch += timeCal[calNum].modForEnds;
    refStamp = xminTs;

    xminUL = xminTs.secPastEpoch;
    xmaxUL = xmaxTs.secPastEpoch;
    elapsedUL = xmaxUL - xminUL;
    xTickDeltaUL = timeCal[calNum].modForTicks;
    xNint = (xmaxUL - xminUL) / xTickDeltaUL;
    if (xNint >= 20) {
	xNint = 5;
	xTickDeltaUL = elapsedUL / xNint;
    }
    pMstr->nInt = xNint;
    pMstr->nSubInt = timeCal[calNum].nSubInt;
    for (i=0; i<=xNint; i++) {
	tsStampToText(&refStamp, TS_TEXT_MMDDYY, stampText);
	if (i == 0 || i == xNint) {
	    strcpy(annot[i], &stampText[timeCal[calNum].firstForEnds]);
	    annot[i][timeCal[calNum].nCharForEnds] = '\0';
	    if (i == 0)
		TsDiffAsDouble(&xmin, &refStamp, &pSspec->restrictRefTs);
	    else
		TsDiffAsDouble(&xmax, &refStamp, &pSspec->restrictRefTs);
	}
	else {
	    strcpy(annot[i], &stampText[timeCal[calNum].firstForTicks]);
	    annot[i][timeCal[calNum].nCharForTicks] = '\0';
	}
	pAnnot[i] = annot[i];
	refStamp.secPastEpoch += xTickDeltaUL;
    }
    pMstr->originVal = xmin;
    pMstr->extentVal = xmax;
    pMstr->nInt = xNint;

    (void)tsStampToText(&xminTs, TS_TEXT_MMDDYY, xminText);
    (void)tsStampToText(&xmaxTs, TS_TEXT_MMDDYY, xmaxText);
    (void)sprintf(pMstr->label, "%s  to  %s", xminText, xmaxText);
}

/*+/subr**********************************************************************
* NAME	sydPlot_SmithPlot - handle Smith Chart plots
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*	sydPlot_SmithGrid, sydPlot_SmithSamples
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_SmithPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_SmithGrid(pMstr);
    if (pSspec->useStats == 0) {
	sydPlot_SmithSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
    }
    else
	sydPlot_SmithStats(pMstr, 0, pSspec->statCount-1);
}

/*+/subr**********************************************************************
* NAME	sydPlot_SmithGrid - draw a Smith chart overlay
*
* DESCRIPTION
*	Draws a Smith chart overlay, to be used in plotting X vs Y data.
*	Three overlays are available, with axis type controlling which is
*	drawn:
*
*	SYD_PLAX_SMITH_IMP	results in an impedance overlay, with
*		circles tangent on the right.  If SYD_PLATTR_FG1 has been
*		used to set an alternate foreground pixel value, then, on
*		color displays, the overlay is drawn using that pixel value.
*	SYD_PLAX_SMITH_ADM	results in an admittance overlay, with
*		circles tangent on the left.  If SYD_PLATTR_FG2 has been
*		used to set an alternate foreground pixel value, then, on
*		color displays, the overlay is drawn using that pixel value.
*	SYD_PLAX_SMITH_IMM	results in an "immittance" overlay, which is
*		a combination of the impedance overlay on top of the
*		admittance overlay.  On color displays when alternate
*		foreground pixel values have been specified, the overlays
*		are drawn as described above.  On monochrome displays, or
*		when no alternate foreground pixel values are specified,
*		the impedance overlay is drawn with a solid line and the
*		admittance overlay is drawn with a dashed line.
*
*	On color displays, if no alternate foreground pixel values have been
*	set, then the overlays are drawn using the color of the plot window.
*
* RETURNS
*	void
*
* BUGS
* o	channel names aren't displayed
* o	colors are done only under X11
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
*-*/
void
sydPlot_SmithGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;
    PPR_AREA	*pArea;
    double	incr=5.;	/* use 5 degree increments */
    double	x, y, rad;
    int		r;
    static char	*xTxt[]={"5","2","1","0.5","0.2","0"};
    double	xlo, ylo, xhi, yhi;
    double	xmin, ymin, xmax, ymax;
    double	yPart;
    double	charHt, charHtX, charHtTY, charHtTX;
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    SYD_PL_SLAVE *pSlaveX;	/* pointer to X axis slave struct */

    pWin = pMstr->pWin;

    sydPlot_setup(pMstr, 1, &xlo, &ylo, &xhi, &yhi, &yPart,
				&charHt, &charHtX, &charHtTY, &charHtTX);

    xlo += 3. * charHtX;
    ylo += 2. * charHt;
    xhi -= charHt;
    yhi -= 2. * charHt;

    pSlaveX = pMstr->pHead;
    while (1) {
	if (pSlaveX->xChan)
	    break;
	pSlaveX = pSlaveX->pNext;
	if (pSlaveX == NULL) {
	    pSlaveX = pMstr->pHead;
	    break;
	}
    }
    if (pSlaveX == pMstr->pHead)
	pSlave = pSlaveX->pNext;
    else
	pSlave = pMstr->pHead;

    pArea = pprAreaOpen(pWin, xlo,ylo, xhi,yhi, 0.,0., 1.,1., 1, 1, 0.);

    if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM ||
			pMstr->plotAxis == SYD_PLAX_SMITH_IMM) {
/*-----------------------------------------------------------------------------
*	admittance overlay, with circles tangent at x=0,y=.5
*
*	For immitance plots, with this as a secondary overlay, a dashed
*	line pattern is used if the screen is monochrome.  The outer
*	circle isn't drawn and annotations aren't drawn.
*----------------------------------------------------------------------------*/
#ifdef XWINDOWS
	if (pMstr->noColor == 0 && pMstr->altPixel2 != 0)
	    pprAreaSetAttr(pArea, PPR_ATTR_FG, 0, &pMstr->altPixel2);
	else {
#else
	if (1) {
#endif
	    if (pMstr->plotAxis == SYD_PLAX_SMITH_IMM || pMstr->noColor == 0)
		pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, 1, NULL);
	}
	if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
	    pprLineSegD(pArea, 0.,.5, 1.,.5);
	y = .5;
	for (r=6; r>0; r--) {
	    rad = (double)r / 12.;
	    x = rad;
	    if (r != 6 || pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
		pprArcD(pArea, x, y, rad, 0., 360., incr);
	    if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
		pprText(pArea, x+rad+.015, y, xTxt[r-1], PPR_TXT_LJ, 0., 0.);
	}
	x = 0.;
	rad = .25, y = .5 + rad;
	pprArcD(pArea, x, y, rad, 270., 37., incr);
	if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
	    pprText(pArea, .19, .92, "2", PPR_TXT_RJ, 0., 0.);
	y = .5 - rad;
	pprArcD(pArea, x, y, rad, 323., 90., incr);
	if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
	    pprText(pArea, .19, .08, "2", PPR_TXT_RJ, 0., 0.);
	rad = .5, y = .5 + rad;
	pprArcD(pArea, x, y, rad, 270., 0., incr);
	if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
	    pprText(pArea, .5, 1.02, "1", PPR_TXT_CEN, 0., 0.);
	y = .5 - rad;
	pprArcD(pArea, x, y, rad, 0., 90., incr);
	if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
	    pprText(pArea, .5, -.02, "1", PPR_TXT_CEN, 0., 0.);
	rad = 1., y = .5 + rad;
	pprArcD(pArea, x, y, rad, 270., 323., incr);
	if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
	    pprText(pArea, .81, .92, "0.5", PPR_TXT_LJ, 0., 0.);
	y = .5 - rad;
	pprArcD(pArea, x, y, rad, 37., 90., incr);
	if (pMstr->plotAxis == SYD_PLAX_SMITH_ADM)
	    pprText(pArea, .81, .08, "0.5", PPR_TXT_LJ, 0., 0.);
    }
    if (pMstr->plotAxis == SYD_PLAX_SMITH_IMP ||
			pMstr->plotAxis == SYD_PLAX_SMITH_IMM) {
/*-----------------------------------------------------------------------------
*	impedance overlay, with circles tangent at x=1,y=.5
*----------------------------------------------------------------------------*/
#ifdef XWINDOWS
	if (pMstr->noColor == 0 && pMstr->altPixel1 != 0)
	    pprAreaSetAttr(pArea, PPR_ATTR_FG, 0, &pMstr->altPixel1);
	else {
#else
	if (1) {
#endif
	    pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, 0, NULL);
	}
	pprLineSegD(pArea, 0.,.5, 1.,.5);
	y = .5;
	for (r=6; r>0; r--) {
	    rad = (double)r / 12.;
	    x = 1. - rad;
	    pprArcD(pArea, x, y, rad, 0., 360., incr);
	    pprText(pArea, x-rad-.015, y, xTxt[r-1], PPR_TXT_RJ, 0., 0.);
	}
	x = 1.;
	rad = .25, y = .5 + rad;
	pprArcD(pArea, x, y, rad, 143., 270., incr);
	pprText(pArea, .81, .92, "2", PPR_TXT_LJ, 0., 0.);
	y = .5 - rad;
	pprArcD(pArea, x, y, rad, 90., 217., incr);
	pprText(pArea, .81, .08, "2", PPR_TXT_LJ, 0., 0.);
	rad = .5, y = .5 + rad;
	pprArcD(pArea, x, y, rad, 180., 270., incr);
	pprText(pArea, .5, 1.02, "1", PPR_TXT_CEN, 0., 0.);
	y = .5 - rad;
	pprArcD(pArea, x, y, rad, 90., 180., incr);
	pprText(pArea, .5, -.02, "1", PPR_TXT_CEN, 0., 0.);
	rad = 1., y = .5 + rad;
	pprArcD(pArea, x, y, rad, 217., 270., incr);
	pprText(pArea, .19, .92, "0.5", PPR_TXT_RJ, 0., 0.);
	y = .5 - rad;
	pprArcD(pArea, x, y, rad, 90., 143., incr);
	pprText(pArea, .19, .08, "0.5", PPR_TXT_RJ, 0., 0.);
    }
    pprAreaClose(pArea);

    pSlaveX->pArea = NULL;
    xmin = pSlaveX->originVal;
    xmax = pSlaveX->extentVal;
    while (pSlave != NULL) {
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin,
		xlo,ylo, xhi,yhi, xmin, ymin, xmax, ymax, 1, 1, 0.);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo + 12. * charHtX;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo + 6. * charHt;
	pSlave->yFracTop = yhi;
	pSlave->annotXFL = xlo;
	pSlave->annotXFR = pSlave->xFracLeft;
	pSlave->annotYFB = pSlave->yFracBot;
	pSlave->annotYFT = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	pSlave = pSlave->pNext;
	if (pSlave == pSlaveX)
	    pSlave = pSlave->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_SmithSamples - plot one or more samples for a Smith Chart plot
*
* DESCRIPTION
*	the first channel in the plot spec is used for the X axis
*
* RETURNS
*	void
*
* BUGS
* o	this isn't a true Smith chart plot--the caller must have transformed
*	the data into simple X vs Y data
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_SmithSamples(pMstr, begin, end, incr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin sample to plot */
int	end;		/* I number of end sample to plot */
int	incr;		/* I 0,1 for batch,incremental plotting */
{
    sydPlot_XYSamples(pMstr, begin, end, incr);
}

/*+/subr**********************************************************************
* NAME	sydPlot_SmithStats - plot one or more snapshots for a Smith Chart plot
*
* DESCRIPTION
*	the first channel in the plot spec is used for the X axis
*
* RETURNS
*	void
*
* BUGS
* o	this isn't a true Smith chart plot--the caller must have transformed
*	the data into simple X vs Y data
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_SmithStats(pMstr, begin, end)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin snapshot to plot */
int	end;		/* I number of end snapshot to plot */
{
    sydPlot_XYStats(pMstr, begin, end);
}

/*+/subr**********************************************************************
* NAME	sydPlot_StripYPlot - handle strip chart, Y plots
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_StripYPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_StripYGrid(pMstr);
    sydPlot_StripYSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
}

/*+/subr**********************************************************************
* NAME	pprAreaShiftLeft - shift the contents of the plot area
*
* DESCRIPTION
*	Shifts the contents of the plot area to the left, as for a strip
*	chart.
*
* RETURNS
*	void
*
*-*/
void
pprAreaShiftLeft(pArea, dataShift)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	dataShift;	/* I amount to shift left, as an x data value */
{
    int		xc, yc, wc, hc;
    int		xl, xr, yb, yt;
    int		x1, y1, wid1, ht1;

    if (pArea->pWin->winType != PPR_WIN_SCREEN)
	return;
    xl = pArea->xPixLeft;
    xr = xl + nint((pArea->xRight - pArea->xLeft) * pArea->xScale);
    yb = pArea->yPixBot;
    yt = yb + nint((pArea->yTop - pArea->yBot) * pArea->yScale);
    x1 = xl + 1;
    wid1 = xr - x1;
    y1 = yc = pArea->pWin->height - yt + 1;
    ht1 = hc = yt - yb - 1;
    wc = nint(wid1 * dataShift / (pArea->xRight - pArea->xLeft));
    if (wc > wid1)
	wc = wid1;
    wid1 -= wc;
    xc = x1 + wid1;

    XSetGraphicsExposures(pArea->pWin->pDisp,pArea->attr.gc,False);
    if (wid1 > 0) {
	XCopyArea(pArea->pWin->pDisp,
			pArea->pWin->plotWindow, pArea->pWin->plotWindow,
			pArea->attr.gc, x1+wc, y1, wid1, ht1, x1, y1);
    }
    if (wc > 0) {
	XClearArea(pArea->pWin->pDisp, pArea->pWin->plotWindow,
			xc, yc, wc, hc, False);
    }
    XFlush(pArea->pWin->pDisp);
    pArea->xRight += dataShift;
    pArea->xLeft += dataShift;
}

/*+/subr**********************************************************************
* NAME	sydPlot_StripYGrid - draw a grid for a strip chart, Y plot
*
* DESCRIPTION
*
* RETURNS
*	void
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_StripYGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    double	xlo, ylo, xhi, yhi;
    double	yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    char	**ppAnnotVal, *pAnnotVal[20], annotVal[20][28];
    double	charHt, charHtX, charHtTY, charHtTX;
    SYD_SPEC	*pSspec = pMstr->pSspec;
    SYD_CHAN	*pSChan;
    int		thick=3;
    int		nGrids;
    int		i;

    pWin = pMstr->pWin;

    nGrids = pMstr->nSlaves;
    sydPlot_setup(pMstr, nGrids, &xlo, &ylo, &xhi, &yhi, &yPart,
			&charHt, &charHtX, &charHtTY, &charHtTX);
    xmin = -1. * (pMstr->stripIncr * pMstr->pSspec->reqCount);
    xmax = 0.;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for each channel, initialize a plot area.
*
*    plot a perimeter with grid lines
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->ppAnnot == NULL) {
	    sydAnnotVal(pSlave, pAnnotVal, annotVal);
	    ppAnnotVal = pAnnotVal;
	}
	else
	    ppAnnotVal = pSlave->ppAnnot;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin,
		xlo+12.*charHtX, ylo+2.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, 1, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo + 12. * charHtX;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo + 2.*charHt;
	pSlave->yFracTop = yhi;
	pSlave->annotXFL = xlo;
	pSlave->annotXFR = pSlave->xFracLeft;
	pSlave->annotYFB = pSlave->yFracBot;
	pSlave->annotYFT = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    if (dbr_type_is_ENUM(pSChan->dbrType))
		pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	}
	pprGrid(pArea);
	pprAnnotY(pArea, 0, ymin, ymax, pSlave->nInt, 0,
				pSlave->pSChan->label, ppAnnotVal, 0.);
	if (pSlave == pMstr->pHead) {
	    pArea->charHt = charHtTY * pWin->height;
	    pprAnnotX(pArea, 0, xmin, xmax, 1, 0, "delta seconds", NULL, 0.);
	    pArea->charHt = charHt * pWin->height;
	}
	ylo += yPart;
	yhi += yPart;
	pSlave = pSlave->pNext;
    }
    pSlave = pMstr->pHead;
    pMstr->originFrac = pSlave->xFracLeft;
    pMstr->extentFrac = pSlave->xFracRight;

}

/*+/subr**********************************************************************
* NAME	sydPlot_StripYSamples - plot samples for a strip chart, Y plot
*
* DESCRIPTION
*
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_StripYSamples(pMstr, begin, end, incr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin sample to plot */
int	end;		/* I number of end sample to plot */
int	incr;		/* I 0,1 for batch,incremental plotting */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    PPR_AREA	*pArea;
    int		i, j;
    SYD_SPEC	*pSspec;
    SYD_CHAN	*pSChan;
    double	oldX, oldY, newX, newY;
    int		skip;
    int		showStat;	/* 1 to show status code on plot */
    int		pointPlot;	/* 1 for point plot */
    int		linePlot;	/* 1 to connect points with lines */
    int		markPlot;	/* 1 to draw marks at points */
    int		markNum;	/* number of mark to use */
    int		nEl;		/* number of array elements */
    int		first;		/* ==1 if this is the first sample */

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);
    pWin = pMstr->pWin;

    linePlot = pMstr->linePlot;
    pointPlot = pMstr->pointPlot;
    markPlot = pMstr->markPlot;
    showStat = pMstr->showStat;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
	pArea = pSlave->pArea;
	pSChan = pSlave->pSChan;
	markNum = pSlave->markNum;

	if (pSChan->pData == NULL || pSChan->dataChan == 0)
	    ;		/* no action if never connected or not data channel */
	else {
	    nEl = pSChan->elCount;

	    i = begin;
	    if (!incr)
		first = 1;
	    else {
		first = pSlave->first;
		oldX = pSlave->oldX;
		oldY = pSlave->oldY;
		skip = pSlave->skip;
	    }
	    while (i >= 0) {
		int	restart, restart1;
		restart = restart1 = 0;
		if (pSChan->pFlags[i].restart) {
		    if (!pSChan->pFlags[i].snapstart)
			restart = 1;
		    else if (!pSChan->pFlags[i].snapend)
			restart = 1;
		}
		if (i != end && pSChan->pFlags[i+1].restart) {
		    if (!pSChan->pFlags[i+1].snapstart)
			restart1 = 1;
		    else if (!pSChan->pFlags[i+1].snapend)
			restart1 = 1;
		}
		if (pSChan->pFlags[i].missing)
		    skip = 1;
		else if (first || skip || restart) {
		    oldX = pSspec->pDeltaSec[i] -
				pMstr->pSspec->restrictDeltaSecSubtract;
		    if (pMstr->wrapX) {
			while (oldX > pMstr->extentVal)
			    oldX -= pMstr->extentVal;
		    }
		    FetchIthValInto(pSChan, oldY)
		    if (markPlot)
			pprMarkD(pArea, oldX, oldY, markNum);
		    if (showStat && pSChan->pDataCodeR[i] != ' ') {
			pprChar(pArea, oldX,oldY, pSChan->pDataCodeR[i],0.,0.);
		    }
		    else if (pointPlot)
			pprPointD(pArea, oldX, oldY);
		    skip = 0;
		}
		else if (pSChan->pFlags[i].filled && restart1 == 0 && i != end)
		    ;	/* no action */
		else {
		    newX = pSspec->pDeltaSec[i] -
				pMstr->pSspec->restrictDeltaSecSubtract;
		    if (pMstr->wrapX) {
			while (newX > pMstr->extentVal)
			    newX -= pMstr->extentVal;
		    }
		    if (linePlot && dbr_type_is_ENUM(pSChan->dbrType)) {
			pprLineSegD(pArea, oldX, oldY, newX, oldY);
			oldX = newX;
		    }
		    FetchIthValInto(pSChan, newY)
		    if (linePlot)
			pprLineSegD(pArea, oldX, oldY, newX, newY);
		    if (markPlot)
			pprMarkD(pArea, newX, newY, markNum);
		    if (showStat && pSChan->pDataCodeR[i] != ' ') {
			pprChar(pArea, newX,newY, pSChan->pDataCodeR[i],0.,0.);
		    }
		    else if (pointPlot)
			pprPointD(pArea, newX, newY);
		    oldX = newX;
		    oldY = newY;
		}
		if (i == end)
		    i = -1;
		else if (++i >= pSspec->dataDim)
		    i = 0;
		first = 0;
	    }
	}
	pSlave->first = first;
	pSlave->oldX = oldX;
	pSlave->oldY = oldY;
	pSlave->skip = skip;
	pSlave = pSlave->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_StripYYPlot - handle strip chart, multiple Y plots
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	labeling of x axis is un-esthetic.  It should be time based, with
*	some intelligent adaptation, based on time interval for X
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_StripYYPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_StripYYGrid(pMstr);
    sydPlot_StripYSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
}

/*+/subr**********************************************************************
* NAME	sydPlot_StripYYGrid - draw a grid for a strip chart, multiple Y plot
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_StripYYGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    double	xlo, ylo, xhi, yhi;
    double	yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    double	charHt, charHtX, charHtTY, charHtTX;
    char	**ppAnnotVal, *pAnnotVal[20], annotVal[20][28];
    SYD_SPEC	*pSspec = pMstr->pSspec;
    SYD_CHAN	*pSChan;
    int		thick=3;
    int		nGrids;
    int		i, calNum;
    int		offsetAnnotY=0;
    int		drawAxis=0;

    pWin = pMstr->pWin;

    nGrids = 1;
    sydPlot_setup(pMstr, nGrids, &xlo, &ylo, &xhi, &yhi, &yPart,
			&charHt, &charHtX, &charHtTY, &charHtTX);
    xmin = -1. * (pMstr->stripIncr * pMstr->pSspec->reqCount);
    xmax = 0.;

    xlo += 6. * charHtX * (double)pMstr->nSlaves;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for the first channel:
*	initialize a plot area; its fractional size depends on how many
*		"sub-plots" there are
*	plot a perimeter with grid lines
*    for the other channels:
*	initialize an overlapping plot area
*	set a dashed line pattern (unless this is a mark or point plot)
*	draw a "floating" Y axis
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->ppAnnot == NULL) {
	    sydAnnotVal(pSlave, pAnnotVal, annotVal);
	    ppAnnotVal = pAnnotVal;
	}
	else
	    ppAnnotVal = pSlave->ppAnnot;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin, xlo, ylo+2.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, pMstr->nInt, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo;
	pSlave->yFracTop = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    if (dbr_type_is_ENUM(pSChan->dbrType))
		pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	    if (pSlave->lineKey > 1 || pMstr->noColor == 0)
		pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, pSlave->lineKey, NULL);
	}
	else if (pMstr->noColor == 0)
	    pprAreaSetAttr(pArea, PPR_ATTR_COLORNUM, pSlave->lineKey, NULL);
	if (drawAxis == 0) {
	    pprGrid(pArea);
	    if (pSlave == pMstr->pHead) {
		pArea->charHt = charHtTY * pWin->height;
		pprAnnotX(pArea, 0, xmin,xmax,1,0,"delta seconds",NULL,0.);
		pArea->charHt = charHt * pWin->height;
	    }
	}
	pprAnnotY(pArea, offsetAnnotY, pSlave->originVal, pSlave->extentVal,
	    			pSlave->nInt, drawAxis,
				pSlave->pSChan->label, ppAnnotVal, 90.);
	if (pMstr->markPlot)
	    pprAnnotYMark(pArea, offsetAnnotY, pSlave->markNum);
	offsetAnnotY += 6;
	drawAxis = 1;		/* draw an "auxiliary" axis next time */
	pSlave->annotXFL = xlo - offsetAnnotY * charHtX;
	pSlave->annotXFR = pSlave->annotXFL + 6. * charHtX;
	pSlave->annotYFB = ylo;
	pSlave->annotYFT = yhi;
	pSlave = pSlave->pNext;
    }
    pSlave = pMstr->pHead;
    pMstr->originFrac = pSlave->xFracLeft;
    pMstr->extentFrac = pSlave->xFracRight;
}

/*+/subr**********************************************************************
* NAME	sydPlot_TYPlot - handle time vs Y plots
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_TYPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_TYGrid(pMstr);
    if (pSspec->useStats == 0) {
	sydPlot_TYSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
    }
    else
	sydPlot_TYStats(pMstr, 0, pSspec->statCount-1);
}

/*+/subr**********************************************************************
* NAME	sydPlot_TYGrid - draw a grid for a time vs Y plot
*
* DESCRIPTION
*
* RETURNS
*	void
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_TYGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    double	xlo, ylo, xhi, yhi;
    double	yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    char	*pAnnot[20], annot[20][28];	/* annot for time axis */
    char	**ppAnnotVal, *pAnnotVal[20], annotVal[20][28];
    double	charHt, charHtX, charHtTY, charHtTX;
    SYD_SPEC	*pSspec = pMstr->pSspec;
    SYD_CHAN	*pSChan;
    int		thick=3;
    int		nGrids;
    int		i;

    pWin = pMstr->pWin;

    nGrids = pMstr->nSlaves;
    sydPlot_setup(pMstr, nGrids, &xlo, &ylo, &xhi, &yhi, &yPart,
			&charHt, &charHtX, &charHtTY, &charHtTX);
    sydPlot_setupTime(pMstr, pAnnot, annot);
    xmin = pMstr->originVal;
    xmax = pMstr->extentVal;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for each channel, initialize a plot area.
*
*    plot a perimeter with grid lines
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->ppAnnot == NULL) {
	    sydAnnotVal(pSlave, pAnnotVal, annotVal);
	    ppAnnotVal = pAnnotVal;
	}
	else
	    ppAnnotVal = pSlave->ppAnnot;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin,
		xlo+12.*charHtX, ylo+2.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, pMstr->nInt, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo + 12. * charHtX;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo + 2.*charHt;
	pSlave->yFracTop = yhi;
	pSlave->annotXFL = xlo;
	pSlave->annotXFR = pSlave->xFracLeft;
	pSlave->annotYFB = pSlave->yFracBot;
	pSlave->annotYFT = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    if (dbr_type_is_ENUM(pSChan->dbrType))
		pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	}
	pprGrid(pArea);
	pprAnnotY(pArea, 0, ymin, ymax, pSlave->nInt, 0,
				pSlave->pSChan->label, ppAnnotVal, 0.);
	if (pSlave == pMstr->pHead) {
	    pArea->charHt = charHtTY * pWin->height;
	    pArea->xNsubint = pMstr->nSubInt;
	    pprAnnotX(pArea, 0, xmin, xmax, pMstr->nInt, 0,
				    pMstr->label, pAnnot, 0.);
	    pArea->charHt = charHt * pWin->height;
	}
	ylo += yPart;
	yhi += yPart;
	pSlave = pSlave->pNext;
    }
    pSlave = pMstr->pHead;
    pMstr->originFrac = pSlave->xFracLeft;
    pMstr->extentFrac = pSlave->xFracRight;

}

/*+/subr**********************************************************************
* NAME	sydPlot_TYSamples - plot one or more samples for a time vs Y plot
*
* DESCRIPTION
*
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_TYSamples(pMstr, begin, end, incr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin sample to plot */
int	end;		/* I number of end sample to plot */
int	incr;		/* I 0,1 for batch,incremental plotting */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    PPR_AREA	*pArea;
    int		i, j;
    SYD_SPEC	*pSspec;
    SYD_CHAN	*pSChan;
    double	oldX, oldY, newX, newY;
    int		skip;
    int		showStat;	/* 1 to show status code on plot */
    int		pointPlot;	/* 1 for point plot */
    int		linePlot;	/* 1 to connect points with lines */
    int		markPlot;	/* 1 to draw marks at points */
    int		markNum;	/* number of mark to use */
    int		nEl;		/* number of array elements */
    int		first;		/* ==1 if this is the first sample */

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);
    pWin = pMstr->pWin;

    linePlot = pMstr->linePlot;
    pointPlot = pMstr->pointPlot;
    markPlot = pMstr->markPlot;
    showStat = pMstr->showStat;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
	pArea = pSlave->pArea;
	pSChan = pSlave->pSChan;
	markNum = pSlave->markNum;

	if (pSChan->pData == NULL || pSChan->dataChan == 0)
	    ;		/* no action if never connected or not data channel */
	else {
	    nEl = pSChan->elCount;

	    i = begin;
	    if (!incr)
		first = 1;
	    else {
		first = pSlave->first;
		oldX = pSlave->oldX;
		oldY = pSlave->oldY;
		skip = pSlave->skip;
	    }
	    while (i >= 0) {
		int	restart, restart1;
		restart = restart1 = 0;
		if (pSChan->pFlags[i].restart) {
		    if (!pSChan->pFlags[i].snapstart)
			restart = 1;
		    else if (!pSChan->pFlags[i].snapend)
			restart = 1;
		}
		if (i != end && pSChan->pFlags[i+1].restart) {
		    if (!pSChan->pFlags[i+1].snapstart)
			restart1 = 1;
		    else if (!pSChan->pFlags[i+1].snapend)
			restart1 = 1;
		}
		if (pSChan->pFlags[i].missing)
		    skip = 1;
		else if (first || skip || restart) {
		    oldX = pSspec->pDeltaSec[i] -
				pMstr->pSspec->restrictDeltaSecSubtract;
		    if (pMstr->wrapX) {
			while (oldX > pMstr->extentVal)
			    oldX -= pMstr->extentVal;
		    }
		    FetchIthValInto(pSChan, oldY)
		    if (markPlot)
			pprMarkD(pArea, oldX, oldY, markNum);
		    if (showStat && pSChan->pDataCodeR[i] != ' ') {
			pprChar(pArea, oldX,oldY, pSChan->pDataCodeR[i],0.,0.);
		    }
		    else if (pointPlot)
			pprPointD(pArea, oldX, oldY);
		    skip = 0;
		}
		else if (pSChan->pFlags[i].filled && restart1 == 0 && i != end)
		    ;	/* no action */
		else {
		    newX = pSspec->pDeltaSec[i] -
				pMstr->pSspec->restrictDeltaSecSubtract;
		    if (pMstr->wrapX) {
			while (newX > pMstr->extentVal)
			    newX -= pMstr->extentVal;
		    }
		    if (linePlot && dbr_type_is_ENUM(pSChan->dbrType)) {
			pprLineSegD(pArea, oldX, oldY, newX, oldY);
			oldX = newX;
		    }
		    FetchIthValInto(pSChan, newY)
		    if (linePlot)
			pprLineSegD(pArea, oldX, oldY, newX, newY);
		    if (markPlot)
			pprMarkD(pArea, newX, newY, markNum);
		    if (showStat && pSChan->pDataCodeR[i] != ' ') {
			pprChar(pArea, newX,newY, pSChan->pDataCodeR[i],0.,0.);
		    }
		    else if (pointPlot)
			pprPointD(pArea, newX, newY);
		    oldX = newX;
		    oldY = newY;
		}
		if (i == end)
		    i = -1;
		else if (++i >= pSspec->dataDim)
		    i = 0;
		first = 0;
	    }
	}
	pSlave->first = first;
	pSlave->oldX = oldX;
	pSlave->oldY = oldY;
	pSlave->skip = skip;
	pSlave = pSlave->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_TYStats - plot one or more snapshots for a time vs Y plot
*
* DESCRIPTION
*
*
* RETURNS
*	void
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
*-*/
void
sydPlot_TYStats(pMstr, begin, end)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin snapshot to plot */
int	end;		/* I number of end snapshot to plot */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    PPR_AREA	*pArea;
    int		i;
    SYD_SPEC	*pSspec;
    SYD_CHAN	*pSChan;
    double	oldX, oldY, newX, newY, err;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    for (pSlave=pMstr->pHead; pSlave!=NULL; pSlave=pSlave->pNext) {
	pArea = pSlave->pArea;
	pSChan = pSlave->pSChan;
	for (i=begin; i<=end; i++) {
	    if (i == begin) {
		oldX = pSspec->pStatDeltaSec[i] -
				pMstr->pSspec->restrictDeltaSecSubtract;
		if (pMstr->wrapX) {
		    while (oldX > pMstr->extentVal)
			oldX -= pMstr->extentVal;
		}
		oldY = pSChan->pStats[i].mean;
	    }
	    else {
		newX = pSspec->pStatDeltaSec[i] -
				pMstr->pSspec->restrictDeltaSecSubtract;
		if (pMstr->wrapX) {
		    while (newX > pMstr->extentVal)
			newX -= pMstr->extentVal;
		}
		newY = pSChan->pStats[i].mean;
		if (pMstr->linePlot)
		    pprLineSegD(pArea, oldX, oldY, newX, newY);
		oldX = newX;
		oldY = newY;
	    }
	    if (pMstr->markPlot)
		pprMarkD(pArea, oldX, oldY, pSlave->markNum);
	    else
		pprPointD(pArea, oldX, oldY);
	    if (pMstr->errBar == SYD_PLATTR_STDDEV) {
		err = pSChan->pStats[i].stdDev;
		pprErrorBar(pArea, oldX, oldY-err, oldX, oldY+err);
	    }
	}
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_TYYPlot - handle time vs multiple Y plots
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	labeling of x axis is un-esthetic.  It should be time based, with
*	some intelligent adaptation, based on time interval for X
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_TYYPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_TYYGrid(pMstr);
    if (pSspec->useStats == 0) {
	sydPlot_TYSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
    }
    else
	sydPlot_TYStats(pMstr, 0, pSspec->statCount-1);
}

/*+/subr**********************************************************************
* NAME	sydPlot_TYYGrid - draw a grid for a time vs multiple Y plot
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_TYYGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    double	xlo, ylo, xhi, yhi;
    double	yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    char	*pAnnot[20], annot[20][28];	/* annot for time axis */
    double	charHt, charHtX, charHtTY, charHtTX;
    SYD_SPEC	*pSspec = pMstr->pSspec;
    SYD_CHAN	*pSChan;
    int		thick=3;
    int		nGrids;
    int		i, calNum;
    int		offsetAnnotY=0;
    int		drawAxis=0;
    char	**ppAnnotVal, *pAnnotVal[20], annotVal[20][28];

    pWin = pMstr->pWin;

    nGrids = 1;
    sydPlot_setup(pMstr, nGrids, &xlo, &ylo, &xhi, &yhi, &yPart,
			&charHt, &charHtX, &charHtTY, &charHtTX);
    sydPlot_setupTime(pMstr, pAnnot, annot);
    xmin = pMstr->originVal;
    xmax = pMstr->extentVal;

    xlo += 6. * charHtX * (double)pMstr->nSlaves;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for the first channel:
*	initialize a plot area; its fractional size depends on how many
*		"sub-plots" there are
*	plot a perimeter with grid lines
*    for the other channels:
*	initialize an overlapping plot area
*	set a dashed line pattern (unless this is a mark or point plot)
*	draw a "floating" Y axis
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->ppAnnot == NULL) {
	    sydAnnotVal(pSlave, pAnnotVal, annotVal);
	    ppAnnotVal = pAnnotVal;
	}
	else
	    ppAnnotVal = pSlave->ppAnnot;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin, xlo, ylo+2.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, pMstr->nInt, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo;
	pSlave->yFracTop = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    if (dbr_type_is_ENUM(pSChan->dbrType))
		pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	    if (pSlave->lineKey > 1 || pMstr->noColor == 0)
		pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, pSlave->lineKey, NULL);
	}
	else if (pMstr->noColor == 0)
	    pprAreaSetAttr(pArea, PPR_ATTR_COLORNUM, pSlave->lineKey, NULL);
	if (drawAxis == 0) {
	    pprGrid(pArea);
	    if (pSlave == pMstr->pHead) {
		pArea->charHt = charHtTY * pWin->height;
		pArea->xNsubint = pMstr->nSubInt;
		pprAnnotX_wc(pArea, 0,
			xmin, xmax, pMstr->nInt, 0, pMstr->label, pAnnot, 0.);
		pArea->charHt = charHt * pWin->height;
	    }
	}
	pprAnnotY(pArea, offsetAnnotY, pSlave->originVal, pSlave->extentVal,
	    			pSlave->nInt, drawAxis,
				pSlave->pSChan->label, ppAnnotVal, 90.);
	if (pMstr->markPlot)
	    pprAnnotYMark(pArea, offsetAnnotY, pSlave->markNum);
	offsetAnnotY += 6;
	drawAxis = 1;		/* draw an "auxiliary" axis next time */
	pSlave->annotXFL = xlo - offsetAnnotY * charHtX;
	pSlave->annotXFR = pSlave->annotXFL + 6. * charHtX;
	pSlave->annotYFB = ylo;
	pSlave->annotYFT = yhi;
	pSlave = pSlave->pNext;
    }
    pSlave = pMstr->pHead;
    pMstr->originFrac = pSlave->xFracLeft;
    pMstr->extentFrac = pSlave->xFracRight;
}

/*+/subr**********************************************************************
* NAME	sydPlot_XYPlot - handle X vs Y plots
*
* DESCRIPTION
*	the first channel in the plot spec is used for the X axis
*
*	alarm state of the "X" channel is not displayed
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_XYPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_XYGrid(pMstr);
    if (pSspec->useStats == 0) {
	sydPlot_XYSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
    }
    else
	sydPlot_XYStats(pMstr, 0, pSspec->statCount-1);
}

/*+/subr**********************************************************************
* NAME	sydPlot_XYGrid - draw a grid for an X vs Y plot
*
* DESCRIPTION
*	the first channel in the plot spec is used for the X axis
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_XYGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    SYD_PL_SLAVE *pSlaveX;	/* pointer to X axis slave struct */
    double	xlo, ylo, xhi, yhi;
    double	yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    double	charHt, charHtX, charHtTY, charHtTX;
    SYD_CHAN	*pSChan;
    SYD_CHAN	*pSChanX;
    int		nGrids;
    char	**ppAnnotVal, *pAnnotVal[20], annotVal[20][28];
    char	**ppAnnotXVal, *pAnnotXVal[20], annotXVal[20][28];
    int		first=1;

    pWin = pMstr->pWin;

    pSlaveX = pMstr->pHead;
    while (1) {
	if (pSlaveX->xChan)
	    break;
	pSlaveX = pSlaveX->pNext;
	if (pSlaveX == NULL) {
	    pSlaveX = pMstr->pHead;
	    break;
	}
    }
    if (pSlaveX == pMstr->pHead)
	pSlave = pSlaveX->pNext;
    else
	pSlave = pMstr->pHead;
    nGrids = pMstr->nSlaves - 1;
    sydPlot_setup(pMstr, nGrids, &xlo, &ylo, &xhi, &yhi, &yPart,
			&charHt, &charHtX, &charHtTY, &charHtTX);

    pSlaveX->pArea = NULL;
    pSChanX = pSlaveX->pSChan;
    if (pSlaveX->ppAnnot == NULL) {
	sydAnnotVal(pSlaveX, pAnnotXVal, annotXVal);
	ppAnnotXVal = pAnnotXVal;
    }
    else
	ppAnnotVal = pSlave->ppAnnot;
    xmin = pSlaveX->originVal;
    xmax = pSlaveX->extentVal;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for each Y channel, plot a perimeter with grid lines
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->ppAnnot == NULL) {
	    sydAnnotVal(pSlave, pAnnotVal, annotVal);
	    ppAnnotVal = pAnnotVal;
	}
	else
	    ppAnnotVal = pSlave->ppAnnot;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin,
		xlo+12.*charHtX, ylo+2.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, pSlaveX->nInt, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo + 12. * charHtX;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo + 2. * charHt;
	pSlave->yFracTop = yhi;
	pSlave->annotXFL = xlo;
	pSlave->annotXFR = pSlave->xFracLeft;
	pSlave->annotYFB = pSlave->yFracBot;
	pSlave->annotYFT = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	pprGrid(pArea);
	pprAnnotY(pArea, 0, ymin, ymax, pSlave->nInt, 0,
				pSlave->pSChan->label, ppAnnotVal, 0.);
	if (first) {
	    pArea->charHt = charHtTY * pWin->height;
	    pprAnnotX(pArea, 0, xmin, xmax, pSlaveX->nInt, 0,
				    pSlaveX->pSChan->label, ppAnnotXVal, 0.);
	    pArea->charHt = charHt * pWin->height;
	    first = 0;
	}
	ylo += yPart;
	yhi += yPart;
	pSlave = pSlave->pNext;
	if (pSlave == pSlaveX)
	    pSlave = pSlave->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_XYSamples - plot one or more samples for an X vs Y plot
*
* DESCRIPTION
*	the first channel in the plot spec is used for the X axis
*
*	alarm state of the "X" channel is not displayed
*
* RETURNS
*	void
*
* BUGS
* o	line plot isn't handled
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_XYSamples(pMstr, begin, end, incr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin sample to plot */
int	end;		/* I number of end sample to plot */
int	incr;		/* I 0,1 for batch,incremental plotting */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    SYD_PL_SLAVE *pSlaveX;	/* pointer to X axis slave struct */
    PPR_AREA	*pArea;
    int		i, j;
    SYD_SPEC	*pSspec;
    SYD_CHAN	*pSChan;
    SYD_CHAN	*pSChanX;
    double	oldX, oldY, newX, newY;
    int		skip;
    int		showStat;	/* 1 to show status code on plot */
    int		pointPlot;	/* 1 for point plot */
    int		linePlot;	/* 1 to connect points with lines */
    int		markPlot;	/* 1 to draw marks at points */
    int		markNum;	/* number of mark to use */
    int		nEl;		/* number of array elements */
    int		first;		/* ==1 if this is the first sample */

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);
    pWin = pMstr->pWin;

    linePlot = pMstr->linePlot;
    pointPlot = pMstr->pointPlot;
    markPlot = pMstr->markPlot;
    showStat = pMstr->showStat;

    pSlaveX = pMstr->pHead;
    while (1) {
	if (pSlaveX->xChan)
	    break;
	pSlaveX = pSlaveX->pNext;
	if (pSlaveX == NULL) {
	    pSlaveX = pMstr->pHead;
	    break;
	}
    }
    if (pSlaveX == pMstr->pHead)
	pSlave = pSlaveX->pNext;
    else
	pSlave = pMstr->pHead;
    pSChanX = pSlaveX->pSChan;
    while (pSlave != NULL) {
	pArea = pSlave->pArea;
	pSChan = pSlave->pSChan;
	if (pSChan->pData == NULL || pSChan->dataChan == 0)
	    ;		/* no action if never connected or not data channel */
	else {
	    markNum = pSlave->markNum;

	    nEl = pSChanX->elCount;
	    if (nEl > pSChan->elCount)
		nEl = pSChan->elCount;

	    i = begin;
	    if (!incr)
		first = 1;
	    else {
		first = pSlave->first;
		oldX = pSlave->oldX;
		oldY = pSlave->oldY;
		skip = pSlave->skip;
	    }
	    while (i >= 0) {
		int	restart, restart1;
		restart = restart1 = 0;
		if (pSChan->pFlags[i].restart) {
		    if (!pSChan->pFlags[i].snapstart)
			restart = 1;
		    else if (!pSChan->pFlags[i].snapend)
			restart = 1;
		}
		if (i != end && pSChan->pFlags[i+1].restart) {
		    if (!pSChan->pFlags[i+1].snapstart)
			restart1 = 1;
		    else if (!pSChan->pFlags[i+1].snapend)
			restart1 = 1;
		}
		if (pSChan->pFlags[i].missing || pSChanX->pFlags[i].missing)
		    skip = 1;
		else if (first || skip || restart ||
						pSChanX->pFlags[i].restart) {
		    if (nEl == 1) {
			FetchIthValInto(pSChanX, oldX)
			FetchIthValInto(pSChan, oldY)
			if (markPlot)
			    pprMarkD(pArea, oldX, oldY, markNum);
			if (showStat && pSChan->pDataCodeR[i] != ' ') {
			    pprChar(pArea,
				oldX, oldY, pSChan->pDataCodeR[i], 0., 0.);
			}
			else if (pointPlot)
			    pprPointD(pArea, oldX, oldY);
		    }
		    else {
			sydPlot_XYarray(pArea, pSChanX, pSChan, i);
		    }
		    skip = 0;
		}
#if 0	/* don't "optimize" filled--show each x,y */
		else if (pSChan->pFlags[i].filled && i != end &&
				pSChan->pFlags[i+1].restart == 0)
		    ;	/* no action */
#endif
		else {
		    if (nEl == 1) {
			FetchIthValInto(pSChanX, newX)
			FetchIthValInto(pSChan, newY)
			if (linePlot)
			    pprLineSegD(pArea, oldX, oldY, newX, newY);
			if (markPlot)
			    pprMarkD(pArea, newX, newY, markNum);
			if (showStat && pSChan->pDataCodeR[i] != ' ') {
			    pprChar(pArea,
				newX, newY, pSChan->pDataCodeR[i], 0., 0.);
			}
			else if (pointPlot)
			    pprPointD(pArea, newX, newY);
			oldX = newX;
			oldY = newY;
		    }
		    else {
			sydPlot_XYarray(pArea, pSChanX, pSChan, i);
		    }
		}
		if (i == end)
		    i = -1;
		else if (++i >= pSspec->dataDim)
		    i = 0;
		first = 0;
	    }
	}
	pSlave->first = first;
	pSlave->oldX = oldX;
	pSlave->oldY = oldY;
	pSlave->skip = skip;
	pSlave = pSlave->pNext;
	if (pSlave == pSlaveX)
	    pSlave = pSlave->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_XYStats - plot one or more snapshots for an X vs Y plot
*
* DESCRIPTION
*	the first channel in the plot spec is used for the X axis
*
*	alarm state of the "X" channel is not displayed
*
* RETURNS
*	void
*
* BUGS
* o	line plot isn't handled
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_XYStats(pMstr, begin, end)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin snapshots to plot */
int	end;		/* I number of end snapshots to plot */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    SYD_PL_SLAVE *pSlaveX;	/* pointer to X axis slave struct */
    PPR_AREA	*pArea;
    int		i;
    SYD_SPEC	*pSspec;
    SYD_CHAN	*pSChan;
    SYD_CHAN	*pSChanX;
    double	oldX, oldY, newX, newY, err;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    pSlaveX = pMstr->pHead;
    while (1) {
	if (pSlaveX->xChan)
	    break;
	pSlaveX = pSlaveX->pNext;
	if (pSlaveX == NULL) {
	    pSlaveX = pMstr->pHead;
	    break;
	}
    }
    if (pSlaveX == pMstr->pHead)
	pSlave = pSlaveX->pNext;
    else
	pSlave = pMstr->pHead;
    pSChanX = pSlaveX->pSChan;
    while (pSlave != NULL) {
	pArea = pSlave->pArea;
	pSChan = pSlave->pSChan;
	for (i=begin; i<=end; i++) {
	    if (i == begin) {
		oldX = pSChanX->pStats[i].mean;
		oldY = pSChan->pStats[i].mean;
	    }
	    else {
		newX = pSChanX->pStats[i].mean;
		newY = pSChan->pStats[i].mean;
		if (pMstr->linePlot)
		    pprLineSegD(pArea, oldX, oldY, newX, newY);
		oldX = newX;
		oldY = newY;
	    }
	    if (pMstr->markPlot)
		pprMarkD(pArea, oldX, oldY, pSlave->markNum);
	    else
		pprPointD(pArea, oldX, oldY);
	    if (pMstr->errBar == SYD_PLATTR_STDDEV) {
		err = pSChan->pStats[i].stdDev;
		pprErrorBar(pArea, oldX, oldY-err, oldX, oldY+err);
		err = pSChanX->pStats[i].stdDev;
		pprErrorBar(pArea, oldX-err, oldY, oldX+err, oldY);
	    }
	}
	pSlave = pSlave->pNext;
	if (pSlave == pSlaveX)
	    pSlave = pSlave->pNext;
    }
}

/*+/internal******************************************************************
* NAME	sydPlot_XYarray - plot array vs array
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
static void
sydPlot_XYarray(pArea, pSChanX, pSChan, sub)
PPR_AREA *pArea;
SYD_CHAN *pSChan;
SYD_CHAN *pSChanX;
int	sub;
{
    int		nEl, nElX, nElY, i;
    char	*pSrcX, *pSrcY;
    double	newX, newY, oldX, oldY;
    int		nByteX, nByteY;

    nEl = nElX = pSChanX->elCount;
    nElY = pSChan->elCount;
    if (nElX > nElY)
	nEl = nElY;
    nByteX = dbr_value_size[pSChanX->dbrType];
    pSrcX = (char *)pSChanX->pData + sub * nByteX * nElX;
    nByteY = dbr_value_size[pSChan->dbrType];
    pSrcY = (char *)pSChan->pData + sub * nByteY * nElY;
    for (i=0; i<nEl; i++) {
	if      (dbr_type_is_FLOAT(pSChanX->dbrType))
	    newX = *(float *)pSrcX;
	else if (dbr_type_is_SHORT(pSChanX->dbrType))
	    newX = *(short *)pSrcX;
	else if (dbr_type_is_DOUBLE(pSChanX->dbrType))
	    newX = *(double *)pSrcX;
	else if (dbr_type_is_LONG(pSChanX->dbrType))
	    newX = *(long *)pSrcX;
	else if (dbr_type_is_CHAR(pSChanX->dbrType))
	    newX = *(unsigned char *)pSrcX;
	else if (dbr_type_is_ENUM(pSChanX->dbrType))
	    newX = *(short *)pSrcX;
	if      (dbr_type_is_FLOAT(pSChan->dbrType))
	    newY = *(float *)pSrcY;
	else if (dbr_type_is_SHORT(pSChan->dbrType))
	    newY = *(short *)pSrcY;
	else if (dbr_type_is_DOUBLE(pSChan->dbrType))
	    newY = *(double *)pSrcY;
	else if (dbr_type_is_LONG(pSChan->dbrType))
	    newY = *(long *)pSrcY;
	else if (dbr_type_is_CHAR(pSChan->dbrType))
	    newY = *(unsigned char *)pSrcY;
	else if (dbr_type_is_ENUM(pSChan->dbrType))
	    newY = *(short *)pSrcY;
	if (i > 0)
	    pprLineSegD(pArea, oldX, oldY, newX, newY);
	oldX = newX;
	oldY = newY;
	pSrcX += nByteX;
	pSrcY += nByteY;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_XYYPlot - handle X vs multiple Y plots
*
* DESCRIPTION
*	the first channel in the plot spec is used for the X axis
*
*	alarm state of the "X" channel is not displayed
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_XYYPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_XYYGrid(pMstr);
    if (pSspec->useStats == 0) {
	sydPlot_XYSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
    }
    else
	sydPlot_XYStats(pMstr, 0, pSspec->statCount-1);
}

/*+/subr**********************************************************************
* NAME	sydPlot_XYYGrid - draw a grid for an X vs multiple Y plot
*
* DESCRIPTION
*	the first channel in the plot spec is used for the X axis
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_XYYGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    SYD_PL_SLAVE *pSlaveX;	/* pointer to X axis slave struct */
    double	xlo, ylo, xhi, yhi, yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    double	charHt, charHtX, charHtTY, charHtTX;
    SYD_CHAN	*pSChan;
    SYD_CHAN	*pSChanX;
    int		offsetAnnotY=0;
    int		drawAxis=0;
    int		nGrids;
    char	**ppAnnotVal, *pAnnotVal[20], annotVal[20][28];
    char	**ppAnnotXVal, *pAnnotXVal[20], annotXVal[20][28];

    pWin = pMstr->pWin;

    pSlaveX = pMstr->pHead;
    while (1) {
	if (pSlaveX->xChan)
	    break;
	pSlaveX = pSlaveX->pNext;
	if (pSlaveX == NULL) {
	    pSlaveX = pMstr->pHead;
	    break;
	}
    }
    if (pSlaveX == pMstr->pHead)
	pSlave = pSlaveX->pNext;
    else
	pSlave = pMstr->pHead;
    pSChanX = pSlaveX->pSChan;

    nGrids = 1;
    sydPlot_setup(pMstr, nGrids, &xlo, &ylo, &xhi, &yhi, &yPart,
			&charHt, &charHtX, &charHtTY, &charHtTX);
    xlo += 6. * charHtX * (double)(pMstr->nSlaves - 1);

    pSlaveX->pArea = NULL;
    xmin = pSlaveX->originVal;
    xmax = pSlaveX->extentVal;
    if (pSlaveX->ppAnnot == NULL) {
	sydAnnotVal(pSlaveX, pAnnotXVal, annotXVal);
	ppAnnotXVal = pAnnotXVal;
    }
    else
	ppAnnotXVal = pSlaveX->ppAnnot;
    pSlaveX->yFracBot = pSlaveX->yFracTop = 0.;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for the first Y channel:
*	initialize a plot area; its fractional size depends on how many
*		"sub-plots" there are
*	plot a perimeter with grid lines
*	set for solid line
*    for the other channels:
*	initialize an overlapping plot area
*	set a dashed line pattern (unless this is a mark or point plot)
*	draw a "floating" Y axis
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->ppAnnot == NULL) {
	    sydAnnotVal(pSlave, pAnnotVal, annotVal);
	    ppAnnotVal = pAnnotVal;
	}
	else
	    ppAnnotVal = pSlave->ppAnnot;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
        pArea = pSlave->pArea = pprAreaOpen(pWin, xlo, ylo+2.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, pSlaveX->nInt, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo;
	pSlave->yFracTop = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    /* set keynum if color is being used or if this is auxiliary axis */
	    if (drawAxis || pMstr->noColor == 0)
		pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, pSlave->lineKey, NULL);
	}
	else if (pMstr->noColor == 0)
	    pprAreaSetAttr(pArea, PPR_ATTR_COLORNUM, pSlave->lineKey, NULL);
	if (drawAxis == 0) {
	    pprGrid(pArea);
	    pArea->charHt = charHtTY * pWin->height;
	    pprAnnotX_wc(pArea, 0, pSlaveX->originVal, pSlaveX->extentVal,
		pSlaveX->nInt, 0, pSlaveX->pSChan->label, ppAnnotXVal, 0.);
	    pArea->charHt = charHt * pWin->height;
	}
	pprAnnotY(pArea, offsetAnnotY, pSlave->originVal, pSlave->extentVal,
	   pSlave->nInt, drawAxis, pSlave->pSChan->label, ppAnnotVal, 90.);
	if (pMstr->markPlot)
	    pprAnnotYMark(pArea, offsetAnnotY, pSlave->markNum);
	offsetAnnotY += 6;
	drawAxis = 1;		/* draw an "auxiliary" axis next time */
	pSlave->annotXFL = xlo - offsetAnnotY * charHtX;
	pSlave->annotXFR = pSlave->annotXFL + 6. * charHtX;
	pSlave->annotYFB = ylo;
	pSlave->annotYFT = yhi;
	pSlave = pSlave->pNext;
	if (pSlave == pSlaveX)
	    pSlave = pSlave->pNext;
    }

    return;
}

/*+/subr**********************************************************************
* NAME	sydPlot_YPlot - handle Y plots
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_YPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_YGrid(pMstr);
    if (pSspec->useStats == 0) {
	sydPlot_YSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
    }
    else
	sydPlot_YStats(pMstr, 0, pSspec->statCount-1);
}

/*+/subr**********************************************************************
* NAME	sydPlot_YGrid - draw a grid for a Y plot
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_YGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    double	xlo, ylo, xhi, yhi;
    double	yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    int		xNint;
    double	charHt, charHtX, charHtTY, charHtTX;
    SYD_CHAN	*pSChan;
    int		thick=3;
    int		nGrids;
    char	**ppAnnotVal, *pAnnotVal[20], annotVal[20][28];

    pWin = pMstr->pWin;

    nGrids = pMstr->nSlaves;
    sydPlot_setup(pMstr, nGrids, &xlo, &ylo, &xhi, &yhi, &yPart,
			&charHt, &charHtX, &charHtTY, &charHtTX);

    xmin = xmax = 0.;
    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
	if (pSlave->pSChan->elCount > xmax)
	    xmax = pSlave->pSChan->elCount;
	pSlave = pSlave->pNext;
    }
    xNint = 1;
    if (xmax == 1.)
	xmax = pMstr->pSspec->reqCount - 1;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for each channel, initialize a plot area.
*
*    plot a perimeter with grid lines
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	charHt = PprDfltCharHt(ylo, yhi);
	charHtX = pprYFracToXFrac(pWin, charHt);
	if (pSlave->ppAnnot == NULL) {
	    sydAnnotVal(pSlave, pAnnotVal, annotVal);
	    ppAnnotVal = pAnnotVal;
	}
	else
	    ppAnnotVal = pSlave->ppAnnot;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin,
		xlo+12.*charHtX, ylo+2.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, xNint, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo + 12. * charHtX;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo + 6. * charHt;
	pSlave->yFracTop = yhi;
	pSlave->annotXFL = xlo;
	pSlave->annotXFR = pSlave->xFracLeft;
	pSlave->annotYFB = pSlave->yFracBot;
	pSlave->annotYFT = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    if (dbr_type_is_ENUM(pSChan->dbrType))
		pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	}
	pprGrid(pArea);
	pprAnnotY(pArea, 0, ymin, ymax, pSlave->nInt, 0,
				pSlave->pSChan->label, ppAnnotVal, 0.);
	if (pSlave == pMstr->pHead) {
	    pArea->charHt = charHtTY * pWin->height;
	    pprAnnotX(pArea, 0, xmin, xmax, xNint, 0, "bin number", NULL, 0.);
	    pArea->charHt = charHt * pWin->height;
	}
	ylo += yPart;
	yhi += yPart;
	pSlave = pSlave->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_YSamples - plot one or more samples for a Y plot
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_YSamples(pMstr, begin, end, incr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin sample to plot */
int	end;		/* I number of end sample to plot */
int	incr;		/* I 0,1 for batch,incremental plotting */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    PPR_AREA	*pArea;
    int		i, j;
    SYD_SPEC	*pSspec;
    SYD_CHAN	*pSChan;
    double	oldX, oldY, newX, newY;
    int		skip;
    int		showStat;	/* 1 to show status code on plot */
    int		pointPlot;	/* 1 for point plot */
    int		linePlot;	/* 1 to connect points with lines */
    int		markPlot;	/* 1 to draw marks at points */
    int		markNum;	/* number of mark to use */
    int		nEl;		/* number of array elements */
    int		first;		/* ==1 if this is the first sample */

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);
    pWin = pMstr->pWin;

    linePlot = pMstr->linePlot;
    pointPlot = pMstr->pointPlot;
    markPlot = pMstr->markPlot;
    showStat = pMstr->showStat;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
	pArea = pSlave->pArea;
	pSChan = pSlave->pSChan;
	if (pSChan->pData == NULL || pSChan->dataChan == 0)
	    ;		/* no action if never connected or not data channel */
	else {
	    markNum = pSlave->markNum;

	    nEl = pSChan->elCount;

	    i = begin;
	    if (!incr)
		first = 1;
	    else {
		first = pSlave->first;
		oldX = pSlave->oldX;
		oldY = pSlave->oldY;
		skip = pSlave->skip;
	    }
	    while (i >= 0) {
		int	restart, restart1;
		restart = restart1 = 0;
		if (pSChan->pFlags[i].restart) {
		    if (!pSChan->pFlags[i].snapstart)
			restart = 1;
		    else if (!pSChan->pFlags[i].snapend)
			restart = 1;
		}
		if (i != end && pSChan->pFlags[i+1].restart) {
		    if (!pSChan->pFlags[i+1].snapstart)
			restart1 = 1;
		    else if (!pSChan->pFlags[i+1].snapend)
			restart1 = 1;
		}
		if (pSChan->pFlags[i].missing)
		    skip = 1;
		else if (first || skip || restart) {
		    if (nEl == 1) {
			oldX = i;
			FetchIthValInto(pSChan, oldY)
			if (markPlot)
			    pprMarkD(pArea, oldX, oldY, markNum);
			if (showStat && pSChan->pDataCodeR[i] != ' ') {
			    pprChar(pArea, oldX, oldY,
						pSChan->pDataCodeR[i], 0., 0.);
			}
			else if (pointPlot)
			    pprPointD(pArea, oldX, oldY);
		    }
		    else {
			sydPlot_Yarray(pArea, pSChan, i);
		    }
		    skip = 0;
		}
		else if (pSChan->pFlags[i].filled && restart1 == 0 && i != end)
		    ;	/* no action */
		else {
		    if (nEl == 1) {
			newX = i;
			if (linePlot && dbr_type_is_ENUM(pSChan->dbrType)) {
			    pprLineSegD(pArea, oldX, oldY, newX, oldY);
			    oldX = newX;
			}
			FetchIthValInto(pSChan, newY)
			if (linePlot)
			    pprLineSegD(pArea, oldX, oldY, newX, newY);
			if (markPlot)
			    pprMarkD(pArea, newX, newY, markNum);
			if (showStat && pSChan->pDataCodeR[i] != ' ') {
			    pprChar(pArea, newX, newY,
						pSChan->pDataCodeR[i], 0., 0.);
			}
			else if (pointPlot)
			    pprPointD(pArea, newX, newY);
			oldX = newX;
			oldY = newY;
		    }
		    else {
			sydPlot_Yarray(pArea, pSChan, i);
		    }
		}
		if (i == end)
		    i = -1;
		else if (++i >= pSspec->dataDim)
		    i = 0;
		first = 0;
	    }
	}
	pSlave->first = first;
	pSlave->oldX = oldX;
	pSlave->oldY = oldY;
	pSlave->skip = skip;
	pSlave = pSlave->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_YStats - plot one or more snapshots for a Y plot
*
* DESCRIPTION
*
* RETURNS
*	void
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
*-*/
void
sydPlot_YStats(pMstr, begin, end)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
int	begin;		/* I number of begin snapshots to plot */
int	end;		/* I number of end snapshots to plot */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    PPR_AREA	*pArea;
    int		i;
    SYD_SPEC	*pSspec;
    SYD_CHAN	*pSChan;
    double	oldX, oldY, newX, newY, err;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    for (pSlave=pMstr->pHead; pSlave!=NULL; pSlave=pSlave->pNext) {
	pArea = pSlave->pArea; pSChan = pSlave->pSChan;
	for (i=begin; i<=end; i++) {
	    if (i == begin) {
		oldX = i; oldY = pSChan->pStats[i].mean;
	    }
	    else {
		newX = i; newY = pSChan->pStats[i].mean;
		if (pMstr->linePlot)
		    pprLineSegD(pArea, oldX, oldY, newX, newY);
		oldX = newX; oldY = newY;
	    }
	    if (pMstr->markPlot)
		pprMarkD(pArea, oldX, oldY, pSlave->markNum);
	    else
		pprPointD(pArea, oldX, oldY);
	    if (pMstr->errBar == SYD_PLATTR_STDDEV) {
		err = pSChan->pStats[i].stdDev;
		pprErrorBar(pArea, oldX, oldY-err, oldX, oldY+err);
	    }
	}
    }
}

/*+/internal******************************************************************
* NAME	sydPlot_Yarray - plot array vs array
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
static void
sydPlot_Yarray(pArea, pSChan, sub)
PPR_AREA *pArea;
SYD_CHAN *pSChan;
int	sub;
{
    int		nEl, nElY, i;
    char	*pSrcY;
    double	newX, newY, oldX, oldY;
    int		nByteY;

    nEl = nElY = pSChan->elCount;
    nByteY = dbr_value_size[pSChan->dbrType];
    pSrcY = (char *)pSChan->pData + sub * nByteY * nElY;
    for (i=0; i<nEl; i++) {
	newX = i;
	if      (dbr_type_is_FLOAT(pSChan->dbrType))
	    newY = *(float *)pSrcY;
	else if (dbr_type_is_SHORT(pSChan->dbrType))
	    newY = *(short *)pSrcY;
	else if (dbr_type_is_DOUBLE(pSChan->dbrType))
	    newY = *(double *)pSrcY;
	else if (dbr_type_is_LONG(pSChan->dbrType))
	    newY = *(long *)pSrcY;
	else if (dbr_type_is_CHAR(pSChan->dbrType))
	    newY = *(unsigned char *)pSrcY;
	else if (dbr_type_is_ENUM(pSChan->dbrType))
	    newY = *(short *)pSrcY;
	if (i > 0)
	    pprLineSegD(pArea, oldX, oldY, newX, newY);
	oldX = newX;
	oldY = newY;
	pSrcY += nByteY;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_YYPlot - handle multiple Y plots
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_YYPlot(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_SPEC	*pSspec;

    assert(pMstr != NULL);
    pSspec = pMstr->pSspec;
    assert(pSspec != NULL);

    sydPlot_YYGrid(pMstr);
    if (pSspec->useStats == 0) {
	sydPlot_YSamples(pMstr,
		pSspec->restrictFirstData, pSspec->restrictLastData, 0);
    }
    else
	sydPlot_YStats(pMstr, 0, pSspec->statCount-1);
}

/*+/subr**********************************************************************
* NAME	sydPlot_YYGrid - draw a grid for a multiple Y plot
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* NOTES
* 1. This routine isn't intended to be called directly.  
*
* EXAMPLE
*
*-*/
void
sydPlot_YYGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    double	xlo, ylo, xhi, yhi, yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    int		xNint;
    double	charHt, charHtX, charHtTY, charHtTX;
    SYD_CHAN	*pSChan;
    int		offsetAnnotY=0;
    int		drawAxis=0;
    int		thick=3;
    int		nGrids;

    pWin = pMstr->pWin;

    nGrids = 1;
    sydPlot_setup(pMstr, nGrids, &xlo, &ylo, &xhi, &yhi, &yPart,
			&charHt, &charHtX, &charHtTY, &charHtTX);
    xlo += 6. * charHtX * (double)pMstr->nSlaves;

    xmin = xmax = 0.;
    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
	if (pSlave->pSChan->elCount > xmax)
	    xmax = pSlave->pSChan->elCount;
	pSlave = pSlave->pNext;
    }
    xNint = 1;
    if (xmax == 1.)
	xmax = pMstr->pSspec->reqCount - 1;

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for the first channel:
*	initialize a plot area; its fractional size depends on how many
*		"sub-plots" there are
*	plot a perimeter with grid lines
*    for the other channels:
*	initialize an overlapping plot area
*	set a dashed line pattern (unless this is a mark or point plot)
*	draw a "floating" Y axis
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin, xlo, ylo+2.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, xNint, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo;
	pSlave->yFracTop = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    if (dbr_type_is_ENUM(pSChan->dbrType))
		pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	    if (pSlave->lineKey > 1 || pMstr->noColor == 0)
		pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, pSlave->lineKey, NULL);
	}
	else if (pMstr->noColor == 0)
	    pprAreaSetAttr(pArea, PPR_ATTR_COLORNUM, pSlave->lineKey, NULL);
	if (drawAxis == 0) {
	    pprGrid(pArea);
	    pArea->charHt = charHtTY * pWin->height;
	    pprAnnotX_wc(pArea, 0, xmin, xmax, xNint, 0, "bin number", NULL,0.);
	    pArea->charHt = charHt * pWin->height;
	}
	pprAnnotY(pArea, offsetAnnotY, pSlave->originVal, pSlave->extentVal,
	    			pSlave->nInt, drawAxis,
				pSlave->pSChan->label, pSlave->ppAnnot, 90.);
	if (pMstr->markPlot)
	    pprAnnotYMark(pArea, offsetAnnotY, pSlave->markNum);
	offsetAnnotY += 6;
	drawAxis = 1;		/* draw an "auxiliary" axis next time */
	pSlave->annotXFL = xlo - offsetAnnotY * charHtX;
	pSlave->annotXFR = pSlave->annotXFL + 6. * charHtX;
	pSlave->annotYFB = ylo;
	pSlave->annotYFT = yhi;
	pSlave = pSlave->pNext;
    }
}
