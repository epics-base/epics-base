/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	12-04-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
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
 * .01	12-04-90	rac	initial version
 * .02	mm-dd-yy	rac	version 2.0, installed in SCCS
 *
 * make options
 *	-DXWINDOWS	makes a version for X11
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	sydPlot.c - plotting for synchronous data
*
* DESCRIPTION
*
* QUICK REFERENCE
*
*         void  sydPlotAxisAutoRange(pSlave)
*         long  sydPlotAxisSetAttr(pSlave, attr, value, pArg)
*			attr = SYD_PLATTR_{GC}
* SYD_PL_SLAVE *sydPlotChanAdd(pMstr, pSChan)
*         long  sydPlotDone(pMstr, quitFlag)
*         long  sydPlotEraseSamples(pMstr)
*         long  sydPlotInit(pMstr, pSspec, winType, dispName, winTitle,
*							fullInit)
*         long  sydPlotInitUW(pMstr, pSspec, pDisp, window, gc)
*         long  sydPlotSamples(pMstr, begin, end, incrFlag)
*         long  sydPlotSetAttr(pMstr, attr, value, pArg)
*		attr = SYD_PLATTR_{FG1,FG2,LINE,MARK,MONO,POINT,SHOW,UNDER,WRAP}
*         long  sydPlotSetTitles(pMstr, top, left, bottom, right) 
*         long  sydPlotWinLoop(pMstr)
*         long  sydPlotWinReplot(pMstr)
* BUGS
* o	sydPlotInitUW doesn't support SUNVIEW
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

/*+/subr**********************************************************************
* NAME	sydPlotAxisAutoRange - set axis ends to min and max data values
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
* EXAMPLE
*
*-*/
void
sydPlotAxisAutoRange(pSlave)
SYD_PL_SLAVE *pSlave;
{
    pSlave->originVal = pSlave->pSChan->minDataVal;
    pSlave->extentVal = pSlave->pSChan->maxDataVal;
}

/*+/subr**********************************************************************
* NAME	sydPlotAxisSetAttr - set plot axis attributes
*
* DESCRIPTION
*	Setting an attribute doesn't automatically reset other related
*	attributes.
*
*	sydPlotAxisSetAttr(pSlave, SYD_PLATTR_XCHAN, {0,1}, NULL)
*	sydPlotAxisSetAttr(pSlave, SYD_PLATTR_BG, 0, pBgPixelValue)
*	sydPlotAxisSetAttr(pSlave, SYD_PLATTR_FG, 0, pFgPixelValue)
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
*
* RETURNS
*	void
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
void
sydPlotAxisSetup(pSlave)
SYD_PL_SLAVE *pSlave;
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
}

/*+/subr**********************************************************************
* NAME	sydPlotChanAdd - add a slave
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
SYD_PL_MSTR *pMstr;	/* IO pointer to plot master */
SYD_CHAN *pSChan;	/* I pointer to synchronous data channel structure */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to slave structure */
    int		i;

    assert(pMstr != NULL);

    if (dbr_type_is_STRING(pSChan->dbrType)) {
	(void)printf("sydPlotChanAdd: can't plot DBF_STRING values\n");
	return NULL;
    }
    if ((pSlave = (SYD_PL_SLAVE *)GenMalloc(sizeof(SYD_PL_SLAVE))) == NULL) {
	(void)printf("sydPlotChanAdd: can't get memory\n");
	return NULL;
    }

    pSlave->pSChan = pSChan;
    pMstr->nSlaves++;
    pSlave->markNum = pMstr->nSlaves - 1;
    pSlave->lineKey = pMstr->nSlaves;
    pSlave->timeLabel[0] = '\0';
    pSlave->xChan = 0;
    pSlave->pArea = NULL;
    pSlave->fg = 0;
    pSlave->bg = 0;
    pSlave->first = 1;
    pSlave->xFracLeft = 0.;
    pSlave->yFracBot = 0.;
    pSlave->xFracRight = 0.;
    pSlave->yFracTop = 0.;

    DoubleListAppend(pSlave, pMstr->pHead, pMstr->pTail);
    sydPlotAxisSetup(pSlave);
    if (dbr_type_is_ENUM(pSChan->dbrType)) {
	for (i=0; i<=pSlave->extentVal; i++)
	    pSlave->pAnnot[i] = pSChan->grBuf.genmval.strs[i];
	pSlave->ppAnnot = pSlave->pAnnot;
    }
    else
	pSlave->ppAnnot = NULL;
    (void)sprintf(pSlave->timeLabel, "sec past %s", pMstr->refText);

    return pSlave;
}

/*+/subr**********************************************************************
* NAME	sydPlotDone - multiple x vs y rundown
*
* DESCRIPTION
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
	    GenFree((char *)pSlave1);
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
sydPlotEraseSamples(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */

    assert(pMstr != NULL);

    pSlave = pMstr->pHead;
    while (pSlave != NULL) {
	pSlave->first = 1;
	if (pSlave->pArea != NULL) {
	    pprAreaErase(pSlave->pArea, 1., 1., -1., -1.);
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
*	use sydPlotWinLoop for actual plotting
*	`fullInit' results in initializing for a default window size and
*		position.  If fullInit is zero, then the size and position
*		in the plot master are used.
*
* RETURNS
*	S_syd_OK
*
* BUGS
* o	need an sdrXxx call to initialize a master with caller-supplied
*	or default size and position
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
long
sydPlotInit(pMstr, pSspec, winType, dispName, winTitle, fullInit)
SYD_PL_MSTR *pMstr;	/* IO pointer to plot master structure */
SYD_SPEC *pSspec;	/* I pointer to synchronous set spec */
PPR_WIN_TY winType;	/* I type of "plot window": PPR_WIN_xxx */
char	*dispName;	/* I name of "plot window"--display, PostScript
			    file, EPS file, or NULL */
char	*winTitle;	/* I title for window title bar and icon */
int	fullInit;	/* I 0 or 1 to do partial or full initialization */
{
    if (fullInit) {
	pMstr->pWin = pprWinOpen(winType, dispName, winTitle, 0,0,0,0);
	assertAlways(pMstr->pWin != NULL);
	pprWinInfo(pMstr->pWin, &pMstr->x, &pMstr->y, &pMstr->width,
							&pMstr->height);
    }
    else {
	pMstr->pWin = pprWinOpen(winType, dispName, winTitle, 
			pMstr->x, pMstr->y, pMstr->width, pMstr->height);
	assertAlways(pMstr->pWin != NULL);
    }

    pMstr->winType = winType;
    pMstr->plotAxis = SYD_PLAX_UNDEF;
    pMstr->pSspec = pSspec;
    sydPlotInit_common(pMstr);
    return S_syd_OK;
}
static void
sydPlotInit_common(pMstr)
SYD_PL_MSTR *pMstr;
{
    pMstr->linePlot = 1;
    pMstr->pointPlot = 0;
    pMstr->markPlot = 0;
    pMstr->showStat = 0;
    pMstr->fillUnder = 0;
    if (pprWinIsMono(pMstr->pWin))
	pMstr->noColor = 1;
    else
	pMstr->noColor = 0;
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
    pMstr->refText[0] = '\0';
    pMstr->pHead = NULL;
    pMstr->pTail = NULL;
    pMstr->nSlaves = 0;
    pMstr->originVal = 0.;
    pMstr->extentVal = 0.;
    pMstr->wrapX = 0;
    if (pMstr->pSspec->sampleCount >= 1) {
	pMstr->extentVal =
		pMstr->pSspec->pDeltaSec[pMstr->pSspec->sampleCount-1];
    }
}

#ifdef XWINDOWS
/*+/subr**********************************************************************
* NAME	sydPlotInitUW - plotting initialization, using User Window
*
* DESCRIPTION
*	use sydPlotWinReplot for actual plotting
*	sydPlotSamples can be used for incremental plotting
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
sydPlotInitUW(pMstr, pSspec, pDisp, window, gc)
SYD_PL_MSTR *pMstr;	/* IO pointer to plot master structure */
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
    pMstr->pDisp = pDisp;
    pMstr->window = window;

    return S_syd_OK;
}
#endif XWINDOWS

/*+/subr**********************************************************************
* NAME	sydPlotSamples - plot one or more sync samples
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
*	sydPlotSetAttr(pMstr, SYD_PLATTR_LINE, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_MARK, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_MONO, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_POINT, {0,1}, NULL)
*	sydPlotSetAttr(pMstr, SYD_PLATTR_SHOW, {0,1}, NULL)
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
* o	text
*
* SEE ALSO
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
* NAME	sydPlotWinLoop - do the actual plotting
*
* DESCRIPTION
*	for use with sydPlotInit
*
* RETURNS
*	OK, or
*	ERROR
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
sydPlotWinLoop(pMstr)
SYD_PL_MSTR *pMstr;	/* IO pointer to plot master structure */
{
    SYD_PLAX pltTy = pMstr->plotAxis;		/* type of plot desired */
    long	stat;
    char	refText[28];
    int		npts;
    SYD_SPEC	*pSspec = pMstr->pSspec;

    npts = pSspec->sampleCount;
    if (npts > 1 && pMstr->originVal != pMstr->extentVal) {
	pprAutoEnds(pSspec->pDeltaSec[0], pSspec->pDeltaSec[npts-1],
					&pMstr->originVal, &pMstr->extentVal);
	pprAutoInterval(pMstr->originVal, pMstr->extentVal, &pMstr->nInt);
	(void)tsStampToText(&pSspec->refTs, TS_TEXT_MMDDYY, refText);
	(void)sprintf(pMstr->label, "sec past %s", refText);
	(void)strcpy(pMstr->refText, refText);
    }
    else {
	pMstr->originVal = 0.;
	pMstr->extentVal = 100.;
	strcpy(pMstr->label, "elapsed seconds");
	pMstr->refText[0] = '\0';
	pMstr->nInt = 5;
    }

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
*	for use with sydPlotInitUW
*
* RETURNS
*	OK, or
*	ERROR
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
sydPlotWinReplot(pMstr)
SYD_PL_MSTR *pMstr;	/* IO pointer to plot master structure */
{
    char	refText[28];
    int		npts;
    SYD_SPEC	*pSspec = pMstr->pSspec;

    npts = pSspec->sampleCount;
    if (npts > 1 && pMstr->originVal != pMstr->extentVal) {
	pprAutoEnds(pSspec->pDeltaSec[0], pSspec->pDeltaSec[npts-1],
					&pMstr->originVal, &pMstr->extentVal);
	pprAutoInterval(pMstr->originVal, pMstr->extentVal, &pMstr->nInt);
	(void)tsStampToText(&pSspec->refTs, TS_TEXT_MMDDYY, refText);
	(void)sprintf(pMstr->label, "sec past %s", refText);
	(void)strcpy(pMstr->refText, refText);
    }
    else {
	pMstr->originVal = 0.;
	pMstr->extentVal = 100.;
	strcpy(pMstr->label, "elapsed seconds");
	pMstr->refText[0] = '\0';
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
*
* RETURNS
*
* BUGS
* o	text
*
* SEE ALSO
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
    else if (pMstr->plotAxis == SYD_PLAX_SMITH_IMP ||
    			pMstr->plotAxis == SYD_PLAX_SMITH_ADM ||
    			pMstr->plotAxis == SYD_PLAX_SMITH_IMM)
	sydPlot_SmithPlot(pMstr);
    else
	assertAlways(0);
}

/*/subhead --------------------------------------------------------------------
*
*----------------------------------------------------------------------------*/
#define FetchIthValInto(pSChan, dbl) \
    if (dbr_type_is_FLOAT(pSChan->dbrType)) \
	dbl = (double)((float *)pSChan->pData)[i]; \
    else if (dbr_type_is_SHORT(pSChan->dbrType)) \
	dbl = (double)((short *)pSChan->pData)[i]; \
    else if (dbr_type_is_DOUBLE(pSChan->dbrType)) \
	dbl = (double)((double *)pSChan->pData)[i]; \
    else if (dbr_type_is_LONG(pSChan->dbrType)) \
	dbl = (double)((long *)pSChan->pData)[i]; \
    else if (dbr_type_is_CHAR(pSChan->dbrType)) \
	dbl = (double)((char *)pSChan->pData)[i]; \
    else if (dbr_type_is_ENUM(pSChan->dbrType)) \
	dbl = (double)((short *)pSChan->pData)[i]; \
    else \
	assertAlways(0);


/*+/subr**********************************************************************
* NAME	sydPlot_setup - set up titles and margins for a plot window
*
* DESCRIPTION
*	Plots whatever titles are present in the plot master, reserving
*	an appropriate margin when necessary.  The caller is given the
*	appropriate coordinates and sizes for the sub-plots.
*
* RETURNS
*	void
*
* BUGS
* o	handles only vertical subdividing of the plot window
*
*-*/
static void
sydPlot_setup(pMstr, nGrids, pXlo, pYlo, pXhi, pYhi, pYpart, pCh, pChX)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
double	*pXlo, *pYlo, *pXhi, *pYhi, *pYpart, *pCh, *pChX;
int	nGrids;
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    double	xlo, ylo, xhi, yhi, yPart, charHt, charHtX;
    PPR_AREA	*pArea;
    TS_STAMP	now;
    char	nowText[32];

    pWin = pMstr->pWin;
    xlo = 0.;
    xhi = .98;
    ylo = 0.;
    yhi = .98;
    charHt = .012;
    charHtX = pprYFracToXFrac(pWin, charHt);
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
	yhi = 1. - charHt;
	pprText(pArea, .5, yhi, pMstr->title, PPR_TXT_CEN, charHt, 0.);
	yhi -= 2. * charHt;
    }
    if (strlen(pMstr->lTitle) > 0) {
	xlo = 2. * charHtX;
	pprText(pArea, xlo, .5, pMstr->lTitle, PPR_TXT_CEN, charHt, 90.);
	xlo += 2. * charHtX;
    }
    if (strlen(pMstr->bTitle) > 0) {
	ylo = 2. * charHt;
	pprText(pArea, .5, ylo, pMstr->bTitle, PPR_TXT_CEN, charHt, 0.);
	ylo += 2. * charHt;
    }
    if (strlen(pMstr->rTitle) > 0) {
	xhi = 1. - 2. * charHtX;
	pprText(pArea, xhi, .5, pMstr->rTitle, PPR_TXT_CEN, charHt, 90.);
	xhi -= 2. * charHtX;
    }
    pprAreaClose(pArea);

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
    sydPlot_SmithSamples(pMstr, pSspec->firstData, pSspec->lastData, 0);
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
    double	charHt, charHtX;
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    SYD_PL_SLAVE *pSlaveX;	/* pointer to X axis slave struct */

    pWin = pMstr->pWin;

    sydPlot_setup(pMstr, 1, &xlo, &ylo, &xhi, &yhi, &yPart, &charHt, &charHtX);

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
    sydPlot_TYSamples(pMstr, pSspec->firstData, pSspec->lastData, 0);
}

/*+/subr**********************************************************************
* NAME	sydPlot_TYGrid - draw a grid for a time vs Y plot
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
    int		xNint;
    double	charHt, charHtX;
    SYD_CHAN	*pSChan;
    TS_STAMP	now;
    char	nowText[32];
    int		thick=3;
    int		nGrids;

    pWin = pMstr->pWin;

    nGrids = pMstr->nSlaves;
    sydPlot_setup(pMstr, nGrids,
			&xlo, &ylo, &xhi, &yhi, &yPart, &charHt, &charHtX);

    xmin = pMstr->originVal;
    xmax = pMstr->extentVal;
    xNint = pMstr->nInt;
    if (xmin == xmax) {
	xmin = 0.;
	xmax = 100.;
	xNint = 5;
    }
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
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin,
		xlo+12.*charHtX, ylo+6.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, xNint, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo + 12. * charHtX;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo + 6. * charHt;
	pSlave->yFracTop = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    if (dbr_type_is_ENUM(pSChan->dbrType))
		pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	}
	pprGridLabel(pArea, pMstr->label, NULL,
				pSlave->pSChan->label, pSlave->ppAnnot, 0.);
	ylo += yPart;
	yhi += yPart;
	pSlave = pSlave->pNext;
    }
}

/*+/subr**********************************************************************
* NAME	sydPlot_TYSamples - plot one or more samples for a time vs Y plot
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
	    if (pSChan->pFlags[i].missing)
		skip = 1;
	    else if (first || skip || pSChan->pFlags[i].restart) {
		oldX = pSspec->pDeltaSec[i];
		if (pMstr->wrapX) {
		    while (oldX > pMstr->extentVal)
			oldX -= pMstr->extentVal;
		}
		FetchIthValInto(pSChan, oldY)
		if (markPlot)
		    pprMarkD(pArea, oldX, oldY, markNum);
		if (showStat && pSChan->pDataCodeR[i] != ' ') {
		    pprChar(pArea, oldX, oldY, pSChan->pDataCodeR[i], 0., 0.);
		}
		else if (pointPlot)
		    pprPointD(pArea, oldX, oldY);
		skip = 0;
	    }
	    else if (pSChan->pFlags[i].filled)
		;	/* no action */
	    else {
		newX = pSspec->pDeltaSec[i];
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
		    pprChar(pArea, newX, newY, pSChan->pDataCodeR[i], 0., 0.);
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
	pSlave->first = first;
	pSlave->oldX = oldX;
	pSlave->oldY = oldY;
	pSlave->skip = skip;
	pSlave = pSlave->pNext;
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
    sydPlot_TYSamples(pMstr, pSspec->firstData, pSspec->lastData, 0);
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
* EXAMPLE
*
*-*/
void
sydPlot_TYYGrid(pMstr)
SYD_PL_MSTR *pMstr;	/* I pointer to master plot structure */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */
    SYD_PL_SLAVE *pSlave;	/* pointer to individual slave struct */
    double	xlo, ylo, xhi, yhi, yPart;
    PPR_AREA	*pArea;
    double	xmin, xmax, ymin, ymax;
    int		xNint;
    double	charHt, charHtX;
    SYD_CHAN	*pSChan;
    TS_STAMP	now;
    char	nowText[32];
    int		offsetAnnotY=0;
    int		drawAxis=0;
    int		thick=3;
    int		nGrids;

    pWin = pMstr->pWin;

    nGrids = 1;
    sydPlot_setup(pMstr, nGrids,
			&xlo, &ylo, &xhi, &yhi, &yPart, &charHt, &charHtX);
    xlo += 6. * charHtX * (double)pMstr->nSlaves;
    ylo += 6. * charHt;

    xmin = pMstr->originVal;
    xmax = pMstr->extentVal;
    xNint = pMstr->nInt;
    if (xmin == xmax) {
	xmin = 0.;
	xmax = 100.;
	xNint = 5;
    }
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
	pArea = pSlave->pArea = pprAreaOpen(pWin, xlo, ylo, xhi, yhi,
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
	    pprAnnotX_wc(pArea, 0,
				xmin, xmax, xNint, 0, pMstr->label, NULL, 0.);
	}
	pprAnnotY(pArea, offsetAnnotY, pSlave->originVal, pSlave->extentVal,
	    			pSlave->nInt, drawAxis,
				pSlave->pSChan->label, pSlave->ppAnnot, 90.);
	if (pMstr->markPlot)
	    pprAnnotYMark(pArea, offsetAnnotY, pSlave->markNum);
	offsetAnnotY += 6;
	drawAxis = 1;		/* draw an "auxiliary" axis next time */
	pSlave = pSlave->pNext;
    }
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
    sydPlot_XYSamples(pMstr, pSspec->firstData, pSspec->lastData, 0);
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
    double	charHt, charHtX;
    SYD_CHAN	*pSChan;
    SYD_CHAN	*pSChanX;
    int		nGrids;

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
    sydPlot_setup(pMstr, nGrids,
			&xlo, &ylo, &xhi, &yhi, &yPart, &charHt, &charHtX);

    pSlaveX->pArea = NULL;
    pSChanX = pSlaveX->pSChan;
    xmin = pSlaveX->originVal;
    xmax = pSlaveX->extentVal;
    while (pSlave != NULL) {
/*-----------------------------------------------------------------------------
*    for each Y channel, plot a perimeter with grid lines
*----------------------------------------------------------------------------*/
	pSChan = pSlave->pSChan;
	ymin = pSlave->originVal;
	ymax = pSlave->extentVal;
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin,
		xlo+12.*charHtX, ylo+6.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, pSlaveX->nInt, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo + 12. * charHtX;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo + 6. * charHt;
	pSlave->yFracTop = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	pprGridLabel(pArea, pSlaveX->pSChan->label, NULL,
				pSlave->pSChan->label, pSlave->ppAnnot, 0.);
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
	    if (pSChan->pFlags[i].missing || pSChanX->pFlags[i].missing)
		skip = 1;
	    else if (first || skip ||
		    pSChan->pFlags[i].restart || pSChanX->pFlags[i].restart) {
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
	    else if (pSChan->pFlags[i].filled)
		;	/* no action */
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
	pSlave->first = first;
	pSlave->oldX = oldX;
	pSlave->oldY = oldY;
	pSlave->skip = skip;
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
    sydPlot_XYSamples(pMstr, pSspec->firstData, pSspec->lastData, 0);
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
    double	charHt, charHtX;
    SYD_CHAN	*pSChan;
    SYD_CHAN	*pSChanX;
    TS_STAMP	now;
    char	nowText[32];
    int		offsetAnnotY=0;
    int		drawAxis=0;
    int		nGrids;

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
    sydPlot_setup(pMstr, nGrids,
			&xlo, &ylo, &xhi, &yhi, &yPart, &charHt, &charHtX);
    xlo += 6. * charHtX * (double)(pMstr->nSlaves - 1);
    ylo += 6. * charHt;

    pSlaveX->pArea = NULL;
    xmin = pSlaveX->originVal;
    xmax = pSlaveX->extentVal;
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
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
        pArea = pSlave->pArea = pprAreaOpen(pWin, xlo, ylo, xhi, yhi,
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
	    pprAnnotX_wc(pArea, 0, pSlaveX->originVal, pSlaveX->extentVal,
		pSlaveX->nInt, 0, pSlaveX->pSChan->label, NULL, 0.);
	}
	pprAnnotY(pArea, offsetAnnotY, pSlave->originVal, pSlave->extentVal,
	   pSlave->nInt, drawAxis, pSlave->pSChan->label, pSlave->ppAnnot, 90.);
	if (pMstr->markPlot)
	    pprAnnotYMark(pArea, offsetAnnotY, pSlave->markNum);
	offsetAnnotY += 6;
	drawAxis = 1;		/* draw an "auxiliary" axis next time */
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
    sydPlot_YSamples(pMstr, pSspec->firstData, pSspec->lastData, 0);
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
    double	charHt, charHtX;
    SYD_CHAN	*pSChan;
    TS_STAMP	now;
    char	nowText[32];
    int		thick=3;
    int		nGrids;

    pWin = pMstr->pWin;

    nGrids = pMstr->nSlaves;
    sydPlot_setup(pMstr, nGrids,
			&xlo, &ylo, &xhi, &yhi, &yPart, &charHt, &charHtX);

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
	if (pSlave->pArea != NULL)
	    pprAreaClose(pSlave->pArea);
	pArea = pSlave->pArea = pprAreaOpen(pWin,
		xlo+12.*charHtX, ylo+6.*charHt, xhi, yhi,
		xmin, ymin, xmax, ymax, xNint, pSlave->nInt, charHt);
	assertAlways(pArea != NULL);
	pSlave->xFracLeft = xlo + 12. * charHtX;
	pSlave->xFracRight = xhi;
	pSlave->yFracBot = ylo + 6. * charHt;
	pSlave->yFracTop = yhi;
	if (pSlave->fg != 0 && pMstr->noColor == 0)
	    pprAreaSetAttr(pSlave->pArea, PPR_ATTR_FG, 0, &pSlave->fg);
	else if (pMstr->linePlot) {
	    if (dbr_type_is_ENUM(pSChan->dbrType))
		pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	}
	pprGridLabel(pArea, "", NULL,
				pSlave->pSChan->label, pSlave->ppAnnot, 0.);
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
	    if (pSChan->pFlags[i].missing)
		skip = 1;
	    else if (first || skip || pSChan->pFlags[i].restart) {
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
	    else if (pSChan->pFlags[i].filled)
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
	pSlave->first = first;
	pSlave->oldX = oldX;
	pSlave->oldY = oldY;
	pSlave->skip = skip;
	pSlave = pSlave->pNext;
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
    sydPlot_YSamples(pMstr, pSspec->firstData, pSspec->lastData, 0);
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
    double	charHt, charHtX;
    SYD_CHAN	*pSChan;
    TS_STAMP	now;
    char	nowText[32];
    int		offsetAnnotY=0;
    int		drawAxis=0;
    int		thick=3;
    int		nGrids;

    pWin = pMstr->pWin;

    nGrids = 1;
    sydPlot_setup(pMstr, nGrids,
			&xlo, &ylo, &xhi, &yhi, &yPart, &charHt, &charHtX);
    xlo += 6. * charHtX * (double)pMstr->nSlaves;
    ylo += 6. * charHt;

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
	pArea = pSlave->pArea = pprAreaOpen(pWin, xlo, ylo, xhi, yhi,
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
	    pprAnnotX_wc(pArea, 0, xmin, xmax, xNint, 0, "", NULL, 0.);
	}
	pprAnnotY(pArea, offsetAnnotY, pSlave->originVal, pSlave->extentVal,
	    			pSlave->nInt, drawAxis,
				pSlave->pSChan->label, pSlave->ppAnnot, 90.);
	if (pMstr->markPlot)
	    pprAnnotYMark(pArea, offsetAnnotY, pSlave->markNum);
	offsetAnnotY += 6;
	drawAxis = 1;		/* draw an "auxiliary" axis next time */
	pSlave = pSlave->pNext;
    }
}
