/*	@(#)pprPlot.c	1.5 8/24/92
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
 *  .02 07-20-91 rac	installed in SCCS; finalized "how to access"
 *  .03 08-09-91 rac	fix a problem with pprMark when clipping
 *  .04 09-04-91 rac	change pprAreaErase to pprRegionErase; add
 *			pprAreaErase; update documentation; add pprPixXToXFrac
 *			and pprPixYToYFrac; add various Line and Point erase
 *			functions
 *  .05 05-15-92 rac	add pprErrorBar; add line caps as special marks for
 *			pprMark
 *  .06 09-07-92 rac	handle sub-intervals; change pixel coordinates to
 *			short integers
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DSUNVIEW	to use SunView window system
 *	-DXWINDOWS	to use X window system
 *	-DNDEBUG	don't compile assert() checking
 *	-DPPR_TEST	compile the embedded main program for testing
 *
 *	UW		is an "edit time" switch which sets the test program
 *			to use pprWinOpenUW and play like a user program which
 *			does all its own windowing operations.
 */
/*+/mod***********************************************************************
* TITLE pprSubr.c - portable plotting routines
*
* DESCRIPTION
*	These routines support simple 2-D plotting in a "window".  The
*	window can be either an actual display window, a PostScript
*	printer page, or an EPS file for Encapsulated PostScript.
*
* QUICK REFERENCE
* PPR_AREA *pArea;     pointer to a plot area
*  PPR_WIN *pWin;      pointer to a plot "window"
*
*     void  pprAnnotX(       pArea,   offset,   dataL,    dataR,   nDiv,
*                                       drawLine, xLabel, xAnnot, angle   )
*     void  pprAnnotY(       pArea,   offset,   dataB,    dataT,   nDiv,
*                                       drawLine, yLabel, yAnnot, angle   )
*     void  pprAnnotYMark(   pArea,   offset,   markNum                   )
*     void  pprAreaClose(    pArea                                        )
*     void  pprAreaErase(    pArea    xDblLft,  yDblBot,  xDblRt,  yDblTop)
* PPR_AREA *pprAreaOpen(     pWin,    fracL,    fracB,    fracR,   fracT,
*                                     dataL,    dataB,    dataR,   dataT,
*                                               nXint,    nYint,   charHt )
* PPR_AREA *pprAreaOpenDflt( pWin,    fracL,    fracB,    fracR,   fracT,
*                                     dataL,    dataB,    dataR,   dataT  )
*     void  pprAreaRescale(  pArea,   dataL,    dataB,    dataR,   dataT  )
*     long  pprAreaSetAttr(  pArea,   code,     attrVal,  pAttr           )
*      code: PPR_ATTR_{CLIP,COLORNUM,BG,FG,KEYNUM,LINE_THICK,NOCLIP,PATT_ARRAY}
*     void  pprAutoEnds(     dbl1,    dbl2,    >pNewDbl1, >pNewDbl2       )
*     void  pprAutoInterval( dbl1,    dbl2,    >pNint                     )
*     void  pprAutoRangeD(   dblAry,  nPts,    >minDbl,  >maxDbl          )
*     void  pprAutoRangeF(   fltAry,  nPts,    >minDbl,  >maxDbl          )
*     void  pprAutoRangeL(   lngAry,  nPts,    >minDbl,  >maxDbl          )
*     void  pprAutoRangeS(   shtAry,  nPts,    >minDbl,  >maxDbl          )
*     void  pprChar(         pArea,   xDbl, yDbl, char, charHt, angle     )
*   double  pprCos_deg(      angle                                        )
*     void  pprErrorBar(     pArea,   xDbl1,    yDbl1,    xDbl2,  yDbl2   )
*     void  pprCvtDblToTxt( >text,    width,    dblVal,   nSigDigits      )
*     void  pprGrid(         pArea                                        )
*     void  pprGridErase(    pArea                                        )
*     void  pprGridLabel(    pArea,   xLabel, xAnnot, yLabel, yAnnot, angle)
*     void  pprLineD(        pArea,   xDblAry,  yDblAry,  nPts            )
*     void  pprLineF(        pArea,   xFltAry,  yFltAry,  nPts            )
*     void  pprLineL(        pArea,   xLngAry,  yLngAry,  nPts            )
*     void  pprLineS(        pArea,   xShtAry,  yShtAry,  nPts            )
*     void  pprLineEraseD(   pArea,   xDblAry,  yDblAry,  nPts            )
*     void  pprLineEraseF(   pArea,   xFltAry,  yFltAry,  nPts            )
*     void  pprLineEraseL(   pArea,   xLngAry,  yLngAry,  nPts            )
*     void  pprLineEraseS(   pArea,   xShtAry,  yShtAry,  nPts            )
*     void  pprLineSegD(     pArea,   xDbl1,    yDbl1,    xDbl2,  yDbl2   )
*     void  pprLineSegL(     pArea,   xLng1,    yLng1,    xLng2,  yLng2   )
*     void  pprLineSegEraseD(pArea,   xDbl1,    yDbl1,    xDbl2,  yDbl2   )
*     void  pprLineSegEraseL(pArea,   xLng1,    yLng1,    xLng2,  yLng2   )
*     void  pprMarkD(        pArea,   xDbl1,    yDbl1,    markNum         )
*     void  pprMarkL(        pArea,   xLng1,    yLng1,    markNum         )
*     void  pprMarkEraseD(   pArea,   xDbl1,    yDbl1,    markNum         )
*     void  pprMarkEraseL(   pArea,   xLng1,    yLng1,    markNum         )
*     void  pprMoveD(        pArea,   xDbl1,    yDbl1,    penDown         )
*     void  pprPerim(        pArea                                        )
*     void  pprPerimErase(   pArea                                        )
*     void  pprPerimLabel(   pArea,   xLabel, xAnnot, yLabel, yAnnot, angle)
*   double  pprPixXToXFrac(  pWin,    xPixel                              )
*   double  pprPixYToYFrac(  pWin,    yPixel                              )
*     void  pprPointD(       pArea,   xDbl1,    yDbl1                     )
*     void  pprPointL(       pArea,   xLng1,    yLng1                     )
*     void  pprPointEraseD(  pArea,   xDbl1,    yDbl1                     )
*     void  pprPointEraseL(  pArea,   xLng1,    yLng1                     )
*     void  pprRegionErase(  pArea    fracL,    fracB,    fracR,   fracT  )
*   double  pprSin_deg(      angle                                        )
*     void  pprText(         pArea,   xDbl, yDbl, text, just, charHt, angle)
*				just: PPR_TXT_{CEN,RJ,LJ}
*     void  pprTextErase(    pArea,   xDbl, yDbl, text, just, charHt, angle)
*     void  pprWinClose(     pWin                                         )
*     void  pprWinErase(     pWin                                         )
*     void  pprWinInfo(      pWin,   >pXpos,   >pYpos,   >pXwid,   >pYht  )
*      int  pprWinIsMono(    pWin                                         )
*     long  pprWinLoop(      pWin,    drawFn,   pDrawArg                  )
*               void  drawFn(pWin, pDrawArg)
*  PPR_WIN *pprWinOpen(      winType, dispName, winTitle, xPos, yPos, xWid,yHt)
*                               winType: PPR_WIN_{SCREEN,POSTSCRIPT,EPS}
*  PPR_WIN *pprWinOpenUW(    pFrame,  pCanvas,  NULL,     NULL            )
*  PPR_WIN *pprWinOpenUW(    ppDisp,  pWindow,  pGC,      NULL            )
*     void  pprWinReplot(    pWin,    drawFn,   pDrawArg                  )
*               code: PPR_ATTR_{COLORNUM,GC,KEYNUM,LINE_THICK,PATT_ARRAY}
*   double  pprYFracToXFrac( pWin,    yFrac                               )
*   
* DESCRIPTION (continued)
*	Plotting is done within "plot areas" which are defined within
*	the window.  Plot areas can be as large as the window, and they
*	can overlap, if desired.  Clipping service is optional, since
*	plot areas are often calibrated for the "normal" range of data,
*	and it is useful to see what's happening if the data are outside
*	the normal range.
*
*	One non-intuitive aspect of the term "plot area" is that the
*	usual case ALWAYS plots outside the plot area.  Generally, this
*	happens when annotations and labels are added to a grid.  The
*	plot area itself only specified where the edges of the grid were
*	to be drawn.  Because of this, determining the size and placement
*	of a plot area must take into account the necessary margins for
*	text.  (pprAreaOpenDflt automatically takes care of margins.)
*
*	Most plotting is done using "data coordinates", which are the
*	actual data values.  In some cases, coordinates or distances are
*	specified in terms of a fraction of the window height (see, for
*	example, pprAreaOpen or pprText).
*
*	Also provided in this package are some routines for interacting
*	with the "window".  In many cases, this means that a plotting
*	program can totally avoid any knowledge of, or dependence on, a
*	particular windowing environment.  This makes easily available
*	the capability for producing hard copies of plots on a PostScript
*	printer.
*
*	Many routines in this package require that values be
*	represented with type 'double'.  In some cases, however, other
*	data types are directly supported, so that the caller doesn't
*	need to alter the way data are stored.
*
* BUGS
* o	Only linear axes are presently supported
* o	The SunView version of this package won't run properly with
*	programs which use the SunOs LightWeight Process library.
* o	The SunView version of this package doesn't handle color.
*
* EXAMPLE
*	The following program plots the first 80 points of a parabola.  The
*	size and position of the plot window can be freely changed; the
*	window can be covered and exposed, iconified and de-iconified, etc.
*	When a `click right' is done in the plot window, the window is
*	closed.  A PostScript file named "testPS" is produced; the size of
*	the plot in this file is about the same as it was when the plot
*	window was closed.
*
*	The program is compiled and linked for X11 with the following
*	command.  (If making to run on sun3, use lib.sun3 in the command.)
*
*          % cc plotTest.c -I~epics/epicsH -I$(OPENWINHOME)/include \
*			-L~epics/share/bin/lib.sun4 -L$(OPENWINHOME)/lib \
*			-lppr -lX -lm
*
*   #include <stdio.h>
*   #include <pprPlotDefs.h>
*
*   #define NPTS 80
* ----------------------------------------------------------------------------
*    define a structure for holding the data.  This is needed because the
*    replot routine (which does the actual plotting) is only given a pointer
*    to the information it needs.
* ----------------------------------------------------------------------------
*   typedef struct {
*       int     nPts;
*       float   x[NPTS];
*       float   y[NPTS];
*       double  xMin;
*       double  xMax;
*       double  yMin;
*       double  yMax;
*   } MY_DATA;
*
*   void replot();
*
*   main()
*   {
*       int     i;
*       int     x=0,y=0,width=0,height=0;
*       PPR_WIN	*pWin;
*       MY_DATA	myData;
*       long	stat;
*
* ----------------------------------------------------------------------------
*    generate the data in the structure.  Once it's generated, figure out
*    the range of values and put the range into the structure using
*    pprAutoRangeF.  This specific routine was chosen because the data
*    is in "float" arrays.  Different forms of data would result in choosing
*    a different routine.
*
*    The routine pprAutoEnds is available to "round" the endpoints of the
*    axes to "nicer" values.  Depending on the application, it will be
*    attractive to use this following the pprAutoRange call.
* ----------------------------------------------------------------------------
*       myData.nPts = NPTS;
*       for (i=0; i<NPTS; i++) {
*           myData.x[i] = (float)i;
*           myData.y[i] = (float)(i*i);
*       }
*       pprAutoRangeF(myData.x, NPTS, &myData.xMin, &myData.xMax);
*       pprAutoRangeF(myData.y, NPTS, &myData.yMin, &myData.yMax);
* 
* ----------------------------------------------------------------------------
*    Now, do the actual plotting, in a roundabout way.  First, an actual
*    window on the screen is created with the pprWinOpen call.  The
*    size and position of the window are determined by the values
*    (initialized above) of x, y, width, and height.
*
*    After opening the window, pprWinLoop is called to accomplish the
*    actual plotting.  This routine calls the replot() routine whenever it
*    is necessary to replot the data.  This will happen immediately after
*    pprWinLoop is called, and also any time the plot window is resized
*    or re-exposed after being fully or partially covered.  pprWinLoop
*    doesn't return until a "click right" over the plot (or a window menu
*    quit) is done.
*
*    After pprWinLoop returns, pprWinInfo is called to get the most
*    recent size and position of the window.  If this program were going
*    to put up another plot, this information would allow being polite
*    and having the window for the new plot cover the same part of the
*    screen.  In this example program, the only use being made of the
*    information is to control the size of the PostScript plot.
*
*    Finally, the plot window is closed using pprWinClose.  It's important
*    not to skip this step.
* ----------------------------------------------------------------------------
*       pWin = pprWinOpen(PPR_WIN_SCREEN, NULL, "test title",
*                                                   x, y, width, height);
*       if (pWin == NULL) abort();
*       stat = pprWinLoop(pWin, replot, &myData);
*       if (stat != 0) abort();
*       pprWinInfo(pWin, &x, &y, &width, &height);
*       pprWinClose(pWin);
*
* ----------------------------------------------------------------------------
*    Produce a file containing a PostScript version of the plot.  Because
*    of the information obtained by pprWinInfo, this will be about the
*    same size as the plot last appeared on the screen.
* ----------------------------------------------------------------------------
*       pWin = pprWinOpen(PPR_WIN_POSTSCRIPT, "testPS", NULL, x,y,width,height);
*       if (pWin == NULL) abort();
*       stat = pprWinLoop(pWin, replot, &myData);
*       if (stat != 0) abort();
*       pprWinClose(pWin);
*
*       return;
*   }
*
* ----------------------------------------------------------------------------
*	D R A W   F U N C T I O N
* ----------------------------------------------------------------------------
*
*   void replot(pWin, pMyData)
*   PPR_WIN	*pWin;
*   MY_DATA	*pMyData;
*   {
*       double  xlo=0., ylo=0., xhi=.95, yhi=.95;
*       double  charHt, charHtX;
*       PPR_AREA *pArea;
*       int     i;
*
* ----------------------------------------------------------------------------
*    First, figure out how big to make the characters for axis labels and
*    annotations.  The character size will be used to specify the left and
*    bottom margins for the actual area used for plotting.  (See pprText
*    for a discussion of PprDfltCharHt.)
*
*    The pprAreaOpen call establishes a plot area that will be used for
*    the actual plotting.  A left margin of 12 lines and a bottom margin
*    of 6 lines are specified.  The top margin is .05 of the window height,
*    and the right margin is .05 of the window width.
*
*    A perimeter with tick marks is drawn, and annotations and labels are
*    applied, with the pprPerimLabel call.
*
*    The actual plotting of data is done with pprLineF.  This specific
*    routine was chosen because the data is in "float" arrays.  Different
*    forms of data would result in choosing a different routine.
*
*    Finally, the plot area is closed with pprAreaClose.  It's important
*    not to skip this step.
* ----------------------------------------------------------------------------
*       charHt = PprDfltCharHt(ylo, yhi);
*       charHtX = pprYFracToXFrac(pWin, charHt);
*       pArea = pprAreaOpen(pWin, xlo+12.*charHtX, ylo+6.*charHt, xhi, yhi,
*               pMyData->xMin, pMyData->yMin, pMyData->xMax, pMyData->yMax,
*               5, 5, charHt);
*       pprPerimLabel(pArea, "x label", NULL, "y label", NULL, 0.);
*
*       pprLineF(pArea, pMyData->x, pMyData->y, pMyData->nPts);
*
*       pprAreaClose(pArea);
*
*       return;
*   }
*
*-***************************************************************************/

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

#define PPR_PRIVATE
#include <pprPlotDefs.h>

void pprAnnotX_gen();
void pprAnnotY_gen();
void pprArcD_gen();
void pprLineSegPixD_ac();
void pprLineSegPixD_wc();
void pprLineSegDashD_wc();
void pprText_gen();


/*-----------------------------------------------------------------------------
* assert macros, so that this package can be portable without having to link
*	to genLib.a
*----------------------------------------------------------------------------*/

#ifndef NDEBUG
#   define PprAssert(expr) ((void)((expr) || pprAssertFail(__FILE__, __LINE__)))
#else
#   define PprAssert(expr) ((void)0)
#endif
#define PprAssertAlways(expr) \
		((void)((expr) || pprAssertFail(__FILE__, __LINE__)))
pprAssertFail(fileName, lineNum)
char *fileName;
int lineNum;
{
    (void)fprintf(stderr, "pprAssertFail: in file %s line %d\n",
						fileName, lineNum);
#ifdef vxWorks
    if (kill(taskIdSelf(), SIGUSR1) == ERROR) {
	int	*j;
	j = (int *)(-1);
	j = (int *)(*j);
    }
    exit(1);
#else
    abort();
#endif
}

/*-----------------------------------------------------------------------------
* doubly linked list macros
*----------------------------------------------------------------------------*/
#define DoubleListAppend(pItem,pHead,pTail) \
{\
    pItem->pNext = NULL;\
    pItem->pPrev = pTail;\
    if (pTail != NULL)\
        pTail->pNext = pItem;         /* link previous tail to here */\
    pTail = pItem;\
    if (pHead == NULL)\
        pHead = pItem;                /* link to head if first item */\
}

#define DoubleListRemove(pItem,pHead,pTail) \
{\
    if (pItem->pPrev != NULL)\
        (pItem->pPrev)->pNext = pItem->pNext;   /* link prev to next */\
    else\
        pHead = pItem->pNext;                   /* link list head to next */\
    if (pItem->pNext != NULL)\
        (pItem->pNext)->pPrev = pItem->pPrev;   /* link next to prev */\
    else\
        pTail = pItem->pPrev;           /* link list tail to prev */\
    pItem->pNext = NULL;\
    pItem->pPrev = NULL;\
}

/*/subhead pprTest-------------------------------------------------------------
*
*----------------------------------------------------------------------------*/
#ifdef PPR_TEST

#ifndef vxWorks
    main() { pprTest(); }
#endif
#define NPTS 80
typedef struct {
    double	xMin;
    double	xMax;
    double	yMin;
    double	yMax;
    int		nPts;
    float	x[NPTS];
    float	y[NPTS];
} MY_DATA;

void replot();

#ifdef SUNVIEW
struct pprWinWin {
    Frame	frame;
    PPR_WIN	*pWin1;
    PPR_WIN	*pWin2;
    void	*pMyData;
};
#endif

#define UW
#undef UW
pprTest()
{
    int		i;
    int		x=0,y=0,width=0,height=0;
    PPR_WIN	*pWin, *pWin2;
    MY_DATA	myData;
    long	stat;
#ifdef SUNVIEW
    Frame	plotFrame;
    Canvas	plotCanvas, plotCanvas1, plotCanvas2;
    struct pprWinWin winList;
    void	pprTestEvHandler();
#elif defined XWINDOWS
    Display	*pDisp;		/* pointer to X server connection */
    int		screenNo;	/* screen number */
    Window	rootWindow;	/* the root window on display */
    GC		gc;		/* graphics context */
    XEvent	anEvent;	/* a window event structure */
    Window	plotWindow;	/* the plot window on the display */
    Window	subWin1, subWin2;
    XSizeHints	sizeHints;	/* defaults for position and size */
#endif

    for (i=0; i<NPTS; i++) {
	myData.x[i] = (float)i;
	myData.y[i] = (float)(i*i);
    }
    pprAutoRangeF(myData.x, NPTS, &myData.xMin, &myData.xMax);
    pprAutoRangeF(myData.y, NPTS, &myData.yMin, &myData.yMax);
    myData.nPts = NPTS;

#if defined XWINDOWS &&  defined UW
/*-----------------------------------------------------------------------------
*    A test program for user window in X; user program interacting with
*    window manager.
*----------------------------------------------------------------------------*/
    if ((pDisp = XOpenDisplay((char *)NULL)) == NULL)
	abort();
    screenNo = DefaultScreen(pDisp);
    rootWindow = DefaultRootWindow(pDisp);
    gc = DefaultGC(pDisp, screenNo);
    sizeHints.x = x = 100;
    sizeHints.y = y = 100;
    sizeHints.width = width = 512;
    sizeHints.height = height = 512;
    sizeHints.flags = PSize | PPosition;
    plotWindow = XCreateSimpleWindow(pDisp, rootWindow, x, y, width, height, 5,
		BlackPixel(pDisp, screenNo), WhitePixel(pDisp, screenNo));

    XSetStandardProperties(pDisp, plotWindow, "win title", "icon title",
		None, 0, NULL, &sizeHints);
    XSelectInput(pDisp, plotWindow,
		ExposureMask |
		ButtonPressMask | ButtonReleaseMask);
    subWin1 = XCreateSimpleWindow(pDisp, plotWindow, 30, 40, 30, 40, 1,
		BlackPixel(pDisp, screenNo), WhitePixel(pDisp, screenNo));
    subWin2 = XCreateSimpleWindow(pDisp, plotWindow, 130, 140, 130, 140, 0,
		BlackPixel(pDisp, screenNo), WhitePixel(pDisp, screenNo));
    XMapSubwindows(pDisp, plotWindow);
    XMapWindow(pDisp, plotWindow);
    pWin = pprWinOpenUW(&pDisp, &subWin1, &gc, NULL);
    PprAssertAlways(pWin != NULL);
    pWin2 = pprWinOpenUW(&pDisp, &subWin2, &gc, NULL);
    PprAssertAlways(pWin2 != NULL);
    while (1) {
	XNextEvent(pDisp, &anEvent);
	if (anEvent.type == ButtonRelease &&
				anEvent.xbutton.button == PPR_BTN_CLOSE) {
	    (void)printf("button 3 up\n");
	    pprWinClose(pWin);
	    pprWinClose(pWin2);
	    XCloseDisplay(pDisp);
	    break;
	}
	else if (anEvent.type == Expose && anEvent.xexpose.count == 0) {
	    pprWinReplot(pWin, replot, &myData);
	    pprWinReplot(pWin2, replot, &myData);
	}
    }

#elif defined SUNVIEW && defined UW
/*-----------------------------------------------------------------------------
*    A test program for user window in SunView; user program interacting with
*    window manager.
*----------------------------------------------------------------------------*/
    plotFrame = window_create(NULL, FRAME,
	FRAME_LABEL, "win title",
	FRAME_NO_CONFIRM, 1,
	WIN_X, 100, WIN_Y, 100,
	WIN_WIDTH, 512,
	WIN_HEIGHT, 512,
	0);
    winList.frame = plotFrame;
    plotCanvas = window_create(plotFrame, CANVAS,
	WIN_WIDTH, 512,
	WIN_HEIGHT, 512,
        CANVAS_AUTO_SHRINK, TRUE,
        CANVAS_AUTO_EXPAND, TRUE,
        CANVAS_FIXED_IMAGE, FALSE,
        CANVAS_RETAINED, FALSE,
        CANVAS_AUTO_CLEAR, TRUE,
	0);
    plotCanvas1 = window_create(plotFrame, CANVAS,
	WIN_X, 30, WIN_Y, 40,
	WIN_WIDTH, 30, WIN_HEIGHT, 40,
        CANVAS_AUTO_SHRINK, TRUE,
        CANVAS_AUTO_EXPAND, TRUE,
        CANVAS_FIXED_IMAGE, FALSE,
        CANVAS_RETAINED, FALSE,
        CANVAS_AUTO_CLEAR, TRUE,
	WIN_EVENT_PROC, pprTestEvHandler,
	WIN_CLIENT_DATA, &winList,
	0);
    plotCanvas2 = window_create(plotFrame, CANVAS,
	WIN_X, 130, WIN_Y, 140,
	WIN_WIDTH, 130, WIN_HEIGHT, 140,
        CANVAS_AUTO_SHRINK, TRUE,
        CANVAS_AUTO_EXPAND, TRUE,
        CANVAS_FIXED_IMAGE, FALSE,
        CANVAS_RETAINED, FALSE,
        CANVAS_AUTO_CLEAR, TRUE,
	WIN_EVENT_PROC, pprTestEvHandler,
	WIN_CLIENT_DATA, &winList,
	0);
    pWin = pprWinOpenUW(&plotFrame, &plotCanvas1, NULL, NULL);
    PprAssertAlways(pWin != NULL);
    pWin2 = pprWinOpenUW(&plotFrame, &plotCanvas2, NULL, NULL);
    PprAssertAlways(pWin2 != NULL);
    winList.pWin1 = pWin;
    winList.pWin2 = pWin2;
    winList.pMyData = &myData;
    window_set(plotFrame, WIN_CONSUME_PICK_EVENTS, WIN_NO_EVENTS,
	ACTION_OPEN, ACTION_CLOSE,
	ACTION_FRONT, ACTION_BACK,
	WIN_MOUSE_BUTTONS, WIN_UP_EVENTS, 0,
	WIN_EVENT_PROC, pprTestEvHandler,
	WIN_CLIENT_DATA, &winList,
	0);
    window_set(plotCanvas, WIN_CONSUME_PICK_EVENTS, WIN_NO_EVENTS,
	WIN_MOUSE_BUTTONS, WIN_UP_EVENTS, 0,
	WIN_EVENT_PROC, pprTestEvHandler,
	WIN_CLIENT_DATA, &winList,
	0);
    window_fit(plotFrame);
    window_set(plotFrame, WIN_SHOW, 1, 0);
    window_set(plotCanvas, WIN_SHOW, 1, 0);
    window_set(plotCanvas1, WIN_SHOW, 1, 0);
    window_set(plotCanvas2, WIN_SHOW, 1, 0);
    window_main_loop(plotFrame);


#else
/*-----------------------------------------------------------------------------
*    A test for plot package created window, plot package interacting with
*    window manager.
*----------------------------------------------------------------------------*/
    pWin = pprWinOpen(PPR_WIN_SCREEN, NULL, "test title", x, y, width, height);
    PprAssert(pWin != NULL);
    stat = pprWinLoop(pWin, replot, &myData);
    PprAssert(stat == 0);
    pprWinInfo(pWin, &x, &y, &width, &height);
    pprWinClose(pWin);
#endif

    pWin = pprWinOpen(PPR_WIN_POSTSCRIPT, "testPS", NULL, x,y,width,height);
    PprAssert(pWin != NULL);
    stat = pprWinLoop(pWin, replot, &myData);
    PprAssert(stat == 0);
    pprWinClose(pWin);
    (void)printf("PostScript plot stored in 'testPS'\n");

    return;
}

void
replot(pWin, pMyData)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
MY_DATA	*pMyData;
{
    char	answer[80];

    replotDraw(pWin, pMyData, 0);

#if 1
    if (pWin->winType == PPR_WIN_SCREEN) {
#ifdef XWINDOWS
        XFlush(pWin->pDisp);
#endif
	(void)printf("eraseTest? (e or cr)  ");
	fflush(stdout);
	if (fgets(answer, 80, stdin) == NULL || answer[0] == '\n')
	    ;	/* no action */
	else {
	    if (answer[0] == 'e')
		replotDraw(pWin, pMyData, 1);
	}
    }
#endif

    return;
}
replotDraw(pWin, pMyData, erase)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
MY_DATA	*pMyData;
int	erase;
{
    double	xlo=0., ylo=0., xhi=.95, yhi=.95;
    double	charHt, charHtX;
    PPR_AREA	*pArea;
    PPR_AREA	*pAreaClip, *pAreaClip2;
    int		i;
    int		thick=5, thin=0;
    int		keyNum=0;
    int		markNum;
    double	y=6000.;
    double	x;
    char	answer[80];

    charHt = PprDfltCharHt(ylo, yhi);
    charHtX = pprYFracToXFrac(pWin, charHt);
    pArea = pprAreaOpen(pWin, xlo+12.*charHtX, ylo+6.*charHt, xhi, yhi,
		pMyData->xMin, pMyData->yMin, pMyData->xMax, pMyData->yMax,
		5, 5, charHt);
    pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thin, NULL);
    if (!erase)
	pprPerimLabel(pArea, "x label", NULL, "y label", NULL, 0.);
    pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, 0, NULL);
    pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
    if (!erase)
	pprLineF(pArea, pMyData->x, pMyData->y, pMyData->nPts);
    else
	pprLineEraseF(pArea, pMyData->x, pMyData->y, pMyData->nPts);

    pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
    for (x=1.; x<10.; x+=1.) {
	if (!erase)
	    pprPointD(pArea, x, x*x+100.);
	else
	    pprPointEraseD(pArea, x, x*x+100.);
    }
    pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thin, NULL);
    for (x=1.; x<10.; x+=1.) {
	if (!erase)
	    pprPointD(pArea, x, x*x+200.);
	else
	    pprPointEraseD(pArea, x, x*x+200.);
    }
    if (!erase)
	pprAnnotYMark(pArea, 0, 1);

    for (keyNum=0; keyNum<=PPR_NKEYS; keyNum++) {
	pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, keyNum, NULL);
	if (!erase)
	    pprLineSegD(pArea, 0., y, 50., y);
	else
	    pprLineSegEraseD(pArea, 0., y, 50., y);
	pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
	if (!erase)
	    pprLineSegD(pArea, 50., y, 70., y);
	else
	    pprLineSegEraseD(pArea, 50., y, 70., y);
	pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thin, NULL);
	y -= 100.;
    }
    pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, 0, NULL);
    for (i=0; i<=PPR_NCOLORS; i++) {
	pprAreaSetAttr(pArea, PPR_ATTR_COLORNUM, i, NULL);
	if (!erase)
	    pprLineSegD(pArea, 0., y, 50., y);
	else
	    pprLineSegEraseD(pArea, 0., y, 50., y);
	y -= 100.;
    }
    x = 1.;
    for (i=0; i<PPR_NMARKS; i++) {
	pprAreaSetAttr(pArea, PPR_ATTR_COLORNUM, i, NULL);
	if (!erase)
	    pprMarkD(pArea, x, y, i);
	else
	    pprMarkEraseD(pArea, x, y, i);
	x += 1.5;
    }
    y -= 100.;
    x = 20.;
    if (!erase) {
	pprText(pArea, x, y, "centered", PPR_TXT_CEN, 0., 0.);
	y -= 100.;
	pprText(pArea, x, y, "right just", PPR_TXT_RJ, 0., 0.);
	y -= 100.;
	pprText(pArea, x, y, "left just", PPR_TXT_LJ, 0., 0.);
	y -= 100.;
    }
    else {
	pprTextErase(pArea, x, y, "centered", PPR_TXT_CEN, 0., 0.);
	y -= 100.;
	pprTextErase(pArea, x, y, "right just", PPR_TXT_RJ, 0., 0.);
	y -= 100.;
	pprTextErase(pArea, x, y, "left just", PPR_TXT_LJ, 0., 0.);
	y -= 100.;
    }
    y -= 200.;
    x = 20.;
    if (!erase) {
	pprText(pArea, x, y, "centered", PPR_TXT_CEN, 0., 170.);
	y -= 200.;
	pprText(pArea, x, y, "right just", PPR_TXT_RJ, 0., 170.);
	y -= 200.;
	pprText(pArea, x, y, "left just", PPR_TXT_LJ, 0., 170.);
	y -= 200.;
    }
    else {
	pprTextErase(pArea, x, y, "centered", PPR_TXT_CEN, 0., 170.);
	y -= 200.;
	pprTextErase(pArea, x, y, "right just", PPR_TXT_RJ, 0., 170.);
	y -= 200.;
	pprTextErase(pArea, x, y, "left just", PPR_TXT_LJ, 0., 170.);
	y -= 200.;
    }

/*-----------------------------------------------------------------------------
* test clipping
*----------------------------------------------------------------------------*/
    pAreaClip = pprAreaOpenDflt(pWin, .65, .06, .9, .26, 0., 0., 1., 1.);
    PprAssert(pAreaClip != NULL);
    if (!erase)
	pprPerim(pAreaClip);
    pprAreaSetAttr(pAreaClip, PPR_ATTR_CLIP, 0, NULL);
    if (!erase) {
	pprLineSegD(pAreaClip, .1,.1, .9,.9);
	pprLineSegD(pAreaClip, .1,.9, .9,.1);
    }
    else {
	pprLineSegEraseD(pAreaClip, .1,.1, .9,.9);
	pprLineSegEraseD(pAreaClip, .1,.9, .9,.1);
    }
#if 0
	pprLineSegD(pAreaClip, -1.,.5, .5,.9);
#endif
    for (y=-1.; y<=2.; y+=.5) {
	if (!erase) {
	    pprLineSegD(pAreaClip, -1.,.5, .5,y);
	    pprLineSegD(pAreaClip, -1.,.5, 2.,y);
	    pprLineSegD(pAreaClip, 2.,.5, .5,y);
	    pprLineSegD(pAreaClip, 2.,.5, -1.,y);
	}
	else {
	    pprLineSegEraseD(pAreaClip, -1.,.5, .5,y);
	    pprLineSegEraseD(pAreaClip, -1.,.5, 2.,y);
	    pprLineSegEraseD(pAreaClip, 2.,.5, .5,y);
	    pprLineSegEraseD(pAreaClip, 2.,.5, -1.,y);
	}
    }
    for (x=-1.; x<=2.; x+=.5) {
	if (!erase) {
	    pprLineSegD(pAreaClip, .5,-1., x,.5);
	    pprLineSegD(pAreaClip, .5,-1., x,2.);
	    pprLineSegD(pAreaClip, .5,2., x,.5);
	    pprLineSegD(pAreaClip, .5,2., x,-1.);
	}
	else {
	    pprLineSegEraseD(pAreaClip, .5,-1., x,.5);
	    pprLineSegEraseD(pAreaClip, .5,-1., x,2.);
	    pprLineSegEraseD(pAreaClip, .5,2., x,.5);
	    pprLineSegEraseD(pAreaClip, .5,2., x,-1.);
	}
    }
    pprAreaClose(pAreaClip);

    pAreaClip2 = pprAreaOpenDflt(pWin, .65, .3, .9, .5, 0., 0., 1., 1.);
    PprAssert(pAreaClip2 != NULL);
    if (!erase)
	pprPerim(pAreaClip2);
    pprAreaSetAttr(pAreaClip2, PPR_ATTR_CLIP, 0, NULL);
    if (!erase)
	printf("25 marks with squares should appear; no others!\n");
    for (y=-.25; y<=2.; y+=.25) {
	for (x=-.25; x<=2.; x+=.25) {
	    if (y < -.01 || y > 2.01 || x < -.01 || x > 2.01)
		markNum = 3;
	    else
		markNum = 1;
	    if (!erase)
		pprMarkD(pAreaClip2, x, y, markNum);
	    else
		pprMarkEraseD(pAreaClip2, x, y, markNum);
	}
    }
    pprAreaClose(pAreaClip2);


#if 1
    if (!erase && pWin->winType == PPR_WIN_SCREEN) {
#ifdef XWINDOWS
        XFlush(pWin->pDisp);
#endif
	(void)printf( "erase win, area, grid, or perim? (w,a,g,p, or cr)  ");
	fflush(stdout);
	if (fgets(answer, 80, stdin) == NULL || answer[0] == '\n')
	    ;	/* no action */
	else {
	    if (answer[0] == 'w')
		pprWinErase(pWin);
	    else if (answer[0] == 'a')
		pprRegionErase(pArea,.3,.3,.6,.9);
	    else if (answer[0] == 'p')
		pprPerimErase(pArea);
	    else if (answer[0] == 'g')
		pprGridErase(pArea);
	    (void)printf("done erasing: ");
	    fflush(stdout);
	}
    }
#endif

    pprAreaClose(pArea);

    return;
}
#endif

/*/subhead test_SunView_EvHandler----------------------------------------------
*
*----------------------------------------------------------------------------*/
#if defined SUNVIEW && defined UW
void
pprTestEvHandler(window, pEvent, pArg)
Window	window;
Event	*pEvent;
void	*pArg;
{
    struct pprWinWin *pWinList;

    pWinList = (struct pprWinWin *)window_get(window, WIN_CLIENT_DATA);
    if (event_action(pEvent) == WIN_REPAINT) {
	if (!window_get(window, FRAME_CLOSED)) {
	    if (window == pWinList->pWin1->canvas)
		pprWinReplot(pWinList->pWin1, replot, pWinList->pMyData);
	    if (window == pWinList->pWin2->canvas)
		pprWinReplot(pWinList->pWin2, replot, pWinList->pMyData);
	}
    }
    else if (event_action(pEvent) == PPR_BTN_CLOSE) {
	if (event_is_up(pEvent)) {
	    pprWinClose(pWinList->pWin1);
	    pprWinClose(pWinList->pWin2);
	    window_destroy(pWinList->frame);
	}
    }
}
#endif

/*+/subr**********************************************************************
* NAME	pprAnnotX - annotate an X axis, perhaps drawing a line and tick marks
*
* DESCRIPTION
*	Annotate an X axis, placing annotations at the major tick intervals.
*
*	If desired, an axis line is also drawn, with tick marks at the
*	major intervals.  The tick marks are drawn using the "generic"
*	line attributes for the plot window.  The axis line is drawn with
*	the line attributes of the plot area; this allows using a dashed
*	line pattern or color to associate an axis with a data line.
*
*	The annotations and label are drawn using the default character
*	height for the plot area, in the color, if any, for the plot area.
*
*	An array of text strings can be supplied for annotating the tick
*	intervals.  This is useful if the desired annotations are text.
*	If an annotation array isn't supplied, then numeric annotations
*	are generated.
*
*	To allow multiple calibrations for an axis, this routine accepts
*	an `offset' argument.  If this argument is greater than 0, then
*	the annotation and labeling activity occurs that many lines (in
*	the default character height for the plot area) below the axis
*	which was established by pprAreaOpen.
*
*	An alternate entry point, pprAnnotX_wc, is available to use the
*	plot window's color for the annotation.
*
* RETURNS
*	void
*
* BUGS
* o	only linear axes are handled
* o	doesn't presently support offset processing
*
* NOTES
* 1.	Uses a space below the axis of 5 character heights.
*
* SEE ALSO
*	pprAnnotY, pprGrid, pprPerim, pprAreaOpen, pprAreaSetAttr
*
*-*/
void
pprAnnotX(pArea, offset, xLeft, xRight, xNint, drawLine, xLabel, xAnnot, angle)
PPR_AREA *pArea;	/* I pointer to plotter area */
int	offset;		/* I offset as number of lines below yBot to annotate */
double	xLeft;		/* I x data value at left end of axis */
double	xRight;		/* I x data value at right end of axis */
int	xNint;		/* I number of major intervals for axis */
int	drawLine;	/* I 1 says to draw a line and tick marks */
char	*xLabel;	/* I label for x axis, or NULL; oriented horizontal */
char	**xAnnot;	/* I pointer to array of x annotations, or NULL */
double	angle;		/* I orientation angle for annotation text; 0. or 90. */
{
    pprAnnotX_gen(pArea,offset,xLeft,xRight,xNint,drawLine,xLabel,xAnnot,angle,
		pprLineSegD_ac, pprLineSegD, pprText);
}
void
pprAnnotX_wc(pArea,offset,xLeft,xRight,xNint,drawLine,xLabel,xAnnot,angle)
PPR_AREA *pArea;	/* I pointer to plotter area */
int	offset;		/* I offset as number of lines below yBot to annotate */
double	xLeft;		/* I x data value at left end of axis */
double	xRight;		/* I x data value at right end of axis */
int	xNint;		/* I number of major intervals for axis */
int	drawLine;	/* I 1 says to draw a line and tick marks */
char	*xLabel;	/* I label for x axis, or NULL; oriented horizontal */
char	**xAnnot;	/* I pointer to array of x annotations, or NULL */
double	angle;		/* I orientation angle for annotation text; 0. or 90. */
{
    pprAnnotX_gen(pArea,offset,xLeft,xRight,xNint,drawLine,xLabel,xAnnot,angle,
		pprLineSegD_wc, pprLineSegD, pprText_wc);
}
static void
pprAnnotX_gen(pArea,offset,xLeft,xRight,xNint,drawLine,xLabel,xAnnot,angle,
		fnTick, fnLine, fnText)
PPR_AREA *pArea;	/* I pointer to plotter area */
int	offset;		/* I offset as number of lines below yBot to annotate */
double	xLeft;		/* I x data value at left end of axis */
double	xRight;		/* I x data value at right end of axis */
int	xNint;		/* I number of major intervals for axis */
int	drawLine;	/* I 1 says to draw a line and tick marks */
char	*xLabel;	/* I label for x axis, or NULL; oriented horizontal */
char	**xAnnot;	/* I pointer to array of x annotations, or NULL */
double	angle;		/* I orientation angle for annotation text; 0. or 90. */
void	(*fnTick)();
void	(*fnLine)();
void	(*fnText)();
{
    double	tickHalf, tick1, tick2, tick1S, tick2S;
    int		i, j;
    double	x, x1, xInterval, xSubint, y;
    double	xVal, xValInterval;
    char	*pText, text[80];
    int		nCol=6;		/* number of columns for annotation label */
    int		sigDigits;	/* sig digits to print */
    double	maxVal;		/* maximum of the end values for axis */
    PPR_TXT_JUST just;		/* justification code for annotations */
    
    tickHalf = pArea->tickHt / pArea->yScale;
    tick1S = pArea->yBot;
    tick2S = tick1S - tickHalf;
    tick1 = tick1S - tickHalf;
    tick2 = tick1S + tickHalf;
    if (drawLine == 0 && pArea->xNsubint > 0) {
	drawLine = 1;
	tick1 = tick1S;
	tick2 = tick1S - 2.*tickHalf;
    }
    maxVal = PprMax(PprAbs(xLeft),PprAbs(xRight));
    if (maxVal >= 100.)		sigDigits = 0;
    else if (maxVal >= 10.)	sigDigits = 1;
    else if (maxVal >= 1.)	sigDigits = 2;
    else			sigDigits = 3;

    x = pArea->xLeft;
    xInterval = (pArea->xRight - pArea->xLeft) / xNint;
    if (pArea->xNsubint > 0)
	xSubint = xInterval / pArea->xNsubint;
    xVal = xLeft;
    xValInterval = (xRight - xLeft) / xNint;
    y = pArea->yBot - 2. * pArea->charHt / pArea->yScale;
    for (i=0; i<=xNint; i++) {
	if (i == xNint) x = pArea->xRight, xVal = xRight;
	if (drawLine) fnTick(pArea, x, tick1, x, tick2);

	if (pArea->xNsubint > 0 && i != xNint) {
	    for (j=1, x1=x+xSubint; j<pArea->xNsubint; j++, x1+=xSubint)
		fnTick(pArea, x1, tick1S, x1, tick2S);
	}

	if (i == 0) just = PPR_TXT_LJ;
	else if (i == xNint) just = PPR_TXT_RJ;
	else just = PPR_TXT_CEN;
	if (xAnnot == NULL) 
	    pprCvtDblToTxt(text, nCol, xVal, sigDigits), pText = text;
	else
	    pText = xAnnot[i];
	fnText(pArea, x, y, pText, just, 0., angle);

	x += xInterval;
	xVal += xValInterval;
    }
    if (xLabel != NULL) {
	x = (pArea->xLeft + pArea->xRight) / 2.;
	y = pArea->yBot - 4. * pArea->charHt / pArea->yScale;
	fnText(pArea, x, y, xLabel, PPR_TXT_CEN, 0., angle);
    }
}

/*+/subr**********************************************************************
* NAME	pprAnnotY - annotate a Y axis, perhaps drawing line and tick marks
*
* DESCRIPTION
*	Annotate a Y axis, placing annotations at the major tick intervals.
*
*	If desired, an axis line is also drawn, with tick marks at the
*	major intervals.  The tick marks are drawn using the "generic"
*	line attributes for the plot window.  The axis line is drawn with
*	the line attributes of the plot area; this allows using a dashed
*	line pattern or color to associate an axis with a data line.
*
*	The annotations and label are drawn using the default character
*	height for the plot area, in the color, if any, for the plot area.
*
*	An array of text strings can be supplied for annotating the tick
*	intervals.  This is useful if the desired annotations are text.
*	If an annotation array isn't supplied, then numeric annotations
*	are generated.
*
*	To allow multiple calibrations for an axis, this routine accepts
*	an `offset' argument.  If this argument is greater than 0, then
*	the annotation and labeling activity occurs that many lines (in
*	the default character height for the plot area) to the left of
*	the axis which was established by pprAreaOpen.
*
*	An alternate entry point, pprAnnotY_wc, is available to use the
*	plot window's color for the annotation.
*
* RETURNS
*	void
*
* BUGS
* o	only linear axes are handled
*
* NOTES
* 1.	Uses a space to the left of the axis of 12 character heights if
*	annotations are horizontal, and a space of 5 character heights
*	if they are vertical.
*
* SEE ALSO
*	pprAnnotX, pprGrid, pprPerim, pprAreaOpen, pprAreaSetAttr
*
*-*/
void
pprAnnotY(pArea, offset, yBot, yTop, yNint, drawLine, yLabel, yAnnot, angle)
PPR_AREA *pArea;	/* I pointer to plot area structure */
int	offset;		/* I number of lines to left of axis for annotation */
double	yBot;		/* I y data value at bottom end of axis */
double	yTop;		/* I y data value at top end of axis */
int	yNint;		/* I number of major intervals for axis */
int	drawLine;	/* I 1 says to draw a line and tick marks */
char	*yLabel;	/* I label for y axis, or NULL; oriented vertical */
char	**yAnnot;	/* I pointer to array of y annotations, or NULL */
double	angle;		/* I orientation angle for annotation text; 0. or 90. */
{
    pprAnnotY_gen(pArea,offset,yBot,yTop,yNint,drawLine,yLabel,yAnnot,angle,
		pprLineSegD_ac, pprLineSegD, pprText);
}
void
pprAnnotY_wc(pArea, offset, yBot, yTop, yNint, drawLine, yLabel, yAnnot, angle)
PPR_AREA *pArea;	/* I pointer to plot area structure */
int	offset;		/* I number of lines to left of axis for annotation */
double	yBot;		/* I y data value at bottom end of axis */
double	yTop;		/* I y data value at top end of axis */
int	yNint;		/* I number of major intervals for axis */
int	drawLine;	/* I 1 says to draw a line and tick marks */
char	*yLabel;	/* I label for y axis, or NULL; oriented vertical */
char	**yAnnot;	/* I pointer to array of y annotations, or NULL */
double	angle;		/* I orientation angle for annotation text; 0. or 90. */
{
    pprAnnotY_gen(pArea,offset,yBot,yTop,yNint,drawLine,yLabel,yAnnot,angle,
		pprLineSegD_wc, pprLineSegD, pprText_wc);
}
void
pprAnnotY_gen(pArea, offset, yBot, yTop, yNint, drawLine, yLabel, yAnnot, angle,
		fnTick, fnLine, fnText)
PPR_AREA *pArea;	/* I pointer to plot area structure */
int	offset;		/* I number of lines to left of axis for annotation */
double	yBot;		/* I y data value at bottom end of axis */
double	yTop;		/* I y data value at top end of axis */
int	yNint;		/* I number of major intervals for axis */
int	drawLine;	/* I 1 says to draw a line and tick marks */
char	*yLabel;	/* I label for y axis, or NULL; oriented vertical */
char	**yAnnot;	/* I pointer to array of y annotations, or NULL */
double	angle;		/* I orientation angle for annotation text; 0. or 90. */
void	(*fnTick)();
void	(*fnLine)();
void	(*fnText)();
{
    double	tickHalf, tick1, tick2;
    int		i;
    double	x;		/* x coord for annotations */
    double	xL;		/* x coord for label */
    double	xBase;		/* base x coordinate */
    double	y, yInterval;
    double	yVal, yValInterval;
    char	text[80];
    int		nCol=6;		/* number of columns for annotation label */
    int		sigDigits;	/* sig digits to print */
    double	maxVal;		/* maximum of the end values for axis */
    PPR_TXT_JUST just;		/* justification flag for text */

    xBase = pArea->xLeft - (double)offset * pArea->charHt / pArea->xScale;
    if (drawLine) {
	tickHalf = pArea->tickHt / pArea->xScale;
	tick1 = xBase - tickHalf;
	if (offset == 0)
	    tick2 = xBase + tickHalf;
	else
	    tick2 = xBase;
    }
    maxVal = PprMax(PprAbs(yBot),PprAbs(yTop));
    if (maxVal >= 100.)		sigDigits = 0;
    else if (maxVal >= 10.)	sigDigits = 1;
    else if (maxVal >= 1.)	sigDigits = 2;
    else			sigDigits = 3;

    if (angle == 0.) {
	x = xBase - 2. * pArea->charHt / pArea->xScale;
	xL = xBase - 10. * pArea->charHt / pArea->xScale;
    }
    else {
	x = xBase - 2. * pArea->charHt / pArea->xScale;
	xL = xBase - 4. * pArea->charHt / pArea->xScale;
    }
    y = pArea->yBot;
    yInterval = (pArea->yTop - pArea->yBot) / yNint;
    yVal = yBot;
    yValInterval = (yTop - yBot) / yNint;
    if (drawLine)
	fnTick(pArea, tick1, y, tick2, y);
    if (angle == 0.)	just = PPR_TXT_RJ;
    else		just = PPR_TXT_LJ;
    if (yAnnot == NULL) {
	pprCvtDblToTxt(text, nCol, yVal, sigDigits);
	fnText(pArea, x, y, text, just, 0., angle);
    }
    else
	fnText(pArea, x, y, yAnnot[0], just, 0., angle);
    if (angle == 0.)	just = PPR_TXT_RJ;
    else		just = PPR_TXT_CEN;
    for (i=1; i<yNint; i++) {
	y += yInterval;
	yVal += yValInterval;
	if (drawLine)
	    fnTick(pArea, tick1, y, tick2, y);
	if (yAnnot == NULL) {
	    pprCvtDblToTxt(text, nCol, yVal, sigDigits);
	    fnText(pArea, x, y, text, just, 0., angle);
	}
	else
	    fnText(pArea, x, y, yAnnot[i], just, 0., angle);
    }
    y = pArea->yTop;
    if (angle == 0.)	just = PPR_TXT_RJ;
    else		just = PPR_TXT_RJ;
    if (yAnnot == NULL) {
	pprCvtDblToTxt(text, nCol, yTop, sigDigits);
	fnText(pArea, x, y, text, just, 0., angle);
    }
    else
	fnText(pArea, x, y, yAnnot[yNint], just, 0., angle);
    if (drawLine) {
	fnTick(pArea, tick1, y, tick2, y);
	fnLine(pArea, xBase, pArea->yBot, xBase, pArea->yTop);
    }
    if (yLabel != NULL) {
	y = (pArea->yBot + pArea->yTop) / 2.;
	fnText(pArea, xL, y, yLabel, PPR_TXT_CEN, 0., 90.);
    }
}

/*+/subr**********************************************************************
* NAME	pprAnnotYMark - add plot marks to a Y axis annotation
*
* DESCRIPTION
*	Draw two plot marks at the foot of the Y axis annotation, to allow
*	associating the axis with a particular set of data.
*
* RETURNS
*	void
*
* BUGS
* o	only linear axes are handled
*
* SEE ALSO
*	pprMark, pprAnnotY
*
* EXAMPLE
*
*-*/
void
pprAnnotYMark(pArea, offset, markNum)
PPR_AREA *pArea;	/* I pointer to plot area structure */
int	offset;		/* I number of lines to left of axis for annotation */
int	markNum;	/* I mark number */
{
    double	x, y;

    x = pArea->xLeft - (double)(offset+3) * pArea->charHt / pArea->xScale;
    y = pArea->yBot - pArea->charHt * 2. / pArea->yScale;
    pprMarkD(pArea, x, y, markNum);
}

/*+/subr**********************************************************************
* NAME	pprArc - draw an arc
*
* DESCRIPTION
*	Draw an arc.  The arc is specified by a radius and two angles.  The
*	angles, in degrees, specify the angle at which the arc starts and
*	the angle at which it ends.  An angle increment specifies both the
*	direction of the arc and the size of the chords which approximate
*	the arc.  Angles are measured counter-clockwise from the positive
*	X axis.
*
*	The radius of the arc is treated as representing data values in the
*	plot area.  If both the X and Y axes of the plot area have the
*	same data scaling, then circular arcs will be produced (assuming
*	a square plot area).  If the X scaling is not the same as the Y
*	scaling, then elliptical arcs will be produced.
*
*	The arc is drawn using the color, dashed line, and other attributes
*	of the plot area.  Alternate entry points are:
*
*		pprArcD_ac	uses the area color, but ignores other
*				attributes
*
* RETURNS
*	void
*
* SEE ALSO
*
*-*/
void
pprArcD(pArea, xDbl, yDbl, radDbl, angle1, angle2, angleIncr)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	xDbl;		/* I x data coordinate for center of arc */
double	yDbl;		/* I y data coordinate for center of arc */
double	radDbl;
double	angle1;		/* I angle to start arc */
double	angle2;		/* I angle to stop arc */
double	angleIncr;	/* I size of steps in drawing arc */
{
    pprArcD_gen(pArea, xDbl, yDbl, radDbl, angle1, angle2, angleIncr,
	pprMoveD);
}
void
pprArcD_ac(pArea, xDbl, yDbl, radDbl, angle1, angle2, angleIncr)
PPR_AREA *pArea;
double	xDbl, yDbl, radDbl, angle1, angle2, angleIncr;
{
    pprArcD_gen(pArea, xDbl, yDbl, radDbl, angle1, angle2, angleIncr,
	pprMoveD_ac);
}
void
pprArcD_gen(pArea, xDbl, yDbl, radDbl, angle1, angle2, angleIncr, fn)
PPR_AREA *pArea;
double	xDbl, yDbl, radDbl, angle1, angle2, angleIncr;
void	(*fn)();	/* I line drawing function */
{
    double	x, y, angle;
    int		pen=0;

    if (angle1 > angle2 && angleIncr > 0.) {
	while (angle1 > angle2)
	    angle2 += 360.;
    }
    else if (angle1 < angle2 && angleIncr < 0.) {
	while (angle1 < angle2)
	    angle1 += 360.;
    }
    angle = angle1;
    while (1) {
	x = xDbl + radDbl * pprCos_deg(angle);
	y = yDbl + radDbl * pprSin_deg(angle);
	fn(pArea, x, y, pen);
	pen = 1;
	if (angle == angle2)
	    break;
	if ((angle += angleIncr) > angle2)
	    angle = angle2;
    }
}

/*+/subr**********************************************************************
* NAME	pprAreaClose - close a plot area
*
* DESCRIPTION
*	Frees the memory associated with a plot area pointer and does other
*	cleanup operations.  This routine should be called prior to calling
*	pprWinClose.
*
* RETURNS
*	void
*
*-*/
void
pprAreaClose(pArea)
PPR_AREA *pArea;	/* I pointer to plot area structure */
{
    PPR_WIN *pWin;

    pWin = pArea->pWin;

#ifdef XWINDOWS
    if (pWin->winType == PPR_WIN_SCREEN) {
	if (pArea->attr.myGC)
	    XFree(pArea->attr.gc);
	if (pArea->attr.bgGC)
	    XFree(pArea->attr.gcBG);
    }
#endif
    DoubleListRemove(pArea, pWin->pAreaHead, pWin->pAreaTail);
    free((char *)pArea);
}

/*+/subr**********************************************************************
* NAME	pprAreaErase - erase an area within a plot window
*
* DESCRIPTION
*	Erases an area within a plot window.
*
* RETURNS
*	void
*
* SEE ALSO
*	pprWinErase, pprGridErase, pprPerimErase, pprRegionErase
*	the ppr...Erase... entry points for the various drawing routines
*
* EXAMPLES
* 1.	A data area occupies the upper right quarter of the plot window,
*	with data values at the lower left corner of xleft,ybot and at
*	the upper right of xright,ytop.  3 annotation areas are used to
*	the left of the data area, extending from the left edge of the
*	plot window to the left edge of the data area; each annotation
*	area occupies 1/3 of the height of the data area.  Erase the
*	middle annotation area.
*
*	double	xl,yb,xr,yt;
*
*	The region to erase is expressed as data values.  The y values are
*	straightforward--just 1/3 and 2/3 of the range of the y axis.  The
*	right-hand x value is just a bit less than the x at the left end
*	of the x axis; the left hand x value requires a bit of analytic
*	geometry to get.
*
*	xl = xleft - (xright - xleft)/(1. - .5) * (.5 - 0.);
*	yb = ybot + (ytop - ybot)/3.;
*	xr = xright - .001 * (xright - xleft);
*	yt = ybot + 2.*(ytop - ybot)/3.;
*	pprAreaErase(pArea, xl, yb, xr, yt);
*
*-*/
void
pprAreaErase(pArea, xDblLeft, yDblBot, xDblRight, yDblTop)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	xDblLeft;	/* I x data value at left edge of area */
double	yDblBot;	/* I y data value at bottom edge of area */
double	xDblRight;	/* I x data value at right edge of area */
double	yDblTop;	/* I y data value at top edge of area */
{
    int		x,y,x1,x2,y1,y2,width,height;

    if (pArea->pWin->winType != PPR_WIN_SCREEN)
	return;
    x1 = pArea->xPixLeft + nint((xDblLeft - pArea->xLeft) * pArea->xScale);
    x2 = pArea->xPixLeft + nint((xDblRight - pArea->xLeft) * pArea->xScale);
    if (x1 < x2) {
	x = x1;
	width = x2 - x1;
    }
    else {
	x = x2;
	width = x1 - x2;
    }
    y1 = pArea->yPixBot + nint((yDblBot - pArea->yBot) * pArea->yScale);
    y2 = pArea->yPixBot + nint((yDblTop - pArea->yBot) * pArea->yScale);
    if (y1 < y2) {
	y = y1;
	height = y2 - y1;
    }
    else {
	y = y2;
	height = y1 - y2;
    }
#ifdef SUNVIEW
    y = pArea->pWin->height - y - height;
    pw_writebackground(pArea->pWin->pw, x, y, width, height, PIX_SRC);
#elif defined XWINDOWS
    y = pArea->pWin->height - y - height;
    XClearArea(pArea->pWin->pDisp, pArea->pWin->plotWindow,
						x, y, width, height, False);
#endif
}

/*+/subr**********************************************************************
* NAME	pprAreaOpen - initialize a plot area
*
* DESCRIPTION
*	Initialize a plot area within the plot window.  This initialization
*	must be done before calling any of the routines which do actual
*	plotting.
*
*	This routine establishes a rectangular "data area" within the plot
*	window.  It is within the data area that data will be plotted.
*	The size and position of the data area are specified in terms
*	of fractions of the window size; they are expressed as
*	"coordinates" of the lower left and upper right corners of the
*	area.  The data area specified in the call to pprAreaOpen does
*	NOT include space for axis annotations and labels.  (pprAreaOpenDflt
*	can be used to automatically get the necessary margins.)
*
*	This routine must also be told the data values at the two corners
*	of the data area in order to determine scaling.
*
*	In addition to establishing scaling, this routine accepts information
*	about how many major divisions there are for each axis and what
*	default character height is to be used for displaying text within
*	the plot area (see pprText for more information).  If any of these
*	parameters is specified as zero, this routine chooses an appropriate
*	value.
*
*	The default line attributes for the plot are copied from those of
*	the plot window.  pprAreaSetAttr can be used to change them.  Under
*	X11, a gc is created for the plot area with the foreground and
*	background being copied from the gc for the plot window;
*	pprAreaSetAttr can be used to change the foreground and background.
*
*	When plotting is complete for a plot area, pprAreaClose should
*	be called.
*
* RETURNS
*	pointer to plot area, or
*	NULL
*
* BUGS
* o	only linear calibration is handled
*
* SEE ALSO
*	pprWinOpen, pprAreaOpenDflt, pprAreaSetAttr
*	pprAutoEnds, pprAutoInterval, pprAutoRange
*	pprText
*
* EXAMPLE
* 1.	Set up an area which occupies the full width of the window, but
*	which uses the middle third vertically.  The range for x values
*	is 0 to 100; for y, the range is -10 to 10.  Both the x and y
*	axes are to be divided into 5 intervals.
*
*	Allow space below and to the left of the actual area for plotting
*	for pprPerim to place labels and annotations.  The required
*	size of "margins" depends on the size of characters used, so a
*	default size is determined (and put into effect as part of the
*	pprAreaOpen call).
*
*	PPR_AREA *pArea;
*	double	charHt, charHtX;
*
*	charHt = PprDfltCharHt(.33, .67);
*	charHtX = pprYFracToXFrac(pWin, charHt);
*	pArea = pprAreaOpen(pWin, 0.+12.*charHtX, .33+6*charHt, 1., .67,
*			0., -10., 100., 10., 5, 5, charHt);
*	...
*	pprAreaClose(pArea);
*
*-*/
PPR_AREA *
pprAreaOpen(pWin, wfracXleft, wfracYbot, wfracXright, wfracYtop,
			    xLeft, yBot, xRight, yTop, xNint, yNint, charHt)
PPR_WIN *pWin;		/* I pointer to plot window structure */
double	wfracXleft;	/* I x window fraction of left edge of data area */
double	wfracYbot;	/* I y window fraction of bottom edge of data area */
double	wfracXright;	/* I x window fraction of right edge of data area */
double	wfracYtop;	/* I y window fraction of top edge of data area */
double	xLeft;		/* I x data value at left side of data area */
double	yBot;		/* I y data value at bottom side of data area */
double	xRight;		/* I x data value at right side of data area */
double	yTop;		/* I y data value at top side of data area */
int	xNint;		/* I x axis number of intervals; if <=0, a default
				value is provided */
int	yNint;		/* I y axis number of intervals; if <=0, a default
				value is provided */
double	charHt;		/* I value to use as default for character size, as
				a fraction of window height; if <= 0.,
				a default value is provided */
{
    PPR_AREA *pArea;		/* pointer to plot area structure */

    if ((pArea = (PPR_AREA *)malloc(sizeof(PPR_AREA))) == NULL) {
	(void)printf("pprAreaOpen: couldn't malloc plot area struct\n");
	return NULL;
    }
    DoubleListAppend(pArea, pWin->pAreaHead, pWin->pAreaTail);

    pArea->pWin = pWin;
    pArea->xFracLeft = wfracXleft;
    pArea->xFracRight = wfracXright;
    if (xNint <= 0)
	pprAutoInterval(xLeft, xRight, &xNint);
    pArea->xNint = xNint;
    pArea->xNsubint = 0;

    pArea->yFracBot = wfracYbot;
    pArea->yFracTop = wfracYtop;
    if (yNint <= 0)
	pprAutoInterval(yBot, yTop, &yNint);
    pArea->yNint = yNint;
    pArea->yNsubint = 0;

    pArea->charHt = charHt * pWin->height;
    pArea->oldWinHt = pWin->height;

    pprAreaRescale(pArea, xLeft, yBot, xRight, yTop);

#ifdef XWINDOWS
    if (pWin->winType == PPR_WIN_SCREEN) {
	pArea->attr.gc = XCreateGC(pWin->pDisp, pWin->plotWindow, 0, NULL);
	XCopyGC(pWin->pDisp, pWin->attr.gc, GCForeground | GCBackground,
							pArea->attr.gc);
	pArea->attr.myGC = 1;
    }
#elif
    pArea->attr.myGC = 0;
#endif
    pArea->attr.bgGC = 0;
    pArea->attr.pPatt = NULL;
    pArea->attr.lineThick = 1;
    pArea->attr.ltCurr = -1;
    pArea->attr.clip = 0;

    return pArea;
}

/*+/subr**********************************************************************
* NAME	pprAreaOpenDflt - initialize a plot area using defaults
*
* DESCRIPTION
*	Initialize a plot area within the plot window.  This initialization
*	must be done before calling any of the routines which do actual
*	plotting.
*
*	This routine is a variant on pprAreaOpen.  It performs the functions
*	of that routine, but uses some defaults rather than making the
*	caller determine specific values.  In particular, this routine:
*
*	o   sets a default character height
*
*	o   determines the number of major divisions for each axis
*
*	o   establishes, inside the plot area specified, margins which
*	    will be adequate for annotating and labeling the axes.
*
*	See the description for pprAreaOpen for additional details.
*
*	When plotting is complete for a plot area, pprAreaClose should
*	be called.
*
* RETURNS
*	pointer to plot area, or
*	NULL
*
* BUGS
* o	only linear calibration is handled
*
* SEE ALSO
*	pprWinOpen, pprAreaOpen, pprAreaSetAttr
*	pprAutoEnds, pprAutoInterval, pprAutoRange
*	pprText
*
*-*/
PPR_AREA *
pprAreaOpenDflt(pWin, wfracXleft, wfracYbot, wfracXright, wfracYtop,
			    xLeft, yBot, xRight, yTop)
PPR_WIN *pWin;		/* I pointer to plot window structure */
double	wfracXleft;	/* I x window fraction of left edge of plot area */
double	wfracYbot;	/* I y window fraction of bottom edge of plot area */
double	wfracXright;	/* I x window fraction of right edge of plot area */
double	wfracYtop;	/* I y window fraction of top edge of plot area */
double	xLeft;		/* I x data value at left side of data area */
double	yBot;		/* I y data value at bottom side of data area */
double	xRight;		/* I x data value at right side of data area */
double	yTop;		/* I y data value at top side of data area */
{
    int		xNint;		/* x axis number of intervals */
    int		yNint;		/* y axis number of intervals */
    double	charHt, charHtX;/* default for character size */

    pprAutoInterval(xLeft, xRight, &xNint);
    pprAutoInterval(yBot, yTop, &yNint);
    charHt = PprDfltCharHt(wfracYbot, wfracYtop);
    charHtX = pprYFracToXFrac(pWin, charHt);
    wfracXleft += 12. * charHtX;
    wfracYbot += 6. * charHt;
    return pprAreaOpen(pWin, wfracXleft, wfracYbot, wfracXright, wfracYtop,
			    xLeft, yBot, xRight, yTop, xNint, yNint, charHt);
}

/*+/subr**********************************************************************
* NAME	pprAreaRescale - change scaling for existing plot area
*
* DESCRIPTION
*	Changes the scaling for a plot area using new data values at the
*	edges of the data area.  The actual size and position of the
*	data area within the plot window aren't changed.
*
*	No re-drawing is done by this routine.  Typically, the caller will
*	erase the appropriate area, draw the grid or perimeter, set the
*	area for clipping, and, finally, replot the data.
*
*	Default character height for the plot area is altered proportionally
*	to the rescaling of the plot area.
*
* RETURNS
*	void
*
* SEE ALSO
*	pprAreaSetAttr, pprAreaErase, pprRegionErase
*
*-*/
void 
pprAreaRescale(pArea, xLeft, yBot, xRight, yTop)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	xLeft;		/* I x data value at left side of data area */
double	yBot;		/* I y data value at bottom side of data area */
double	xRight;		/* I x data value at right side of data area */
double	yTop;		/* I y data value at top side of data area */
{
    PPR_WIN *pWin;

    pWin = pArea->pWin;
    if (xLeft == xRight) {
	(void)printf("pprAreaRescale: x left and right are equal\n");
	return;
    }
    else if (yBot == yTop) {
	(void)printf("pprAreaRescale: y bottom and top are equal\n");
	return;
    }
    pArea->xPixLeft = nint(((double)pWin->width)*pArea->xFracLeft);
    pArea->xPixRight = nint(((double)pWin->width)*pArea->xFracRight);
    pArea->xLeft = xLeft;
    pArea->xRight = xRight;
    pArea->xInterval = (xRight - xLeft) / pArea->xNint;
    pArea->xScale = ((double)pWin->width) *
		(pArea->xFracRight - pArea->xFracLeft) / (xRight - xLeft);
    pArea->yPixBot = nint(((double)pWin->height)*pArea->yFracBot);
    pArea->yPixTop = nint(((double)pWin->height)*pArea->yFracTop);
    pArea->yBot = yBot;
    pArea->yTop = yTop;
    pArea->yInterval = (yTop - yBot) / pArea->yNint;
    pArea->yScale = ((double)pWin->height) *
		(pArea->yFracTop - pArea->yFracBot) / (yTop - yBot);
    pArea->tickHt = (double)pWin->height *
	    PprMin(PprAbs(.03 * (pArea->yFracTop - pArea->yFracBot)), .01);
    if (pArea->charHt <= 0.)
	pArea->charHt = pWin->height *
			PprDfltCharHt(pArea->yFracBot, pArea->yFracTop);
    else
	pArea->charHt = pArea->charHt * pWin->height / pArea->oldWinHt;
    pArea->oldWinHt = pWin->height;
}

/*+/subr**********************************************************************
* NAME	pprAreaSetAttr - set attributes for a plot area
*
* DESCRIPTION
*	Set individual attributes for a plot area.  In most cases, the
*	attributes affect the drawing of lines in the plot area.
*
*	To use this routine, an attribute code and a corresponding value
*	are supplied.  The form of the value argument depends on the code.
*
*     o PPR_ATTR_CLIP sets the plot area so that line segments which lie
*	outside the data area won't be drawn, but will terminate at their
*	intersection with the edge of the data area.  Clipping can be
*	disabled by setting the PPR_ATTR_NOCLIP attribute; the default when
*	a plot area is created is no clipping.
*
*	    pprAreaSetAttr(pArea, PPR_ATTR_CLIP, 0, NULL);
*	    pprAreaSetAttr(pArea, PPR_ATTR_NOCLIP, 0, NULL);
*
*     o PPR_ATTR_COLORNUM selects a color for use in drawing lines in a plot
*	area.  For monochrome screens, no action is taken.  There are
*	PPR_NCOLORS colors provided, numbered starting with 1.  A colorNum
*	of 0 selects black.
*
*	    int		colorNum;
*	    colorNum = 4;
*	    pprAreaSetAttr(pArea, PPR_ATTR_COLORNUM, colorNum, NULL);
*
*     o PPR_ATTR_BG installs a caller-supplied background pixel value in
*	the gc for the plot area.  (For use only with X11.  Under X11,
*	pprAreaOpen initially set the gc for the plot area to have the
*	same foreground and background colors as the gc for the plot window.)
*
*	    pprAreaSetAttr(pArea, PPR_ATTR_BG, 0, &bg);
*
*     o PPR_ATTR_FG installs a caller-supplied foreground pixel value in
*	the gc for the plot area.  (For use only with X11.  Under X11,
*	pprAreaOpen initially set the gc for the plot area to have the
*	same foreground and background colors as the gc for the plot window.)
*
*	    pprAreaSetAttr(pArea, PPR_ATTR_FG, 0, &fg);
*
*     o PPR_ATTR_KEYNUM selects a legend key for identifying lines drawn
*	in a plot area, and thus distinguishing them from the lines drawn
*	by a different plot area.  This is primarily useful for overlapping
*	plot areas, where several sets of data are drawn on the same axis.
*	The key number, which is expected to be in the range 0 to PPR_NKEYS,
*	inclusive, selects either a dashed line pattern or a color,
*	depending on the nature of the device on which the plot window
*	resides.  There are PPR_NKEYS unique patterns and colors; a key
*	number of 0 resets to a "plain", solid line.
*
*	Use of the PPR_ATTR_KEYNUM option provides a way to restart a
*	dashed line pattern at its beginning.
*
*	    int		keyNum;
*	    keyNum = 4;
*	    pprAreaSetAttr(pArea, PPR_ATTR_KEYNUM, keyNum, NULL);
*
*     o PPR_ATTR_LINE_THICK sets the line thickness for the plot area.  The
*	thickness is the integer number of thousandths of plot window height.
*	The thickness is used for data drawing operations.  A thickness of
*	0. results in pixel-thick lines.  As an example, a thickness of 10
*	represents 10/1000 (or .01) of the window height.
*
*	    int		thick;
*	    thick = 3;			.003 of window height
*	    pprAreaSetAttr(pArea, PPR_ATTR_LINE_THICK, thick, NULL);
*
*     o PPR_ATTR_PATT_ARRAY installs a caller-supplied dashed line pattern
*	array.  This is an array of type short.  The first element
*	contains a number of pixels with `pen down'; the second has a
*	number of pixels with `pen up'; the third with `pen down'; etc.
*	Following the last element must be an element with a value of 0  .
*	(pprAreaSetAttr stores only a pointer to the array, so the caller
*	must preserve the array until pprAreaClose is called or until
*	pprAreaSetAttr is called to `de-install' the pattern array.)  An
*	array pointer of NULL resets the plot area back to normal drawing.
*
*	    short	pattern[]={16,4,2,4,0};
*	    pprAreaSetAttr(pArea, PPR_ATTR_PATT_ARRAY, 0, pattern);
*
*	If one of the ppr...Erase routines is to be used in conjunction
*	with a dashed line pattern, then the sequence of operations for
*	erasing must be made the same as the sequence of operations for
*	the original drawing.
*
*
*	Some pprXxx routines don't use the attributes from the plot
*	area, but instead use the `generic' attributes from the
*	plot window structure.  The pprLineSegx_wc and pprMovex_wc
*	routines provide an explicit mechanism for using the plot window
*	attributes.
*
* RETURNS
*	0, or
*	-1 if an error is encountered
*
* BUGS
* o	color is supported only for X
* o	when color is used, ALL output for the plot area is colored; it's
*	not clear yet whether this is a bug or a feature.
* o	line thickness doesn't operate consistently under SunView
*
* SEE ALSO
*	pprAreaOpen
*
*-*/
long
pprAreaSetAttr(pArea, code, arg, pArg)
PPR_AREA *pArea;	/* I pointer to plot area structure */
PPR_ATTR_CODE code;	/* I attribute code: one of PPR_ATTR_xxx */
int	arg;		/* I attribute value, or 0 */
void	*pArg;		/* I pointer to attribute, or NULL */
{
    int		keyNum, colorNum;
    PPR_WIN	*pWin;
#ifdef XWINDOWS
    int		screenNo;
    Colormap	cmap;
    XColor	color, rgbDb;
#endif

    if (code ==						PPR_ATTR_CLIP) {
	pArea->attr.clip = 1;
	return 0;
    }
    if (code ==						PPR_ATTR_NOCLIP) {
	pArea->attr.clip = 0;
	return 0;
    }
    if (code ==						PPR_ATTR_COLORNUM) {
#ifdef XWINDOWS
	pWin = pArea->pWin;
	if (pWin->winType != PPR_WIN_SCREEN)
	    return 0;
	colorNum = arg;
	if (colorNum == 0)
	    XCopyGC(pWin->pDisp, pWin->attr.gc, GCForeground, pArea->attr.gc);
	else {
	    screenNo = DefaultScreen(pWin->pDisp);
	    colorNum = (colorNum - 1) % PPR_NCOLORS;
	    if (!pprWinIsMono(pWin)) {
		cmap = DefaultColormap(pWin->pDisp, screenNo);
		if (XAllocNamedColor(pWin->pDisp, cmap, pglPprColor[colorNum],
							&color, &rgbDb)) {
		    XSetForeground(pWin->pDisp,pArea->attr.gc,color.pixel);
		}
		else
		    return -1;
	    }
	}
#endif
	return 0;
    }
    if (code ==						PPR_ATTR_BG) {
#ifdef XWINDOWS
	pWin = pArea->pWin;
	if (pWin->winType != PPR_WIN_SCREEN)
	    return 0;
	XSetBackground(pWin->pDisp, pArea->attr.gc, *(unsigned long *)pArg);
#endif
	return 0;
    }
    if (code ==						PPR_ATTR_FG) {
#ifdef XWINDOWS
	pWin = pArea->pWin;
	if (pWin->winType != PPR_WIN_SCREEN)
	    return 0;
	XSetForeground(pWin->pDisp, pArea->attr.gc, *(unsigned long *)pArg);
#endif
	return 0;
    }
    if (code ==						PPR_ATTR_KEYNUM) {
	keyNum = arg;
	pWin = pArea->pWin;
	if (keyNum == 0) {
	    pArea->attr.pPatt = NULL;
#ifdef XWINDOWS
	    if (pWin->winType == PPR_WIN_SCREEN)
		XCopyGC(pWin->pDisp,pWin->attr.gc,GCForeground,pArea->attr.gc);
#endif
	}
	else {
	    keyNum = (keyNum - 1) % PPR_NKEYS;
#ifdef XWINDOWS
	    if (pWin->winType == PPR_WIN_SCREEN) {
		screenNo = DefaultScreen(pWin->pDisp);
		if (!pprWinIsMono(pWin)) {
		    cmap = DefaultColormap(pWin->pDisp, screenNo);
		    if (XAllocNamedColor(pWin->pDisp, cmap, pglPprColor[keyNum],
							&color, &rgbDb)) {

			XSetForeground(pWin->pDisp,pArea->attr.gc,color.pixel);
			pArea->attr.pPatt = NULL;
			return 0;
		    }
		}
	    }
#endif
#if 0
	    if (keyNum == 0) {
		pArea->attr.pPatt = NULL;
		return 0;
	    }
#endif
	    pArea->attr.pPatt = pglPprPat[keyNum];
	    pArea->attr.sub = 0;
	    pArea->attr.rem = pArea->attr.pPatt[0];
	    pArea->attr.pen = 1;
	}
	return 0;
    }
    if (code ==						PPR_ATTR_LINE_THICK) {

	pArea->attr.lineThick = arg;
	return 0;
    }
    if (code ==						PPR_ATTR_COLORNUM) {
#ifdef XWINDOWS
	pWin = pArea->pWin;
	if (pWin->winType != PPR_WIN_SCREEN)
	    return 0;
	colorNum = arg;
	if (colorNum == 0)
	    XCopyGC(pWin->pDisp, pWin->attr.gc, GCForeground, pArea->attr.gc);
	else {
	    colorNum = (colorNum - 1) % PPR_NCOLORS;
	    screenNo = DefaultScreen(pWin->pDisp);
	    if (!pprWinIsMono(pWin)) {
		cmap = DefaultColormap(pWin->pDisp, screenNo);
		if (XAllocNamedColor(pWin->pDisp, cmap, pglPprColor[colorNum],
							&color, &rgbDb)) {
		    XSetForeground(pWin->pDisp,pArea->attr.gc,color.pixel);
		}
		else
		    return -1;
	    }
	}
#endif
	return 0;
    }
    if (code ==						PPR_ATTR_PATT_ARRAY) {
	pArea->attr.pPatt = (short *)pArg;
	if (pArg != NULL) {
	    pArea->attr.sub = 0;
	    pArea->attr.rem = pArea->attr.pPatt[0];
	    pArea->attr.pen = 1;
	}
	return 0;
    }
    return -1;
}

/*+/subr**********************************************************************
* NAME	pprAutoEnds - choose `clean' endpoint valuess for an axis
*
* DESCRIPTION
*	For a specific numeric range, this routine calculates a possibly
*	altered numeric range which will produce more tasteful axis
*	calibration.
*
* RETURNS
*	void
*
* BUGS
* o	this routine should probably focus some attention on choice of
*	number of intervals for an axis; presently, the new endpoints
*	chosen by this routine may be difficult to use for choosing
*	interval size
* o	uses exp10(), which doesn't exist in VxWorks
* o	only linear calibration is handled
*
* SEE ALSO
*	pprAutoInterval, pprAutoRange, pprAreaOpen
*
*-*/
void
pprAutoEnds(left, right, pLeftNew, pRightNew)
double	left;		/* I leftmost value */
double	right;		/* I rightmost value */
double	*pLeftNew;	/* O new leftmost value */
double	*pRightNew;	/* O new rightmost value */
{
    double	x1, x2, x1a, x2a, exp1, exp2, new1, new2;
    double	pwr1, pwr2, pwr;

    if (left == right) {
	if (left == 0.) {
	    left = -1.;
	    right = 1.;
	}
	 else if (left < 0.) {
	     left -= 1.;
	     right = 0.;
	 }
	 else {
	     left = 0.;
	     right += 1.;
	 }
    }

/*-----------------------------------------------------------------------------
*    if axis if "reversed", temporarily put it "normal", to make life easy
*----------------------------------------------------------------------------*/
    if (left > right) {		x1 = right;	x2 = left; }
    else {			x1 = left;	x2 = right; }

/*-----------------------------------------------------------------------------
*    now, find a reasonable place to round each end to; the larger magnitude
*    number controls to what "boundary" to round.  Use absolute values in
*    the sleuthing.
*----------------------------------------------------------------------------*/
    x1a = x1 >= 0. ? x1 : -x1;
    if (x1a == 0.)
	pwr1 = x1a;
    else
	pwr1 = exp10((double)((int)log10(x1a)));
    x2a = x2 >= 0. ? x2 : -x2;
    if (x2a == 0.)
	pwr2 = x2a;
    else
	pwr2 = exp10((double)((int)log10(x2a)));
    pwr = pwr1>pwr2 ? pwr1 : pwr2;

/*-----------------------------------------------------------------------------
*    actually do the rounding; and restore the values' original signs
*----------------------------------------------------------------------------*/
    if (x1 < 0.) {	new1 = (1+(int)(x1a/pwr-.0001)) * pwr; new1 = -new1; }
    else		new1 = ((int)(x1a/pwr-.0001)) * pwr;

    if (x2 < 0.) {	new2 = ((int)(x2a/pwr-.0001)) * pwr;	new2 = -new2; }
    else		new2 = (1+(int)(x2a/pwr-.0001)) * pwr;

/*-----------------------------------------------------------------------------
*    unscramble if input was "reversed"; and give values to caller
*----------------------------------------------------------------------------*/
    if (left < right) {
	*pLeftNew = new1;
	*pRightNew = new2;
    }
    else {
	*pLeftNew = new2;
	*pRightNew = new1;
    }
}

/*+/subr**********************************************************************
* NAME	pprAutoInterval - figure out good interval size for an axis
*
* DESCRIPTION
*	Determine a "good" interval size for an axis, so that axis
*	annotation will be tasteful.
*
* RETURNS
*	void
*
* BUGS
* o	this routine always chooses to divide an axis into 5 intervals
* o	only linear calibration is handled
*
* SEE ALSO
*	pprAutoEnds, pprAutoRange, pprAreaOpen
*
*-*/
void
pprAutoInterval(val1, val2, pNint)
double	val1;		/* I value at one end of axis */
double	val2;		/* I value at other end of axis */
int	*pNint;		/* O number of intervals */
{
#define PPR_EQ(v1, v2) PprAbs((v2)-(v1)) <= slop

    double	vmin, vmax;
    double	slop;		/* differences no larger mean == */
    double	diff, ratio, aint;
    int		nInt;

    slop = PprAbs(val2 - val1);

    vmin = PprMin(val1, val2);
    vmax = PprMax(val1, val2);

    if (PPR_EQ(vmin, 0.))
	/* pprAutoInt(vmax, &nInt); */
	;
    else if (PPR_EQ(vmax, 0.))
	/* pprAutoInt(vmin, &nInt); */
	;
    else if (vmin < 0. && vmax > 0.) {
	;
    }
    nInt = 5.;
    *pNint = nInt;
}

/*+/macro*********************************************************************
* NAME	pprAutoRange - find minimum and maximum values for an array
*
* DESCRIPTION
*	Finds the minimum and maximum values in an array of values.
*
*	Four different routines are available, depending on the type of
*	the input array.  Each returns the min and max as a double:
*
*	void pprAutoRangeD(doubleArray, nPoints, doubleMin, doubleMax)
*	void pprAutoRangeF(floatArray, nPoints, doubleMin, doubleMax)
*	void pprAutoRangeL(longArray, nPoints, doubleMin, doubleMax)
*	void pprAutoRangeS(shortArray, nPoints, doubleMin, doubleMax)
*
* RETURNS
*	void
*
* SEE ALSO
*	pprAutoEnds, pprAutoInterval, pprAreaOpen
*
*-*/
void pprAutoRangeD(dblArray, npts, dblMin, dblMax)
double	*dblArray;	/* I data value array */
int	npts;		/* I number of data points */
double	*dblMin;	/* O minimum value in array */
double	*dblMax;	/* O maximum value in array */
{
    int		i;

    *dblMin = *dblMax = dblArray[0];
    for (i=1; i<npts; i++) {
	if (*dblMin > dblArray[i])	*dblMin = dblArray[i];
	if (*dblMax < dblArray[i])	*dblMax = dblArray[i];
    }
}
void pprAutoRangeF(fltArray, npts, dblMin, dblMax)
float	*fltArray;	/* I data value array */
int	npts;		/* I number of data points */
double	*dblMin;	/* O minimum value in array */
double	*dblMax;	/* O maximum value in array */
{
    int		i;
    *dblMin = *dblMax = fltArray[0];
    for (i=1; i<npts; i++) {
	if (*dblMin > fltArray[i])	*dblMin = fltArray[i];
	if (*dblMax < fltArray[i])	*dblMax = fltArray[i];
    }
}
void pprAutoRangeL(lngArray, npts, dblMin, dblMax)
long	*lngArray;	/* I data value array */
int	npts;		/* I number of data points */
double	*dblMin;	/* O minimum value in array */
double	*dblMax;	/* O maximum value in array */
{
    int		i;
    *dblMin = *dblMax = lngArray[0];
    for (i=1; i<npts; i++) {
	if (*dblMin > lngArray[i])	*dblMin = lngArray[i];
	if (*dblMax < lngArray[i])	*dblMax = lngArray[i];
    }
}
void pprAutoRangeS(shtArray, npts, dblMin, dblMax)
short	*shtArray;	/* I data value array */
int	npts;		/* I number of data points */
double	*dblMin;	/* O minimum value in array */
double	*dblMax;	/* O maximum value in array */
{
    int		i;
    *dblMin = *dblMax = shtArray[0];
    for (i=1; i<npts; i++) {
	if (*dblMin > shtArray[i])	*dblMin = shtArray[i];
	if (*dblMax < shtArray[i])	*dblMax = shtArray[i];
    }
}

static int	pprTrigInit=0;
static double	pprCos[361];	/* cos for 0-90, in .25 steps */
/*+/subr**********************************************************************
* NAME	pprCos_deg - get the cosine of an angle in degrees
*
* DESCRIPTION
*	Get the cosine of an angle in degrees.  A table lookup technique
*	is used.  The angle argument is truncated to the next lower 1/4
*	degree prior to lookup.
*
* RETURNS
*	cosine
*
*-*/
double
pprCos_deg(angle)
double	angle;		/* I angle, in degrees */
{
    int		indx;
    double	cosine;

    if (!pprTrigInit) {
	int	i;
	double	angle;

	pprTrigInit = 1;
	for (i=0; i<=360; i++) {
	    angle = (double)i / 4. * .017453292;
	    pprCos[i] = cos(angle);
	}
    }
    indx = angle * 4.;
    if (indx < 0)
	indx = -indx;
    if (indx >= 1440)
	indx %= 1440;
    if (indx <= 360)
	cosine = pprCos[indx];
    else if ((indx -= 360) <= 360)
	cosine = -pprCos[360 - indx];
    else if ((indx -= 360) <= 360)
	cosine = -pprCos[indx];
    else {
	indx -= 360;
	cosine = pprCos[360 - indx];
    }
    return cosine;
}

/*+/subr**********************************************************************
* NAME	pprChar - plot a character
*
* DESCRIPTION
*	Plots a single text character at a location.  The center of the
*	character is placed at the specified x,y position.
*
*	The character height specification is in terms of a fraction of the
*	height of the window.  This results in automatic scaling of
*	character sizes as the window size changes.  If a height of zero
*	is specified in the call, then the default height for the plot
*	area is used.  (The default height was established by pprAreaOpen.)
*
*	A macro is available which returns the default character height
*	used by this plot package.  The value returned is proportional to
*	the height of the plot area.  This value can be used to generate
*	"big" and "small" character sizes.
*
*		PprDfltCharHt(lowYfrac, highYfrac)
*
*		lowYfrac is the vertical fraction of the window at which
*			the bottom edge of the plot area lies
*		highYfrac is the vertical fraction of the window at which
*			the top edge of the plot area lies
*
* RETURNS
*	void
*
* BUGS
* o	technique used works only with linear axes
* o	ASCII character codes are assumed
* o	no checks are made for illegal characters
*
* SEE ALSO
*	pprText, pprAreaOpen, pprLine, pprPoint
*
*-*/
void
pprChar(pArea, x, y, chr, height, angle)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate of character */
double	y;		/* I y data coordinate of character */
char	chr;		/* I character to plot */
double	height;		/* I height of character, as a fraction of
				the height of the window; a value of
				zero results in using a default height */
double	angle;		/* I orientation angle of character, ccw degrees */
{
    double	xWin, yWin;
    double	scale;		/* convert character units to win coord */
    double	cosT, sinT;
    char	str[2];

    if (height <= 0.)
        height = pArea->charHt;
    else
        height *= pArea->pWin->height;
    xWin = pArea->xPixLeft + nint((x - pArea->xLeft) * pArea->xScale);
    yWin = pArea->yPixBot + nint((y - pArea->yBot) * pArea->yScale);

    if (pArea->pWin->winType == PPR_WIN_SCREEN) {
	if (angle == 0.)	cosT = 1., sinT = 0.;
	else if (angle == 90.)	cosT = 0., sinT = 1.;
	else {			cosT = pprCos_deg(angle);
				sinT = pprSin_deg(angle);
	}
	scale = height / 6.;
	pprText1(pArea,
		    xWin, yWin, chr, 0, scale, sinT, cosT, pprLineSegPixD_ac);
    }
    else {
	height = 1.5 * height;
	str[0] = chr;
	str[1] = '\0';
	pprTextPS(pArea->pWin->file, xWin, yWin,PPR_TXT_CEN,str,height,angle);
    }
}

/*+/subr**********************************************************************
* NAME	pprCvtDblToTxt - format a double for printing
*
* DESCRIPTION
*	Formats a double for printing.  This routine is dedicated to
*	getting as large a range of values as possible into a particular
*	field width.
*
*	This routine doesn't attempt to handle extremely small values.
*	It assumes that the field is large enough to handle the smallest
*	significant value to be encountered.
*
* RETURNS
*	void
*
* BUGS
* o	extremely small values aren't handled well
*
* NOTES
* 1.	If the value can't be represented at all in the field, the sign
*	followed by *'s appears.
* 2.	In extreme cases, only the magnitude of the value will appear, as
*	En or Enn.  For negative values, a - will precede the E.
* 3.	When appropriate, the value is rounded to the nearest integer
*	for formatting.
*
*-*/
void
pprCvtDblToTxt(text, width, value, sigDig)
char	*text;		/* O text representation of value */
int	width;		/* I max width of text string (not counting '\0') */
double	value;		/* I value to print */
int	sigDig;		/* I max # of dec places to print */
{
    double	valAbs;		/* absolute value of caller's value */
    int		wholeNdig;	/* number of digits in "whole" part of value */
    double	logVal;		/* log10 of value */
    int		decPlaces;	/* number of decimal places to print */
    int		expWidth;	/* width needed for exponent field */
    int		excess;		/* number of low order digits which
				    won't fit into the field */

    if (value == 0.) {
	(void)strcpy(text, "0");
	return;
    }

/*-----------------------------------------------------------------------------
*    find out how many columns are required to represent the integer part
*    of the value.  A - is counted as a column;  the . isn't.
*----------------------------------------------------------------------------*/
    valAbs = value>0 ? value : -value;
    logVal = log10(valAbs);
    wholeNdig = 1 + (int)logVal;
    if (wholeNdig < 0)
	wholeNdig = 1;
    if (value < 0.)
	wholeNdig++;
    if (wholeNdig < width-1) {
/*-----------------------------------------------------------------------------
*    the integer part fits well within the field.  Find out how many
*    decimal places can be printed (honoring caller's sigDig limit).
*----------------------------------------------------------------------------*/
	decPlaces = width - wholeNdig - 1;
	if (sigDig < decPlaces)
	    decPlaces = sigDig;
	if (sigDig > 0)
	    (void)sprintf(text, "%.*f", decPlaces, value);
	else
	    (void)sprintf(text, "%d", nint(value));
    }
    else if (wholeNdig == width || wholeNdig == width-1) {
/*-----------------------------------------------------------------------------
*    The integer part just fits within the field.  Print the value as an
*    integer, without printing the superfluous decimal point.
*----------------------------------------------------------------------------*/
	(void)sprintf(text, "%d", nint(value));
    }
    else {
/*-----------------------------------------------------------------------------
*    The integer part is too large to fit within the caller's field.  Print
*    with an abbreviated E notation.
*----------------------------------------------------------------------------*/
	expWidth = 2;				/* assume that En will work */
	excess = wholeNdig - (width - 2);
	if (excess > 999) {
	    expWidth = 5;			/* no! it must be Ennnn */
	    excess += 3;
	}
	else if (excess > 99) {
	    expWidth = 4;			/* no! it must be Ennn */
	    excess += 2;
	}
	else if (excess > 9) {
	    expWidth = 3;			/* no! it must be Enn */
	    excess += 1;
	}
/*-----------------------------------------------------------------------------
*	Four progressively worse cases, with all or part of exponent fitting
*	into field, but not enough room for any of the value
*		Ennn		positive value; exponent fits
*		-Ennn		negative value; exponent fits
*		+****		positive value; exponent too big
*		-****		negative value; exponent too big
*----------------------------------------------------------------------------*/
	if (value >= 0. && expWidth == width)
	    (void)sprintf(text, "E%d", nint(logVal));
	else if (value < 0. && expWidth == width-1)
	    (void)sprintf(text, "-E%d", nint(logVal));
	else if (value > 0. && expWidth > width)
	    (void)sprintf(text, "%.*s", width, "+*******");
	else if (value < 0. && expWidth > width-1)
	    (void)sprintf(text, "%.*s", width, "-*******");
	else {
/*-----------------------------------------------------------------------------
*	The value can fit, in exponential notation
*----------------------------------------------------------------------------*/
	    (void)sprintf(text, "%dE%d",
			nint(value/exp10((double)excess)), excess);
	}
    }
}

/*+/subr**********************************************************************
* NAME	pprErrorBar - plot an error bar
*
* DESCRIPTION
*	Plot a line between the endpoints and draw end caps, using the color
*	attribute of the plot area.  Other plot area attributes are ignored.
*
* RETURNS
*	void
*
* BUGS
* o	only linear calibration is handled
*
* SEE ALSO
*	pprLine, pprMove, pprPoint, pprText
*
*-*/
void
pprErrorBar(pArea, x1, y1, x2, y2)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x1;		/* I first x point */
double	y1;		/* I first y point */
double	x2;		/* I second x point */
double	y2;		/* I second y point */
{
    pprLineSegD_ac(pArea, x1, y1, x2, y2);
    if (x1 == x2) {
	pprMarkD(pArea, x1, y1, -1);
	pprMarkD(pArea, x2, y2, -1);
    }
    else {
	pprMarkD(pArea, x1, y1, -2);
	pprMarkD(pArea, x2, y2, -2);
    }
}

/*+/subr**********************************************************************
* NAME	pprGrid - draw a grid
*
* DESCRIPTION
*	Draw a perimeter and grid lines.  The number of intervals
*	specified in the plot area structure is used for placing
*	the grid lines.
*
*	The color attributes for the plot window are used for drawing;
*	dashed line and line thickness are ignored.
*
* RETURNS
*	void
*
* BUGS
* o	only linear axes are handled
*
* SEE ALSO
*	pprGridLabel, pprGridErase, pprPerim, pprAnnotX, pprAnnotY, pprAreaOpen
*
*-*/
void
pprGrid(pArea)
PPR_AREA *pArea;	/* I pointer to plot area structure */
{
    int		i;
    double	x, y;
    static short patt[]={1, 14, 0};

/*-----------------------------------------------------------------------------
*    draw the box
*----------------------------------------------------------------------------*/
    pprMoveD_wc(pArea, pArea->xLeft, pArea->yBot, 0);
    pprMoveD_wc(pArea, pArea->xRight, pArea->yBot, 1);
    pprMoveD_wc(pArea, pArea->xRight, pArea->yTop, 1);
    pprMoveD_wc(pArea, pArea->xLeft, pArea->yTop, 1);
    pprMoveD_wc(pArea, pArea->xLeft, pArea->yBot, 1);
/*-----------------------------------------------------------------------------
*    draw the vertical grid lines
*----------------------------------------------------------------------------*/
    x = pArea->xLeft;
    for (i=1; i<pArea->xNint; i++) {
	x += pArea->xInterval;
	pprLineSegDashD_wc(pArea, x, pArea->yBot, x, pArea->yTop, patt);
    }
/*-----------------------------------------------------------------------------
*    ditto for y axis
*----------------------------------------------------------------------------*/
    y = pArea->yBot;
    for (i=1; i<pArea->yNint; i++) {
	y += pArea->yInterval;
	pprLineSegDashD_wc(pArea, pArea->xLeft, y, pArea->xRight, y, patt);
    }
}

/*+/subr**********************************************************************
* NAME	pprGridErase - erase within a grid
*
* DESCRIPTION
*	Erases the screen inside the grid for the plot area.  (Actually,
*	the entire data area is erased and then the grid is redrawn.)
*
* RETURNS
*	void
*
* SEE ALSO
*	pprPerimErase, pprAreaErase, pprWinErase, pprRegionErase
*	the ppr...Erase... entry points for the various drawing routines
*
*-*/
void
pprGridErase(pArea)
PPR_AREA *pArea;	/* I pointer to plot area structure */
{
    int		x,y,width,height;

    if (pArea->pWin->winType != PPR_WIN_SCREEN)
	return;
    x = pArea->xPixLeft - 3;
    y = pArea->yPixBot - 3;
    width = (pArea->xRight - pArea->xLeft) * pArea->xScale + 6;
    height = (pArea->yTop - pArea->yBot) * pArea->yScale + 6;
#ifdef SUNVIEW
    y = pArea->pWin->height - y - height;
    pw_writebackground(pArea->pWin->pw, x, y, width, height, PIX_SRC);
    pprGrid(pArea);
#elif defined XWINDOWS
    y = pArea->pWin->height - y - height;
    XClearArea(pArea->pWin->pDisp, pArea->pWin->plotWindow,
						x, y, width, height, False);
    pprGrid(pArea);
    XFlush(pArea->pWin->pDisp);
#endif
}

/*+/subr**********************************************************************
* NAME	pprGridLabel - draw and label a grid
*
* DESCRIPTION
*	Draw a perimeter and grid lines.  The number of intervals
*	specified in the plot area structure is used for placing
*	the grid lines.
*
*	Axis labels and annotations are drawn using the information from
*	the plot area, as specified in the pprAreaOpen call.
*
* RETURNS
*	void
*
* BUGS
* o	only linear axes are handled
*
* SEE ALSO
*	pprGrid, pprPerim, pprAnnotX, pprAnnotY, pprAreaOpen
*
*-*/
void
pprGridLabel(pArea, xLabel, xAnnot, yLabel, yAnnot, angle)
PPR_AREA *pArea;	/* I pointer to plot area structure */
char	*xLabel;	/* I label for x axis, or NULL */
char	**xAnnot;	/* I pointer to array of x annotations, or NULL */
char	*yLabel;	/* I label for y axis, or NULL */
char	**yAnnot;	/* I pointer to array of y annotations, or NULL */
double	angle;		/* I angle for y annotations; 0. or 90. */
{
    pprGrid(pArea);
    pprAnnotX(pArea, 0, pArea->xLeft, pArea->xRight, pArea->xNint,
						0, xLabel, xAnnot, 0.);
    pprAnnotY(pArea, 0, pArea->yBot, pArea->yTop, pArea->yNint,
						0, yLabel, yAnnot, angle);
}

/*+/subr**********************************************************************
* NAME	pprLine - plot a line using x and y data value vectors
*
* DESCRIPTION
*	Draw a line along the path specified by two value arrays.
*
*	Several entry points are available to accomodate various
*	types of data:
*
*		pprLineF(pArea, x, y, npts)	x and y are float[]
*		pprLineD(pArea, x, y, npts)	x and y are double[]
*		pprLineS(pArea, x, y, npts)	x and y are short[]
*		pprLineL(pArea, x, y, npts)	x and y are long[]
*
*	Several entry points are available for erasing:
*
*		pprLineEraseF(pArea, x, y, npts)	x and y are float[]
*		pprLineEraseD(pArea, x, y, npts)	x and y are double[]
*		pprLineEraseS(pArea, x, y, npts)	x and y are short[]
*		pprLineEraseL(pArea, x, y, npts)	x and y are long[]
*
* RETURNS
*	void
*
* BUGS
* o	only linear calibration is handled
*
* SEE ALSO
*	pprLineSeg, pprMove, pprPoint, pprText
*
*-*/
void
pprLineF(pArea, x, y, npts)
PPR_AREA *pArea;	/* I pointer to plot area structure */
float	*x;		/* I x array of data values */
float	*y;		/* I y array of data values */
int	npts;		/* I number of points to plot */
{
    int		i;
    pprMoveD(pArea, (double)(x[0]), (double)(y[0]), 0);
    for (i=1; i<npts; i++) 
	pprMoveD(pArea, (double)(x[i]), (double)(y[i]), 1);
}
void
pprLineD(pArea, x, y, npts)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	*x;		/* I x array of data values */
double	*y;		/* I y array of data values */
int	npts;		/* I number of points to plot */
{
    int		i;
    pprMoveD(pArea, x[0], y[0], 0);
    for (i=1; i<npts; i++) 
	pprMoveD(pArea, x[i], y[i], 1);
}
void
pprLineS(pArea, x, y, npts)
PPR_AREA *pArea;	/* I pointer to plot area structure */
short	*x;		/* I x array of data values */
short	*y;		/* I y array of data values */
int	npts;		/* I number of points to plot */
{
    int		i;
    pprMoveD(pArea, (double)x[0], (double)y[0], 0);
    for (i=1; i<npts; i++) 
	pprMoveD(pArea, (double)x[i], (double)y[i], 1);
}
void
pprLineL(pArea, x, y, npts)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	*x;		/* I x array of data values */
long	*y;		/* I y array of data values */
int	npts;		/* I number of points to plot */
{
    int		i;
    pprMoveD(pArea, (double)x[0], (double)y[0], 0);
    for (i=1; i<npts; i++) 
	pprMoveD(pArea, (double)x[i], (double)y[i], 1);
}
void
pprLineEraseF(pArea, x, y, npts)
PPR_AREA *pArea;	/* I pointer to plot area structure */
float	*x;		/* I x array of data values */
float	*y;		/* I y array of data values */
int	npts;		/* I number of points to plot */
{
    int		i;
    pprMoveD(pArea, (double)(x[0]), (double)(y[0]), 0);
    for (i=1; i<npts; i++) 
	pprMoveEraseD(pArea, (double)(x[i]), (double)(y[i]));
}
void
pprLineEraseD(pArea, x, y, npts)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	*x;		/* I x array of data values */
double	*y;		/* I y array of data values */
int	npts;		/* I number of points to plot */
{
    int		i;
    pprMoveD(pArea, x[0], y[0], 0);
    for (i=1; i<npts; i++) 
	pprMoveEraseD(pArea, x[i], y[i]);
}
void
pprLineEraseS(pArea, x, y, npts)
PPR_AREA *pArea;	/* I pointer to plot area structure */
short	*x;		/* I x array of data values */
short	*y;		/* I y array of data values */
int	npts;		/* I number of points to plot */
{
    int		i;
    pprMoveD(pArea, (double)x[0], (double)y[0], 0);
    for (i=1; i<npts; i++) 
	pprMoveEraseD(pArea, (double)x[i], (double)y[i]);
}
void
pprLineEraseL(pArea, x, y, npts)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	*x;		/* I x array of data values */
long	*y;		/* I y array of data values */
int	npts;		/* I number of points to plot */
{
    int		i;
    pprMoveD(pArea, (double)x[0], (double)y[0], 0);
    for (i=1; i<npts; i++) 
	pprMoveEraseD(pArea, (double)x[i], (double)y[i]);
}

/*+/subr**********************************************************************
* NAME	pprLineSeg - plot a line segment between a pair of points
*
* DESCRIPTION
*	Move to the first point and then draw a line to the second, using
*	the line attributes of the plot area.  If the attributes indicate a
*	dashed line, the current dashed line pattern will be used.
*
*	Two entry points are available:
*
*		pprLineSegD(pArea, x1, y1, x2, y2)	x and y are double
*		pprLineSegL(pArea, x1, y1, x2, y2)	x and y are long
*
*	For drawing ignoring the dashed line, line thickness, and other plot
*	area attributes, but using the area color attribute, two alternate
*	entry points are available:
*
*		pprLineSegD_ac(pArea, x1, y1, x2, y2)	x and y are double
*		pprLineSegL_ac(pArea, x1, y1, x2, y2)	x and y are long
*
*	Two entry points are available for erasing:
*
*		pprLineSegEraseD(pArea, x1, y1, x2, y2)	x and y are double
*		pprLineSegEraseL(pArea, x1, y1, x2, y2)	x and y are long
* RETURNS
*	void
*
* BUGS
* o	only linear calibration is handled
*
* SEE ALSO
*	pprLine, pprMove, pprPoint, pprText
*
*-*/
void
pprLineSegD(pArea, x1, y1, x2, y2)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x1;		/* I first x point */
double	y1;		/* I first y point */
double	x2;		/* I second x point */
double	y2;		/* I second y point */
{
    pprMoveD(pArea, x1, y1, 0);
    pprMoveD(pArea, x2, y2, 1);
}
void
pprLineSegL(pArea, x1, y1, x2, y2)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	x1;		/* I first x point */
long	y1;		/* I first y point */
long	x2;		/* I second x point */
long	y2;		/* I second y point */
{
    pprMoveD(pArea, (double)x1, (double)y1, 0);
    pprMoveD(pArea, (double)x2, (double)y2, 1);
}
void
pprLineSegD_ac(pArea, x1, y1, x2, y2)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x1;		/* I first x point */
double	y1;		/* I first y point */
double	x2;		/* I second x point */
double	y2;		/* I second y point */
{
    pprMoveD_ac(pArea, x1, y1, 0);
    pprMoveD_ac(pArea, x2, y2, 1);
}
void
pprLineSegD_wc(pArea, x1, y1, x2, y2)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x1;		/* I first x point */
double	y1;		/* I first y point */
double	x2;		/* I second x point */
double	y2;		/* I second y point */
{
    pprMoveD_wc(pArea, x1, y1, 0);
    pprMoveD_wc(pArea, x2, y2, 1);
}
void
pprLineSegL_ac(pArea, x1, y1, x2, y2)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	x1;		/* I first x point */
long	y1;		/* I first y point */
long	x2;		/* I second x point */
long	y2;		/* I second y point */
{
    pprMoveD_ac(pArea, (double)x1, (double)y1, 0);
    pprMoveD_ac(pArea, (double)x2, (double)y2, 1);
}
void
pprLineSegEraseD(pArea, x1, y1, x2, y2)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x1;		/* I first x point */
double	y1;		/* I first y point */
double	x2;		/* I second x point */
double	y2;		/* I second y point */
{
    pprMoveD(pArea, x1, y1, 0);
    pprMoveEraseD(pArea, x2, y2);
}
void
pprLineSegEraseL(pArea, x1, y1, x2, y2)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	x1;		/* I first x point */
long	y1;		/* I first y point */
long	x2;		/* I second x point */
long	y2;		/* I second y point */
{
    pprMoveD(pArea, (double)x1, (double)y1, 0);
    pprMoveEraseD(pArea, (double)x2, (double)y2);
}

/*+/internal******************************************************************
* NAME	pprLineSegPixD - line segment routine private to pprPlot
*
* DESCRIPTION
*	Provides `nitty-gritty' interface to the various platforms.
*
*	pprLineSegPixD_ac - uses attributes for plot area: thick, color
*	pprLineSegPixD_wc - uses attributes for plot window: thick, color
*
* RETURNS
*	void
*
*-*/
#ifdef SUNVIEW
short glSvPattern[3]={100,0,0};	/* line pattern to make pw_line happy */
struct pr_texture texture;
static int	initTexture=1;
static initTex()
{
    bzero((char *)&texture, sizeof(texture));
    texture.pattern = glSvPattern;
    texture.offset = 0;
    texture.options.startpoint = 1;
    texture.options.endpoint = 1;
    texture.options.givenpattern = 1;
    initTexture = 0;
}
#endif
static void
pprLineSegPixD_ac(pArea, xp0, yp0, xp1, yp1)
PPR_AREA *pArea;
double	xp0, xp1, yp0, yp1;	/* y must be corrected properly by the caller
				for the windowing system being used.  I.e.,
				most of the pprXxx routines assume 0,0 is
				lower left, but X and SunView assume it is
				upper left--the caller must have dealt with
				this. */
{
#ifdef SUNVIEW
    if (initTexture)
	initTex();
#endif
    if (pArea->pWin->winType == PPR_WIN_SCREEN) {
#ifdef SUNVIEW
        if (pArea->pWin->brush.width > 1)
	    pw_line(pArea->pWin->pw, (int)xp0, (int)yp0, (int)xp1, (int)yp1,
		&pArea->pWin->brush, &texture, (int)PIX_SRC);
        else
	    pw_vector(pArea->pWin->pw, (int)xp0, (int)yp0, (int)xp1, (int)yp1,
			PIX_SRC, 1);
#elif defined XWINDOWS
	XDrawLine(pArea->pWin->pDisp, pArea->pWin->plotWindow,
			pArea->attr.gc, (int)xp0, (int)yp0, (int)xp1, (int)yp1);
#endif
    }
    else if (pArea->pWin->winType == PPR_WIN_POSTSCRIPT ||
				pArea->pWin->winType == PPR_WIN_EPS) {
	(void)fprintf(pArea->pWin->file, "%.1f %.1f %.1f %.1f DS\n",
							xp0, yp0, xp1, yp1);
    }
}
static void
pprLineSegPixD_wc(pArea, xp0, yp0, xp1, yp1)
PPR_AREA *pArea;
double	xp0, xp1, yp0, yp1;	/* y must be corrected properly by the caller
				for the windowing system being used.  I.e.,
				most of the pprXxx routines assume 0,0 is
				lower left, but X and SunView assume it is
				upper left--the caller must have dealt with
				this. */
{
#ifdef SUNVIEW
    if (initTexture)
	initTex();
#endif
    if (pArea->pWin->winType == PPR_WIN_SCREEN) {
#ifdef SUNVIEW
        if (pArea->pWin->brush.width > 1)
	    pw_line(pArea->pWin->pw, (int)xp0, (int)yp0, (int)xp1, (int)yp1,
		&pArea->pWin->brush, &texture, (int)PIX_SRC);
        else
	    pw_vector(pArea->pWin->pw, (int)xp0, (int)yp0, (int)xp1, (int)yp1,
			PIX_SRC, 1);
#elif defined XWINDOWS
	XDrawLine(pArea->pWin->pDisp, pArea->pWin->plotWindow,
		pArea->pWin->attr.gc, (int)xp0, (int)yp0, (int)xp1, (int)yp1);
#endif
    }
    else if (pArea->pWin->winType == PPR_WIN_POSTSCRIPT ||
				pArea->pWin->winType == PPR_WIN_EPS) {
	(void)fprintf(pArea->pWin->file, "%.1f %.1f %.1f %.1f DS\n",
							xp0, yp0, xp1, yp1);
    }
}
static void
pprLineSegPixEraseD(pArea, xp0, yp0, xp1, yp1)
PPR_AREA *pArea;
double	xp0, xp1, yp0, yp1;	/* y must be corrected properly by the caller
				for the windowing system being used.  I.e.,
				most of the pprXxx routines assume 0,0 is
				lower left, but X and SunView assume it is
				upper left--the caller must have dealt with
				this. */
{
#ifdef SUNVIEW
    if (initTexture)
	initTex();
#endif
    if (pArea->pWin->winType != PPR_WIN_SCREEN)
	return;
#ifdef SUNVIEW
    if (pArea->pWin->brush.width > 1)
	pw_line(pArea->pWin->pw, (int)xp0, (int)yp0, (int)xp1, (int)yp1,
		&pArea->pWin->brush, &texture, (int)(PIX_NOT(PIX_SRC)&PIX_DST));
    else
	pw_vector(pArea->pWin->pw, (int)xp0, (int)yp0, (int)xp1, (int)yp1,
			PIX_SRC, 0);
#elif defined XWINDOWS
    XDrawLine(pArea->pWin->pDisp, pArea->pWin->plotWindow,
	    pArea->pWin->attr.gcBG, (int)xp0, (int)yp0, (int)xp1, (int)yp1);
#endif
}

/*+/internal******************************************************************
* NAME	pprLineSegDashD_wc - drawing a dashed line segment
*
* DESCRIPTION
*	Draws a dashed line between the specified points.  The dashed line
*	starts at the beginning of the specified pattern.  The dashes are
*	drawn using the plot window color.
*
*	(The dashed line attributes for the plot area are neither used nor
*	altered.)
*
*	No clipping service is provided.
*
* RETURNS
*	void
*
*-*/
static void
pprLineSegDashD_wc(pArea, x0, y0, x1, y1, pPatt)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x0;		/* I x data coordinate of first point */
double	y0;		/* I y data coordinate of first point */
double	x1;		/* I x data coordinate of second point */
double	y1;		/* I y data coordinate of second point */
short	*pPatt;		/* I pointer to pattern array */
{
    double	xp0,yp0,xp1,yp1;
    double	xpA,ypA,xpB;
    double	segLen, endLen, dashLen, xbeg, xend, ybeg, yend;
    int		pen=0, sub=-1, rem=0;

    xbeg = xp0 = pArea->xPixLeft + nint((x0 - pArea->xLeft) * pArea->xScale);
    ybeg = yp0 = pArea->yPixBot + nint((y0 - pArea->yBot) * pArea->yScale);
    if (pArea->pWin->winType == PPR_WIN_SCREEN)
	ybeg = yp0 = pArea->pWin->height - yp0;
    xp1 = pArea->xPixLeft + nint((x1 - pArea->xLeft) * pArea->xScale);
    yp1 = pArea->yPixBot + nint((y1 - pArea->yBot) * pArea->yScale);
    if (pArea->pWin->winType == PPR_WIN_SCREEN)
	yp1 = pArea->pWin->height - yp1;
    pprLineThick(pArea, pArea->pWin->attr.lineThick);
    segLen = sqrt((xp1-xp0)*(xp1-xp0) + (yp1-yp0)*(yp1-yp0));
    endLen = 0.;
    while (endLen < segLen) {
	if (rem < 1.) {
	    if ((rem = pPatt[++sub]) <= 0.) {
		sub = 0; rem = pPatt[0]; pen = 1;
	    }
	    else
		pen ^= 1;
	}
	dashLen = PprMin(rem, segLen-endLen);
	endLen += dashLen;
	if (PprAbs(endLen-segLen) < .5)
	    endLen = segLen;
	xend = xp0 + endLen/segLen * (xp1 - xp0);
	yend = yp0 + endLen/segLen * (yp1 - yp0);
	if (pen) {
	    if (dashLen > 1.) pprLineSegPixD_wc(pArea, xbeg, ybeg, xend, yend);
	    else {
		if (pArea->pWin->winType == PPR_WIN_SCREEN) {
#ifdef SUNVIEW
		    if (pArea->pWin->attr.ltPix > 1) {
			ypA = ybeg;
			xpA = xbeg - pArea->pWin->attr.ltPix / 2;
			xpB = xpA + pArea->pWin->attr.ltPix - 1;
#elif defined XWINDOWS
		    if (pArea->attr.ltPix > 1) {
			ypA = ybeg;
			xpA = xbeg - pArea->attr.ltPix / 2;
			xpB = xpA + pArea->attr.ltPix - 1;
#endif
			/* for thick lines, draw a square 'blob' */
			pprLineSegPixD_wc(pArea, xpA, ypA, xpB, ypA);
		    }
		    else {
#ifdef SUNVIEW
			pw_put(pArea->pWin->pw, (int)xbeg, (int)ybeg, 1);
#elif defined XWINDOWS
			XDrawPoint(pArea->pWin->pDisp, pArea->pWin->plotWindow,
				pArea->pWin->attr.gc, (int)xbeg, (int)ybeg);
#endif
		    }
		}
		else if (pArea->pWin->winType == PPR_WIN_POSTSCRIPT ||
					pArea->pWin->winType == PPR_WIN_EPS) {
		   (void)fprintf(pArea->pWin->file,"%.1f %.1f DP\n",xbeg,ybeg);
		}
	    }
	}
	xbeg = xend; ybeg = yend;
	rem -= dashLen;
    }
}

/*+/internal******************************************************************
* NAME	pprLineThick - set line thickness
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	for X, each linewidth ought to have its own gc
* o	doesn't work consistently for SunView (especially after first draw)
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
pprLineThick(pArea, thick)
PPR_AREA *pArea;
short	thick;	/* I thickness in thousandths of window height */
{
    PPR_WIN	*pWin;
    double	lt;		/* line thickness */
    short	ltPix;		/* line thickness, in pixels */

    pWin = pArea->pWin;

    if (pWin->winType==PPR_WIN_POSTSCRIPT || pWin->winType==PPR_WIN_EPS) {
	if (thick != pWin->attr.ltCurr) {
	    pWin->attr.ltCurr = thick;
	    if (thick == 0)	lt = .1;
	    else		lt = .001 * thick * pWin->height;
	    (void)fprintf(pWin->file, "%.1f setlinewidth\n", lt);
	}
    }
    else {
#ifdef SUNVIEW
	if (thick != pWin->attr.ltCurr) {
	    pWin->attr.ltCurr = thick;
#elif defined XWINDOWS
	if (thick != pArea->attr.ltCurr) {
	    pArea->attr.ltCurr = thick;
#endif
	    if (thick == 0)
		ltPix = 1;
	    else {
		lt = .001 * thick * pWin->height;
		if (lt < 1.) ltPix = 1;
		else	ltPix = (int)(lt + .5);
	    }
#ifdef SUNVIEW
	    pWin->attr.ltPix = ltPix;
            pWin->brush.width = ltPix;
#elif defined XWINDOWS
	    pArea->attr.ltPix = ltPix;
	    XSetLineAttributes(pWin->pDisp, pArea->attr.gc, ltPix, LineSolid,
				CapButt, JoinRound);
	    if (pArea->pWin->attr.bgGC) {
		XSetLineAttributes(pWin->pDisp, pArea->pWin->attr.gcBG,
				ltPix, LineSolid, CapButt, JoinRound);
	    }
#endif
	}
    }
}

/*+/subr**********************************************************************
* NAME	pprMarkD - draw a plotting mark
*
* DESCRIPTION
*	Draw a plotting mark at the specified point.  The color attribute
*	(if any) of the plot area is used in drawing the mark.  Line
*	thickness and dashed line pattern attributes are ignored.
*
*	Two entry points are available:
*
*		pprMarkD(pArea, x, y, markNum)	x and y are double
*		pprMarkL(pArea, x, y, markNum)	x and y are long
*
*	Two additional entry points are available for erasing plotting marks:
*
*		pprMarkEraseD(pArea, x, y, markNum)	x and y are double
*		pprMarkEraseL(pArea, x, y, markNum)	x and y are long
*
* RETURNS
*	void
*
* NOTES
* 1.	Several special mark numbers are available:
*	-1	draws a short horizontal line "cap"
*	-2	draws a short vertical line "cap"
*
* BUGS
* o	only linear calibration is handled
*
* SEE ALSO
*	pprLine, pprLineSeg, pprPoint, pprText
*
*-*/
void
pprMarkD(pArea, x, y, markNum)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate */
double	y;		/* I y data coordinate */
int	markNum;	/* I mark number--0 to PPR_NMARKS-1, inclusive */
{
    pprMark_gen(pArea, x, y, markNum, pprLineSegPixD_ac);
}
static
pprMark_gen(pArea, x, y, markNum, drawFn)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate */
double	y;		/* I y data coordinate */
int	markNum;	/* I mark number--0 to PPR_NMARKS-1, inclusive */
void	(*drawFn)();	/* I function to draw lines, using pixel coordinates */
{
    short	*pMark;
    int		xp0, xp1, yp0, yp1;
    int		pen;

    pprLineThick(pArea, pArea->attr.lineThick);
    if (markNum >= 0) {
	markNum %= PPR_NMARKS;
	pMark = pglPprMarkS[markNum];	/* use small marks */
    }
    else if (markNum == -1)
	pMark = glPprMarkS_hCap;
    else if (markNum == -2)
	pMark = glPprMarkS_vCap;
    xp0 = pArea->xPixLeft + nint((x - pArea->xLeft) * pArea->xScale);
    yp0 = pArea->yPixBot + nint((y - pArea->yBot) * pArea->yScale);
    if (pArea->attr.clip) {
	if (xp0 < PprMin(pArea->xPixLeft, pArea->xPixRight) ||
	    xp0 > PprMax(pArea->xPixLeft, pArea->xPixRight) ||
	    yp0 < PprMin(pArea->yPixBot, pArea->yPixTop) ||
	    yp0 > PprMax(pArea->yPixBot, pArea->yPixTop)) {
	    return;
	}
    }
    if (pArea->pWin->winType == PPR_WIN_SCREEN)
	yp0 = pArea->pWin->height - yp0;
    while (1) {
	xp1 = xp0 + *pMark++;
	if (pArea->pWin->winType == PPR_WIN_SCREEN)	yp1 = yp0 - *pMark++;
	else						yp1 = yp0 + *pMark++;
	pen = *pMark++;
	if (pen) {
	    drawFn(pArea, (double)xp0, (double)yp0, (double)xp1, (double)yp1);
	    if (pen == 2)
		break;
	}
	xp0 = xp1;
	yp0 = yp1;
    }
}
void
pprMarkL(pArea, x, y, markNum)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	x;		/* I x data coordinate */
long	y;		/* I y data coordinate */
int	markNum;	/* I mark number--0 to PPR_NMARKS-1, inclusive */
{
    pprMark_gen(pArea, (double)x, (double)y, markNum, pprLineSegPixD_ac);
}
void
pprMarkEraseD(pArea, x, y, markNum)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate */
double	y;		/* I y data coordinate */
int	markNum;	/* I mark number--0 to PPR_NMARKS-1, inclusive */
{
    pprMark_gen(pArea, (double)x, (double)y, markNum, pprLineSegPixEraseD);
}
void
pprMarkEraseL(pArea, x, y, markNum)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	x;		/* I x data coordinate */
long	y;		/* I y data coordinate */
int	markNum;	/* I mark number--0 to PPR_NMARKS-1, inclusive */
{
    pprMark_gen(pArea, (double)x, (double)y, markNum, pprLineSegPixEraseD);
}

/*+/subr**********************************************************************
* NAME	pprMoveD - move the pen, possibly drawing a line
*
* DESCRIPTION
*	Move the "pen" to the specified point.  If the "pen" is "down"
*	a line will be drawn.  The line attributes of the plot area are
*	used in drawing the line.  If the attributes indicate a dashed
*	line, the current dashed line pattern will be used.
*
*	The "clipping" attribute for the plot area is honored only by
*	pprMoveD; pprMoveD_ac and pprMoveD_wc never clip lines at the
*	data area edges.
*
*	Two alternate entry points are available for drawing plain lines,
*	ignoring all attributes except color.  One uses the color for the
*	plot area (pprMoveD_ac); the other uses the plot window color
*	(pprMoveD_wc).  (As noted above, these routines don't clip.)
*
*		pprMoveD_ac(pArea, x, y, pen)	x and y are double
*		pprMoveD_wc(pArea, x, y, pen)	x and y are double
*
*	An additional entry point is available for erasing:
*
*		pprMoveEraseD(pArea, x, y)	x and y are double
*
* RETURNS
*	void
*
* BUGS
* o	only linear calibration is handled
*
* SEE ALSO
*	pprLine, pprLineSeg, pprText
*
*-*/
void
pprMoveD(pArea, x, y, pen)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate of new point */
double	y;		/* I y data coordinate of new point */
int	pen;		/* I pen indicator--non-zero draws a line */
{
    pprMoveD_gen(pArea, x, y, pen, pprLineSegPixD_ac);
}


pprMoveD_gen(pArea, x, y, pen, drawFn)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate of new point */
double	y;		/* I y data coordinate of new point */
int	pen;		/* I pen indicator--non-zero draws a line */
void	(*drawFn)();	/* I pointer to function to use in drawing */
{
    int		xp0, xp1, yp0, yp1;
    double	segLen, endLen, dashLen, xbeg, xend, ybeg, yend;

    xp0 = pArea->xPix[0];
    yp0 = pArea->yPix[0];
    xp1 = pArea->xPixLeft + nint((x - pArea->xLeft)* pArea->xScale);
    pArea->xPix[1] = xp1;
    yp1 = pArea->yPixBot + nint((y - pArea->yBot) * pArea->yScale);
    pArea->yPix[1] = yp1;
    if (pArea->pWin->winType == PPR_WIN_SCREEN)
	yp1 = pArea->yPix[1] = pArea->pWin->height - pArea->yPix[1];
    pArea->xPix[0] = xp1;
    pArea->yPix[0] = yp1;
    if (pen) {
	pprLineThick(pArea, pArea->attr.lineThick);
	if (pArea->attr.clip) {
	    int xpl=pArea->xPixLeft, xpr=pArea->xPixRight;
	    int ypb=pArea->yPixBot, ypt=pArea->yPixTop;
	    int ypeb, ypet;	/* "logical" top and bottom pix values */
	    int xpmin=PprMin(xp0,xp1), xpmax=PprMax(xp0,xp1);
	    int ypmin=PprMin(yp0,yp1), ypmax=PprMax(yp0,yp1);
	    if (pArea->pWin->winType == PPR_WIN_SCREEN) {
		ypb = pArea->pWin->height - ypb;
		ypt = pArea->pWin->height - ypt;
	    }
	    ypeb = PprMin(ypb, ypt);
	    ypet = PprMax(ypb, ypt);

	    if (xpmin > xpr || xpmax < xpl || ypmin > ypet || ypmax < ypeb)
		return;		/* no possible intersection with data area */
	    if (xpmin < xpl || xpmax > xpr || ypmin < ypeb || ypmax > ypet) {
		/* part of path is outside data area; find intersections */
		if (xp0 == xp1) {	/* no intersection with sides */
		    if (yp0 < ypeb)		yp0 = ypeb;
		    else if (yp0 > ypet)	yp0 = ypet;
		    if (yp1 < ypeb)		yp1 = ypeb;
		    else if (yp1 > ypet)	yp1 = ypet;
		}
		else if (yp0 == yp1) {	/* no intersection with top or bot */
		    if (xp0 < xpl)		xp0 = xpl;
		    else if (xp0 > xpr)		xp0 = xpr;
		    if (xp1 < xpl)		xp1 = xpl;
		    else if (xp1 > xpr)		xp1 = xpr;
		}
		else {
		    double S;		/* slope of line to draw */
		    int XP0=xp0, XP1=xp1, YP0=yp0, YP1=yp1;
		    S = (double)(yp1 - yp0) / (double)(xp1 - xp0);
		    if (XP0 < xpl)
			XP0 = xpl, YP0 = yp0 + nint((double)(xpl-xp0) * S);
		    else if (XP0 > xpr)
			XP0 = xpr, YP0 = yp0 + nint((double)(xpr-xp0) * S);
		    if (YP0 < ypeb)
			YP0 = ypeb, XP0 = xp0 + nint((double)(ypeb-yp0)/S);
		    else if (YP0 > ypet)
			YP0 = ypet, XP0 = xp0 + nint((double)(ypet-yp0)/S);
		    if (XP0 < xpl || XP0 > xpr)
			return;		/* no intersection */
		    if (XP1 < xpl)
			XP1 = xpl, YP1 = yp0 + nint((double)(xpl-xp0) * S);
		    else if (XP1 > xpr)
			XP1 = xpr, YP1 = yp0 + nint((double)(xpr-xp0) * S);
		    if (YP1 < ypeb)
			YP1 = ypeb, XP1 = xp0 + nint((double)(ypeb-yp0)/S);
		    else if (YP1 > ypet)
			YP1 = ypet, XP1 = xp0 + nint((double)(ypet-yp0)/S);
		    if (XP1 < xpl || XP1 > xpr)
			return;		/* no intersection */
		    xp0 = XP0, xp1 = XP1, yp0 = YP0, yp1 = YP1;
		}
	    }
	}
	if (pArea->attr.pPatt == NULL)
	    drawFn(pArea, (double)xp0, (double)yp0, (double)xp1, (double)yp1);
	else {
/*-----------------------------------------------------------------------------
* draw a dashed line pattern
*----------------------------------------------------------------------------*/
	    segLen = sqrt((double)((xp1-xp0)*(xp1-xp0) + (yp1-yp0)*(yp1-yp0)));
	    endLen = 0.;
	    xbeg = xp0;
	    ybeg = yp0;
	    while (endLen < segLen) {
		if (pArea->attr.rem < 1.) {
		    if ((pArea->attr.rem =
			    pArea->attr.pPatt[++pArea->attr.sub]) <= 0.) {
			pArea->attr.sub = 0;
			pArea->attr.rem = pArea->attr.pPatt[0];
			pArea->attr.pen = 1;
		    }
		    else
			pArea->attr.pen ^= 1;
		}
		dashLen = PprMin(pArea->attr.rem, segLen-endLen);
		endLen += dashLen;
		if (PprAbs(endLen-segLen) < .5)
		    endLen = segLen;
		xend = xp0 + endLen/segLen * (xp1 - xp0);
		yend = yp0 + endLen/segLen * (yp1 - yp0);
		if (pArea->attr.pen)
		    drawFn(pArea, xbeg, ybeg, xend, yend);
		xbeg = xend;
		ybeg = yend;
		pArea->attr.rem -= dashLen;
	    }
	}
    }
}
void
pprMoveD_ac(pArea, x, y, pen)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate of new point */
double	y;		/* I y data coordinate of new point */
int	pen;		/* I pen indicator--1 draws a line */
{
    pArea->xPix[1] = pArea->xPixLeft + nint((x - pArea->xLeft) * pArea->xScale);
    pArea->yPix[1] = pArea->yPixBot + nint((y - pArea->yBot) * pArea->yScale);
    if (pArea->pWin->winType == PPR_WIN_SCREEN)
	pArea->yPix[1] = pArea->pWin->height - pArea->yPix[1];
    if (pen) {
	pprLineThick(pArea, pArea->pWin->attr.lineThick);
	pprLineSegPixD_ac(pArea, (double)pArea->xPix[0], (double)pArea->yPix[0],
			(double)pArea->xPix[1], (double)pArea->yPix[1]);
    }
    pArea->xPix[0] = pArea->xPix[1];
    pArea->yPix[0] = pArea->yPix[1];
}
void
pprMoveD_wc(pArea, x, y, pen)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate of new point */
double	y;		/* I y data coordinate of new point */
int	pen;		/* I pen indicator--1 draws a line */
{
    pArea->xPix[1] = pArea->xPixLeft + nint((x - pArea->xLeft) * pArea->xScale);
    pArea->yPix[1] = pArea->yPixBot + nint((y - pArea->yBot) * pArea->yScale);
    if (pArea->pWin->winType == PPR_WIN_SCREEN)
	pArea->yPix[1] = pArea->pWin->height - pArea->yPix[1];
    if (pen) {
	pprLineThick(pArea, pArea->pWin->attr.lineThick);
	pprLineSegPixD_wc(pArea, (double)pArea->xPix[0], (double)pArea->yPix[0],
			(double)pArea->xPix[1], (double)pArea->yPix[1]);
    }
    pArea->xPix[0] = pArea->xPix[1];
    pArea->yPix[0] = pArea->yPix[1];
}
void
pprMoveEraseD(pArea, x, y)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate of new point */
double	y;		/* I y data coordinate of new point */
{
    pprMoveD_gen(pArea, x, y, 1, pprLineSegPixEraseD);
}

/*+/subr**********************************************************************
* NAME	pprPerim - draw a perimeter
*
* DESCRIPTION
*	Draw a perimeter with tick marks.  The number of intervals
*	specified in the plot area structure is used for placing
*	the tick marks.
*
*	The color attributes for the plot window are used for drawing;
*	dashed line and line thickness are ignored.
*
* RETURNS
*	void
*
* BUGS
* o	only linear axes are handled
*
* SEE ALSO
*	pprPerimLabel, pprPerimErase, pprGrid, pprAnnotX, pprAnnotY, pprAreaOpen
*
*-*/
void
pprPerim(pArea)
PPR_AREA *pArea;	/* I pointer to plot area structure */
{
    double	tickHalf;
    int		i;
    double	x, y;

/*-----------------------------------------------------------------------------
*    draw the box
*----------------------------------------------------------------------------*/
    pprMoveD_wc(pArea, pArea->xLeft, pArea->yBot, 0);
    pprMoveD_wc(pArea, pArea->xRight, pArea->yBot, 1);
    pprMoveD_wc(pArea, pArea->xRight, pArea->yTop, 1);
    pprMoveD_wc(pArea, pArea->xLeft, pArea->yTop, 1);
    pprMoveD_wc(pArea, pArea->xLeft, pArea->yBot, 1);
/*-----------------------------------------------------------------------------
*    draw the x axis tick marks
*----------------------------------------------------------------------------*/
    tickHalf = pArea->tickHt / pArea->yScale;
    x = pArea->xLeft;
    y = pArea->yBot - 3. * tickHalf;
    for (i=1; i<pArea->xNint; i++) {
	x += pArea->xInterval;
	pprLineSegD_wc(pArea, x, pArea->yBot - tickHalf, x, pArea->yBot);
	pprLineSegD_wc(pArea, x, pArea->yTop + tickHalf, x, pArea->yTop);
    }
/*-----------------------------------------------------------------------------
*    ditto for y axis
*----------------------------------------------------------------------------*/
    tickHalf = pArea->tickHt / pArea->xScale;
    x = pArea->xLeft - 1.5 * tickHalf;
    y = pArea->yBot;
    for (i=1; i<pArea->yNint; i++) {
	y += pArea->yInterval;
	pprLineSegD_wc(pArea, pArea->xLeft - tickHalf, y, pArea->xLeft, y);
	pprLineSegD_wc(pArea, pArea->xRight + tickHalf, y, pArea->xRight, y);
    }
}

/*+/subr**********************************************************************
* NAME	pprPerimErase - erase within a perimeter
*
* DESCRIPTION
*	Erases the screen inside the perimeter for the plot area.  (Actually,
*	the perimeter and tick marks are erased as well, since plot marks
*	may have been drawn on (and thus lie partly outside of) the perimeter
*	itself.  The perimeter and tick marks are then redrawn.)
*
* RETURNS
*	void
*
* SEE ALSO
*	pprGridErase, pprAreaErase, pprWinErase, pprRegionErase
*	the ppr...Erase... entry points for the various drawing routines
*
*-*/
void
pprPerimErase(pArea)
PPR_AREA *pArea;	/* I pointer to plot area structure */
{
    int		x,y,width,height;

    if (pArea->pWin->winType != PPR_WIN_SCREEN)
	return;
    x = pArea->xPixLeft - 3;
    y = pArea->yPixBot - 3;
    width = (pArea->xRight - pArea->xLeft) * pArea->xScale + 6;
    height = (pArea->yTop - pArea->yBot) * pArea->yScale + 6;
#ifdef SUNVIEW
    y = pArea->pWin->height - y - height;
    pw_writebackground(pArea->pWin->pw, x, y, width, height, PIX_SRC);
    pprPerim(pArea);
#elif defined XWINDOWS
    y = pArea->pWin->height - y - height;
    XClearArea(pArea->pWin->pDisp, pArea->pWin->plotWindow,
						x, y, width, height, False);
    pprPerim(pArea);
    XFlush(pArea->pWin->pDisp);
#endif
}

/*+/subr**********************************************************************
* NAME	pprPerimLabel - draw and label a perimeter
*
* DESCRIPTION
*	Draw a perimeter with tick marks.  The number of intervals
*	specified in the plot area structure is used for placing
*	the tick marks.
*
*	Axis labels and annotations are drawn using the information from
*	the plot area, as specified in the pprAreaOpen call.
*
* RETURNS
*	void
*
* BUGS
* o	only linear axes are handled
*
* SEE ALSO
*	pprPerim, pprGrid, pprAnnotX, pprAnnotY, pprAreaOpen
*
*-*/
void
pprPerimLabel(pArea, xLabel, xAnnot, yLabel, yAnnot, angle)
PPR_AREA *pArea;	/* I pointer to plot area structure */
char	*xLabel;	/* I label for x axis, or NULL */
char	**xAnnot;	/* I pointer to array of x annotations, or NULL */
char	*yLabel;	/* I label for y axis, or NULL */
char	**yAnnot;	/* I pointer to array of y annotations, or NULL */
double	angle;		/* I angle for y annotations; 0. or 90. */
{
    pprPerim(pArea);
    pprAnnotX(pArea, 0, pArea->xLeft, pArea->xRight, pArea->xNint,
						0, xLabel, xAnnot, 0.);
    pprAnnotY(pArea, 0, pArea->yBot, pArea->yTop, pArea->yNint,
						0, yLabel, yAnnot, angle);
}

/*+/macro*********************************************************************
* NAME	PprPixXToXFrac - convert pixel address to window fraction
*
* DESCRIPTION
*	Converts an x or y pixel address (for example, from a mouse click)
*	into a fraction of the plot window's width or height.
*
*	PprPixXToXFrac(pWin, xPixel)
*	PprPixYToYFrac(pWin, yPixel)
*
* RETURNS
*	window fraction
*
*-*/

/*+/subr**********************************************************************
* NAME	pprPoint - plot a point at a coordinate
*
* DESCRIPTION
*	Plot a pixel sized point using the line thickness and color
*	attributes of the plot area.
*
*	Two entry points are available:
*
*		pprPointD(pArea, x, y)	x and y are double
*		pprPointL(pArea, x, y)	x and y are long
*
*	Two entry points are available for erasing:
*
*		pprPointEraseD(pArea, x, y)	x and y are double
*		pprPointEraseL(pArea, x, y)	x and y are long
*
* RETURNS
*	void
*
* BUGS
* o	only linear calibration is handled
*
* SEE ALSO
*	pprMark, pprAreaOpen, pprAreaSetAttr, pprLine,
*	pprMove, pprText
*
*-*/
void
pprPointD(pArea, x, y)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate */
double	y;		/* I y data coordinate */
{
    double	xPix, yPix;
    double	xp0,yp0,xp1;

    xPix = pArea->xPixLeft + nint((x - pArea->xLeft) * pArea->xScale);
    yPix = pArea->yPixBot + nint((y - pArea->yBot) * pArea->yScale);
    pprLineThick(pArea, pArea->attr.lineThick);
    if (pArea->pWin->winType == PPR_WIN_SCREEN) {
	yPix = pArea->pWin->height - yPix;
#ifdef SUNVIEW
        if (pArea->pWin->attr.ltPix > 1) {
	    yp0 = yPix;
	    xp0 = xPix - pArea->pWin->attr.ltPix / 2;
	    xp1 = xp0 + pArea->pWin->attr.ltPix - 1;
#elif defined XWINDOWS
        if (pArea->attr.ltPix > 1) {
	    yp0 = yPix;
	    xp0 = xPix - pArea->attr.ltPix / 2;
	    xp1 = xp0 + pArea->attr.ltPix - 1;
#endif
	    pprLineSegPixD_ac(pArea, xp0, yp0, xp1, yp0);
	}
	else {
#ifdef SUNVIEW
	    pw_put(pArea->pWin->pw, (int)xPix, (int)yPix, 1);
#elif defined XWINDOWS
	    XDrawPoint(pArea->pWin->pDisp, pArea->pWin->plotWindow,
				pArea->attr.gc, (int)xPix, (int)yPix);
#endif
	}
    }
    else if (pArea->pWin->winType == PPR_WIN_POSTSCRIPT ||
					pArea->pWin->winType == PPR_WIN_EPS) {
	(void)fprintf(pArea->pWin->file, "%.1f %.1f DP\n", xPix, yPix);
    }
}
void
pprPointL(pArea, x, y)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	x;		/* I first x point */
long	y;		/* I first y point */
{
    pprPointD(pArea, (double)x, (double)y);
}
void
pprPointEraseD(pArea, x, y)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate */
double	y;		/* I y data coordinate */
{
    double	xPix, yPix;
    double	xp0,yp0,xp1;

    if (pArea->pWin->winType != PPR_WIN_SCREEN)
	return;
    xPix = pArea->xPixLeft + nint((x - pArea->xLeft) * pArea->xScale);
    yPix = pArea->yPixBot + nint((y - pArea->yBot) * pArea->yScale);
    pprLineThick(pArea, pArea->attr.lineThick);
    yPix = pArea->pWin->height - yPix;
#ifdef SUNVIEW
    if (pArea->pWin->attr.ltPix > 1) {
	yp0 = yPix;
	xp0 = xPix - pArea->pWin->attr.ltPix / 2;
	xp1 = xp0 + pArea->pWin->attr.ltPix - 1;
#elif defined XWINDOWS
    if (pArea->attr.ltPix > 1) {
	yp0 = yPix;
	xp0 = xPix - pArea->attr.ltPix / 2;
	xp1 = xp0 + pArea->attr.ltPix - 1;
#endif
	pprLineSegPixEraseD(pArea, xp0, yp0, xp1, yp0);
    }
    else {
#ifdef SUNVIEW
	pw_put(pArea->pWin->pw, (int)xPix, (int)yPix, 0);
#elif defined XWINDOWS
	XDrawPoint(pArea->pWin->pDisp, pArea->pWin->plotWindow,
				pArea->pWin->attr.gcBG, (int)xPix, (int)yPix);
#endif
    }
}
void
pprPointEraseL(pArea, x, y)
PPR_AREA *pArea;	/* I pointer to plot area structure */
long	x;		/* I first x point */
long	y;		/* I first y point */
{
    pprPointEraseD(pArea, (double)x, (double)y);
}

/*/subhead pprPSProg-------------------------------------------------------
* PostScript routines for handling lines, points, text strings, etc.
*----------------------------------------------------------------------------*/

static char *pprPSProg[]={
/*-----------------------------------------------------------------------------
* DP - draw a point
*	x y DP -
*----------------------------------------------------------------------------*/
"/DP {",
"	newpath",
"	moveto",
"	currentlinewidth dup 2 div neg 0 rlineto 0 rlineto",
"	stroke",
"} def",
/*-----------------------------------------------------------------------------
* DS - draw a line segment from x1,y1 to x2,y2
*	x1 y1 x2 y2 DS -
*----------------------------------------------------------------------------*/
"/DS {",
"	newpath",
"	4 2 roll moveto lineto",
"	stroke",
"} def",
/*-----------------------------------------------------------------------------
* FSet - set size of current font; if already proper size, no action
* 	size FSet -
*
* BUGS
* o	should keep a list of scalefont which have already been done, to
*	avoid the need to do a new one
* o	doesn't allow selecting font family
*----------------------------------------------------------------------------*/
"/F /Helvetica findfont def",
"/F_HT -1 def",
"/FSet {",
"	/F_ht exch def",
"	F_ht F_HT ne {",
"		/F_HT F_ht def",
"		F F_HT scalefont dup /FF exch def setfont",
"	} {",
"		FF setfont",	/* if not changing, still must set */
"	} ifelse",
"} def",

/*-----------------------------------------------------------------------------
* PT - plot a text string
*	Text appears at current location with specified size and angle.
*	The text can be right, left, or center justified; the "half height"
*	of the characters will be at the current location.
*
* string just angle size PT -
*				"just" is one of (PTC), (PTR), or (PTL)
*----------------------------------------------------------------------------*/

"/PTL { } def",
"/PTC { dup stringwidth pop -2 div 0 rmoveto } def",
"/PTR { dup stringwidth pop neg 0 rmoveto } def",

"/PT {	FSet gsave",
"	currentpoint translate rotate",
"	cvn cvx exec",			/* execute the PTx procedure */
"	0 F_HT -3 div rmoveto",		/* offset to vertical center of char */
"	show",
"	grestore } def"
};

/*+/subr**********************************************************************
* NAME	pprRegionErase - erase an area within a plot window
*
* DESCRIPTION
*	Erases an area within a plot window.
*
* RETURNS
*	void
*
* SEE ALSO
*	pprWinErase, pprAreaErase, pprGridErase, pprPerimErase
*	the ppr...Erase... entry points for the various drawing routines
*
* NOTES
* 1.	Another mode of calling pprRegionErase, in which the arguments are
*	pixel offsets from the data area boundaries, is invoked by
*	having one or more of the arguments be greater than 1. or less
*	than 0.  In this case, all the "wfrac" arguments are treated as
*	pixel offsets.
*
* EXAMPLES
* 1.	Erase within a data area, preserving the perimeter line.  This
*	example uses pixel offsets from the plot area boundary--one pixel
*	inside each edge of the plot area.
*
*		pprRegionErase(pArea, 1., 1., -1., -1.);
*
* 2.	A data area occupies the upper right quarter of the plot window.
*	An annotation area (using default character height) extends from
*	the left edge of the plot window to the left edge of the data area,
*	centered at the vertical midpoint of the data area.  Erase the
*	annotation area.
*
*	double	yb,yt;
*
*	The region to erase is expressed as fractions of window width and
*	height.  For x, the region is from 0. (the left edge of the plot
*	window) to .49 (just a bit left of the center of the plot window).
*	For y, the region is centered at .75 (the vertical midpoint of the
*	plot area within the plot window), with a height of the default
*	character height.
*
*	yb = .75 - PprDfltCharHt(.5,1.);
*	yt = yb + PprDfltCharHt(.5,1.);
*	pprRegionErase(pArea, 0., yb, .49, yt);
*
*-*/
void
pprRegionErase(pArea, wfracXleft, wfracYbot, wfracXright, wfracYtop)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	wfracXleft;	/* I x win frac of left edge of area (see Note 1) */
double	wfracYbot;	/* I y win frac of bottom edge of area (see Note 1) */
double	wfracXright;	/* I x win frac of right edge of area (see Note 1) */
double	wfracYtop;	/* I y win frac of top edge of area (see Note 1) */
{
    int		x,y,width,height;

    if (pArea->pWin->winType != PPR_WIN_SCREEN)
	return;
    if (wfracXleft<0. || wfracXleft>1. || wfracXright<0. || wfracXright>1. ||
	wfracYbot<0. || wfracYbot>1. || wfracYtop<0. || wfracYtop>1.) {
	x = pArea->xPixLeft + wfracXleft;
	y = pArea->yPixBot + wfracYbot;
	width = pArea->xPixRight + wfracXright - x + 1;
	height = pArea->yPixTop + wfracYtop - y + 1;
    }
    else {
	x = wfracXleft * pArea->pWin->width;
	y = wfracYbot * pArea->pWin->height;
	width = (wfracXright - wfracXleft) * pArea->pWin->width;
	height = (wfracYtop - wfracYbot) * pArea->pWin->height;
    }
#ifdef SUNVIEW
    y = pArea->pWin->height - y - height;
    pw_writebackground(pArea->pWin->pw, x, y, width, height, PIX_SRC);
#elif defined XWINDOWS
    y = pArea->pWin->height - y - height;
    XClearArea(pArea->pWin->pDisp, pArea->pWin->plotWindow,
						x, y, width, height, False);
#endif
}

/*+/subr**********************************************************************
* NAME	pprSin_deg - get the sine of an angle in degrees
*
* DESCRIPTION
*	Get the sine of an angle in degrees.  A table lookup technique
*	is used.  The angle argument is truncated to the next lower 1/4
*	degree prior to lookup.
*
* RETURNS
*	sine
*
*-*/
double
pprSin_deg(angle)
double	angle;		/* I angle, in degrees */
{
    return pprCos_deg(angle - 90.);
}

/*/subhead character_tables----------------------------------------------------
* ITABLE = the array defining the character set. Each character is
*          defined by one 34 character string "matrix". The "matrix"
*          elements consist of a series of up to 17 (IDX,IDY)'s which
*          define the next stroke. The vector is positioned at (IX1,IY1)
*          and drawn to (IX2,IY2) where:
*                IX1 = IX + IDX1     IY1 = IY + IDY1
*                IX2 = IX + IDX2     IY2 = IY + IDY2
*          (IX2,IY2) becomes the next (IX1,IY1) unless the new IDX = 0
*          in which case a new vector is started with the next 2
*          elements after the 0 becoming (IDX,IDY) etc. (ie., an IDX
*          = 0 causes a break in the character strokes.) An IDY = 0
*          causes an offset to be added to IY for the remaining strokes
*          in the character. This is used for characters such as g and
*          y which extend below the relative 0 line. The element
*          following an IDY = 0 becomes the IDY actually used. The
*         (IDX,IDY) represents a position in a 6 x 9 matrix. IDX has
*          a range of 0 - 6, where most characters fall within 1 - 5;
*          0 triggers a new stroke unless it is the 1st element of the
*          string in which case it is really 0. IDY has a range of
*          0 - 9, where most characters fall within 1 - 7; 0 triggers
*          an offset to lower the rest of the strokes. A non-numeric
*          character in the string, ie. a period, is used to terminate
*          the stroke matrix for the character.
*
*      converted to C by Roger Cole, 12-90
*
* EFC 4/16/85: included call to UNPKST to make character string
*              matrices nearly as efficient as octal matrices.
*              Uses 95 microseconds more per character on average.
* EFC 4/8/85: rewritten for STRING a character variable rather than
*             hollerith. Extended and rewrote character stroke array
*             for nicer characters. Changed definition of INSIZE = 0
*             matrix from 10 x 13 to 9 x 13.
* EFC 9/21/84: rewritten for FORTRAN 77 on DEC KL-10 to use character
*              functions rather than binary masks and shifts. Changed
*              matrix for tau and xsi. Changed coding to fix bug in
*              "y" shifting of charcters and eliminate inconsistencies
*              in described usage and actual usage.
*      modified by Debby Hyman, 9-80
*      Greek characters almost match QUME printer char set, now
*
*changed 16-may-76 STB CTR
*----------------------------------------------------------------------------*/
static char	*glPprFont0[]={
/*-----------------------------------------------------------------------------
* blank  !  "  #  $  %  &  '  (  ) * +  ,  -  . /
*----------------------------------------------------------------------------*/
     ".", "2737242702221313222.", "27253727057454757.", "2721047410155501353.",
     "5626152444521203137.", "57110162736251604132435241.",
     "511516273746451312213153.", "35473735.", "41232547.", "21434527.",
     "46220422605414.", "323605414.", "31433331.", "5414.", "3242413132.",
     "5711.",
/*-----------------------------------------------------------------------------
* 0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
*----------------------------------------------------------------------------*/
     "132141535547271513.", "51110313726.", "51111256472716.",
     "44240122141524456472716.", "46410531337.", "57171425455452412112.",
     "564727161221415253442413.", "315717.",
     "44555647271615244453524121121324.", "5424151627475652412112.",
     "454636354504342323343.", "3646453536031334331.", "521456.",
     "155501353.", "125416.", "16274756553303231212232.",
/*-----------------------------------------------------------------------------
* @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
*----------------------------------------------------------------------------*/
     "512112162747565443344554.", "23430513711.", "4453524111174756554414.",
     "5647271612214152.", "17114153554717.", "1444051111757.", "44140111757.",
     "56472716122141520515434.", "51570541401117.", "51110313705717.",
     "122131424705727.", "51240571301117.", "511117.", "5157331711.",
     "57511711.", "271612214152564727.",
/*-----------------------------------------------------------------------------
* P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
*----------------------------------------------------------------------------*/
     "14445556471711.", "27161221415256472703361.", "5134014445556471711.",
     "564727161524445352412112.", "313705717.", "575241211217.", "573117.",
     "5741352117.", "571105117.", "57340313417.", "51115717.", "41212747.",
     "1751.", "21414727.", "273847.", "10252.",
/*-----------------------------------------------------------------------------
* `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o
*----------------------------------------------------------------------------*/
     "37454737.", "14254554510524121122353.", "1545535241211201117.",
     "512112142555.", "5525141221415205157.", "512112142545545313.",
     "56473726210113103414.", "52412112142545540507524121.", "111701425455451.",
     "214103135250373737.", "373737030747423121.", "45134101117.",
     "41210313727.", "1115014253531034455551.", "111501425455451.",
     "122141525445251412.",
/*-----------------------------------------------------------------------------
* p  q  r  s  t  u  v  w  x  y  z  { | } ~
*----------------------------------------------------------------------------*/
     "1425455452412112010711.", "544525141221415205075161.",
     "1115014254554.", "54452514234352412112.", "2545037324151.",
     "151221415205155.", "553115.", "5541352115.", "551105115.",
     "15122141520507524121.", "51115515.", "47372622314101424.",
     "3137.", "27374642312104454.", "16274657.",
/*-----------------------------------------------------------------------------
*     box   dot
*----------------------------------------------------------------------------*/
     "1252561612.", "3254361432."
};
#if 0
static char	*glPprFont1[]={
/*-----------------------------------------------------------------------------
* blank  !  "  #  $  %  &  '  (  ) * +  ,  -  . /
*----------------------------------------------------------------------------*/
     ".", "115103137.", "212704147.", "15550571101353.", "52511135195958.",
     "14264254462214.", "52422614224656.", "135351.", "492523401.",
     "294543201.", "125601652.", "52120333705515.", "24324403236.",
     "16560353101353.", "3343443433.", "2559.",
/*-----------------------------------------------------------------------------
* 0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
*----------------------------------------------------------------------------*/
     "253546483929181625.", "45150353928.", "45151648392918.",
     "37270162535463748392918.", "38350461629.", "491917283847463515.",
     "4839291816253546372716.", "194925.", "374839291827374635251627.",
     "153546483929182747.", "172728181703747483837.",
     "45463635450434232334302454.", "13510531557.", "1353015264556.",
     "53110135517.", "12213137475601555.",
/*-----------------------------------------------------------------------------
* @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
*----------------------------------------------------------------------------*/
     "12213142332536273848570364433.", "1136510234303526374635.",
     "17315705414.", "31370575544241517.", "11513711.", "12520541401656.",
     "224253554626151322021410313702747.", "1131021270175756.",
     "12214152564727161202444.", "122131475766.", "162747545241211213244453.",
     "11212212110415152424103545463635.", "0121011375104161.",
     "512112142555.", "114152544515.", "11415254451502353.",
/*-----------------------------------------------------------------------------
* P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
*----------------------------------------------------------------------------*/
     "112127017570474151.", "132242535546261513.", "132147.", "52511144175756.",
     "21270414705727161524.", "21410313627160364756.", "31571731.",
     "0211212314162747565443415162.", "12115152043450442402325016175756.",
     "111526465551.", "161221415256.", "492920141.", "33543505414.",
     "294940121.", "24364403632.", "33143501454.",
/*-----------------------------------------------------------------------------
* `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o
*----------------------------------------------------------------------------*/
     "1353031643705515.", "5141443525141221314255.",
     "2141525344140445556472716101.", "1626243343545604721.",
     "56473726255352412112132434.", "52412112234302314254554.",
     "452514122141525445047201.", "152636454131224556.",
     "5414132141535547271514.", "25353141.", "2145473726254151.",
     "1511051134555.", "11340512717.", "514204542312112015101.", "25315355.",
     "171524445557482817.",
/*-----------------------------------------------------------------------------
* p  q  r  s  t  u  v  w  x  y  z  { | } ~
*----------------------------------------------------------------------------*/
     "252104144055442514.", "1626415101156.", "2231415254453524101.",
     "552514132232434435.", "4131350552514.", "15132141525445.",
     "145404354450251423.", "25141221320333241525445.",
     "17260561615244402413125251402.", "152521024354554501.",
     "1736560361312215150241.", "142404939282023141.", "6859493012112.",
     "445402939484023121.", "041525445465.",
/*-----------------------------------------------------------------------------
*     box   dot   / diamond   triangle
*----------------------------------------------------------------------------*/
     "3434.", "13533713."
};
#endif

/*+/subr**********************************************************************
* NAME	pprText - plot a text string
*
* DESCRIPTION
*	Plots a text string at a location.  The character "half height" is
*	placed at the specified x,y position; the 'justification' option
*	selects whether the left end, center, or right end of the text is
*	placed at the x,y coordinate.
*
*	The character height specification is in terms of a fraction of the
*	height of the window.  This results in automatic scaling of
*	character sizes as the window size changes.  If a height of zero
*	is specified in the call, then the default height for the plot
*	area is used.  (The default height was established by pprAreaOpen.)
*
*	A macro is available which returns the default character height
*	used by this plot package.  The value returned is proportional to
*	the height of the plot area.  This value can be used to generate
*	"big" and "small" character sizes with simple multiplication of
*	the default height by a "scale factor".
*
*		PprDfltCharHt(lowYfrac, highYfrac)
*
*		lowYfrac is the vertical fraction of the window at which
*			the bottom edge of the plot area lies
*		highYfrac is the vertical fraction of the window at which
*			the top edge of the plot area lies
*
*	It is also often useful to know what horizontal fraction of the
*	window width corresponds to a vertical fraction of the height.  The
*	following routine returns the horizontal fraction:
*
*		pprYFracToXFrac(pWin, yFrac)
*
*	An alternate entry point, pprText_wc, is available to use the
*	plot window's color for drawing the text.
*
*	An additional entry point is available for erasing (which requires
*	all arguments to be exactly the same as when the text was originally
*	plotted):
*
*		pprTextErase(pArea, x, y, text, just, height, angle)
*
* RETURNS
*	void
*
* BUGS
* o	technique used works only with linear axes
* o	ASCII character codes are assumed
* o	no checks are made for illegal characters
* o	positioning and sizing are somewhat different for the various
*	window types
*
* SEE ALSO
*	pprChar, pprAreaOpen, pprLine, pprPoint, pprCvtDblToTxt
*
*-*/
void
pprText(pArea, x, y, text, just, height, angle)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate of text */
double	y;		/* I y data coordinate of text */
char	*text;		/* I text to plot */
PPR_TXT_JUST just;	/* I text justification selector: one of
				PPR_TXT_CEN, PPR_TXT_RJ, or PPR_TXT_LJ */
double	height;		/* I height of text characters, as a fraction of
				the height of the window; a value of
				zero results in using a default height */
double	angle;		/* I orientation angle of text string, ccw degrees */
{
    pprText_gen(pArea, x, y, text, just, height, angle, pprLineSegPixD_ac);
}
void
pprText_wc(pArea, x, y, text, just, height, angle)
PPR_AREA *pArea;
double	x;
double	y;
char	*text;
PPR_TXT_JUST just;
double	height;
double	angle;
{
    pprText_gen(pArea, x, y, text, just, height, angle, pprLineSegPixD_wc);
}
static void
pprText_gen(pArea, x, y, text, just, height, angle, fn)
PPR_AREA *pArea;
double	x;
double	y;
char	*text;
PPR_TXT_JUST just;
double	height;
double	angle;
void	(*fn)();
{
    double	xWin, yWin;
    double	scale;		/* convert character units to win coord */
    double	cosT, sinT;

    if (height <= 0.)
        height = pArea->charHt;
    else
        height *= pArea->pWin->height;
    xWin = pArea->xPixLeft + nint((x - pArea->xLeft) * pArea->xScale);
    yWin = pArea->yPixBot + nint((y - pArea->yBot) * pArea->yScale);

    if (pArea->pWin->winType == PPR_WIN_SCREEN) {
	if (angle == 0.)	cosT = 1., sinT = 0.;
	else if (angle == 90.)	cosT = 0., sinT = 1.;
	else {			cosT = pprCos_deg(angle);
				sinT = pprSin_deg(angle);
	}

	scale = height / 6.;

	if (just == PPR_TXT_CEN) {
	    xWin -= .5 * (scale * 6. * (double)(strlen(text)-1) * cosT);
	    yWin -= .5 * (scale * 6. * (double)(strlen(text)-1) * sinT);
	}
	else if (just == PPR_TXT_RJ) {
	    xWin -= scale * 6. * (double)(strlen(text)-1) * cosT;
	    yWin -= scale * 6. * (double)(strlen(text)-1) * sinT;
	}

	while (*text != '\0') {
	    pprText1(pArea, xWin, yWin, *text, 0, scale, sinT, cosT, fn);
	    xWin += scale * 6. * cosT;
	    yWin += scale * 6. * sinT;
	    text++;
	}
    }
    else {
	height = 1.5 * height;
	pprTextPS(pArea->pWin->file, xWin, yWin, just, text, height, angle);
    }
}
void
pprTextErase(pArea, x, y, text, just, height, angle)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	x;		/* I x data coordinate of text */
double	y;		/* I y data coordinate of text */
char	*text;		/* I text to plot */
PPR_TXT_JUST just;	/* I text justification selector: one of
				PPR_TXT_CEN, PPR_TXT_RJ, or PPR_TXT_LJ */
double	height;		/* I height of text characters, as a fraction of
				the height of the window; a value of
				zero results in using a default height */
double	angle;		/* I orientation angle of text string, ccw degrees */
{
    pprText_gen(pArea, x, y, text, just, height, angle, pprLineSegPixEraseD);
}

/*+/internal******************************************************************
* NAME	pprText1 - plot a "drawn" character
*
* DESCRIPTION
*	Plots a character at x,y using specified scale factor and rotation.
*	All sizes and coordinates use window units, rather than data units.
*
* RETURNS
*	void
*
* BUGS
* o	only linear calibration is handled
*
*-*/
void
pprText1(pArea, xWin, yWin, ic, nfont, scale, sinT, cosT, fn)
PPR_AREA *pArea;	/* I pointer to plot area structure */
double	xWin;		/* IO x position, in window coordinates */
double	yWin;		/* IO y position, in window coordinates */
int	ic;		/* I character code to plot */
int	nfont;		/* I font selector--0 or 1 */
double	scale;		/* I scale factor to convert a character height of
				6 to the desired height in window coord */
double	sinT;		/* I sine of orientation angle */
double	cosT;		/* I cosine of orientation angle */
void	(*fn)();	/* I pointer to drawing fn: pprLineSegPixD_.. */
{
/* stolen (who knows how many times removed) from the Los Alamos National
   Laboratory Plasma Physics Plotting Package */
    char	*font;		/* font string for this character */
    int		nStrokes;
    int		mcx, mcy, indw, drawit;
    double	rx, ry;
    int		ibyt;
    double	xp0,xp1,yp0,yp1;

    if (ic < ' ' || ic > '~')
	ic = '#';
    if (nfont == 0)
	font = glPprFont0[ic-' '];
#if  0
    else
	font = glPprFont1[ic-' '];
#endif
    nStrokes = (int)(index(font, '.') - font) -1;
    mcx = 3;
    mcy = 4;
    indw = 0;

    pprLineThick(pArea, pArea->pWin->attr.lineThick);
    drawit = 0;
    xp0 = pArea->xPix[0];
    yp0 = pArea->yPix[0];
    while (indw < nStrokes) {
	ibyt = font[indw] - '0';
	if (ibyt == 0 && indw != 0)
	    drawit = 0;		/* there is a break in the strokes */
	else {
	    rx = (float)(ibyt - mcx);
	    while ((ibyt = font[++indw] - '0') == 0)
		mcy = 6;	/* this char has a descender, as for y or g */
	    ry = (float)(ibyt - mcy);

	    xp1 = xWin + scale * (rx*cosT - ry*sinT);
	    yp1 = yWin + scale * (rx*sinT + ry*cosT);
	    if (pArea->pWin->winType == PPR_WIN_SCREEN)
		yp1 = pArea->pWin->height - yp1;
	    if (drawit)
		fn(pArea, xp0, yp0, xp1, yp1);
	    xp0 = xp1;
	    yp0 = yp1;

	    drawit = 1;
	}
	indw++;
    }
}

/*+/internal******************************************************************
* NAME	pprTextPS - send the PostScript commands to plot some text
*
* DESCRIPTION
*	This routine sets up the call to the routine (unique to this
*	package) in the PostScript printer.  This involves:
*	o   moving to the desired position
*	o   massaging the caller's text to enclose it in ( and ) and to
*	    "escape" characters which are special to PostScript
*	o   setting the font size
*	o   setting the angle for the text
*	o   setting for right, centered, or left justification
*
*	All text is printed with the font selected by the PostScript
*	routine.
*
* RETURNS
*	void
*
* BUGS
* o	assumes that all characters in the caller's text string are printable
*
*-*/
static void
pprTextPS(psFile, x, y, just, text, height, angle)
FILE	*psFile;	/* I pointer to PostScript file */
double	x,y;		/* I x and y position, in window coordinates */
PPR_TXT_JUST just;	/* I text justification selector: one of
				PPR_TXT_CEN, PPR_TXT_RJ, or PPR_TXT_LJ */
char	*text;		/* I the text to plot */
double	height;		/* I the desired height of the text, in points */
double	angle;		/* I orientation angle of the text, + is ccw */
{
    char	myJust[4];

/*-----------------------------------------------------------------------------
* send out the text string, enclosed in PostScript string delimiters of (  )
*	Special characters are sent out as an appropriate \ sequence
*----------------------------------------------------------------------------*/
    (void)fprintf(psFile, "%d %d moveto\n", (int)(x+.5), (int)(y+.5));
    fputc('(', psFile);
    while (*text != '\0') {
	switch (*text) {
	    case '\b':
		fputc('\\', psFile); fputc('b', psFile);
		break;
	    case '\f':
		fputc('\\', psFile); fputc('f', psFile);
		break;
	    case '\n':
		fputc('\\', psFile); fputc('n', psFile);
		break;
	    case '\r':
		fputc('\\', psFile); fputc('r', psFile);
		break;
	    case '\t':
		fputc('\\', psFile); fputc('t', psFile);
		break;
	    case '(':
	    case ')':
	    case '\\':
		fputc('\\', psFile);
	    default:
		fputc(*text, psFile);
	}
	text++;
    }
    fputc(')', psFile);
    fputc(' ', psFile);
/*-----------------------------------------------------------------------------
* now send out the rest of the PostScript information, so that the whole
*	thing from this routine will be something like the following, with
*	the proper choice of PTC PTL and PTR:
*
*	x y moveto (text) (PTx) angle typesize PT
*----------------------------------------------------------------------------*/
    if (just == PPR_TXT_CEN)
	(void)strcpy(myJust, "PTC");
    else if (just == PPR_TXT_RJ)
	(void)strcpy(myJust, "PTR");
    else
	(void)strcpy(myJust, "PTL");
    (void)fprintf(psFile, "(%s) %.2f %d PT\n", myJust, angle, (int)(height+.5));
}

/*/subhead pprWinAttr----------------------------------------------------------
*
*----------------------------------------------------------------------------*/
static void
pprWinAttr(pWin)
PPR_WIN	*pWin;
{
#ifdef SUNVIEW
    pWin->width = (int)window_get(pWin->canvas, WIN_WIDTH);
    pWin->height = (int)window_get(pWin->canvas, WIN_HEIGHT);
    pWin->x = (int)window_get(pWin->frame, WIN_X);
    pWin->y = (int)window_get(pWin->frame, WIN_Y);
#elif defined XWINDOWS
    XWindowAttributes	winAttr;
    XWindowAttributes	parentAttr;
    long	stat;
    Window	rootWindow, parentWindow, *pChildWindows;
    unsigned int nChildren;
    int		actualX, actualY;

    XGetWindowAttributes(pWin->pDisp, pWin->plotWindow, &winAttr);
    actualX = winAttr.x;
    actualY = winAttr.y;
    stat = XQueryTree(pWin->pDisp, pWin->plotWindow, &rootWindow,
				&parentWindow, &pChildWindows, &nChildren);
    PprAssert(stat != 0);
    if (pChildWindows != NULL)
	XFree(pChildWindows);
    if (rootWindow != parentWindow) {
	XGetWindowAttributes(pWin->pDisp, parentWindow, &parentAttr);
	actualX = parentAttr.x;
	actualY = parentAttr.y;
    }
    pWin->width = winAttr.width;
    pWin->height = winAttr.height;
    pWin->x = actualX;
    pWin->y = actualY;
#endif
}

/*+/subr**********************************************************************
* NAME	pprWinClose - close a plot window
*
* DESCRIPTION
*	Free the memory associated with a plot window structure and do other
*	cleanup activities.
*
*	This routine should be called when plotting is complete for a plot
*	window.  Any plot areas not previously closed are automatically
*	closed by this routine.
*
*	No further references to the plot window may be made.
*
* RETURNS
*	void
*
* SEE ALSO
*	pprAreaClose, pprWinInfo, pprWinOpen
*
*-*/
void
pprWinClose(pWin)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
{
    PPR_AREA	*pArea, *pAreaNext;
#ifdef XWINDOWS
    if (pWin->winType == PPR_WIN_SCREEN) {
	if (pWin->attr.myGC)
	    XFree(pWin->attr.gc);
	if (pWin->attr.bgGC)
	    XFree(pWin->attr.gcBG);
    }
#endif
    pArea = pWin->pAreaHead;
    while (pArea != NULL) {
	pAreaNext = pArea->pNext;
	pprAreaClose(pArea);
	pArea = pAreaNext;
    }
    free((char *)pWin);
}

/*+/subr**********************************************************************
* NAME	pprWinErase - erase a plot window
*
* DESCRIPTION
*	Erase the contents of the entire plot window.
*
* RETURNS
*	void
*
* SEE ALSO
*	pprGridErase, pprPerimErase, pprAreaErase, pprRegionErase
*	the ppr...Erase... entry points for the various drawing routines
*
*-*/
void
pprWinErase(pWin)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
{
    if (pWin->winType == PPR_WIN_SCREEN) {
#ifdef SUNVIEW
        pw_writebackground(pWin->pw, 0, 0, pWin->width, pWin->height, PIX_SRC);
#elif defined XWINDOWS
	XClearArea(pWin->pDisp, pWin->plotWindow, 0, 0,
			pWin->width, pWin->height, False);
        XFlush(pWin->pDisp);
#endif
    }
}

/*+/internal******************************************************************
* NAME	pprWinEvHandler - handle events in the plotting window
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	action needs to depend on type of window
* o	for SunView, a pw_lock() and pw_unlock() would make drawing more
*	efficient
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
#ifdef SUNVIEW
static void
pprWinEvHandler(window, pEvent, pArg)
Window	window;
Event	*pEvent;
void	*pArg;
{
    PPR_WIN	*pWin;

    pWin = (PPR_WIN *)window_get(window, WIN_CLIENT_DATA);
    if (pWin->winType != PPR_WIN_SCREEN)
	PprAssertAlways(0);
    if (event_action(pEvent) == WIN_REPAINT) {
	if (window == pWin->canvas && !window_get(window, FRAME_CLOSED)) {
	    pprWinWrapup(pWin);
	    pWin->attr.ltCurr = -1;
	    pWin->attr.lineThick = 1;
	    pprWinAttr(pWin);
	    (pWin->drawFun)(pWin, pWin->pDrawArg);
	}
    }
    else if (event_action(pEvent) == PPR_BTN_CLOSE) {
	if (event_is_up(pEvent)) {
	    pprWinAttr(pWin);
	    window_destroy(pWin->frame);
	    pWin->frame = NULL;
	    pWin->canvas = NULL;
	    pprWinWrapup(pWin);
	}
    }
}
#endif

#ifdef XWINDOWS
static void
pprWinEvHandler(pWin, pEvent)
PPR_WIN	*pWin;
XEvent	*pEvent;	/* pointer to a window event structure */
{

    if (pEvent->type == Expose && pEvent->xexpose.count == 0) {
#define PPR_DEBUG_EVENTS 0
#if PPR_DEBUG_EVENTS
	(void)printf("expose event\n");
#endif
	pprWinWrapup(pWin);
	pprWinAttr(pWin);
	(pWin->drawFun)(pWin, pWin->pDrawArg);
    }
    else if (pEvent->type == ButtonRelease) {
	pprWinAttr(pWin);
	if (pEvent->xbutton.x < 0  || pEvent->xbutton.x > pWin->width ||
	    pEvent->xbutton.y < 0  || pEvent->xbutton.y > pWin->height) {
#if PPR_DEBUG_EVENTS
	    (void)printf("button up but mouse not home\n");
	    (void)printf("button x,y=%d,%d win x,y width,ht=%d,%d %d,%d\n",
			pEvent->xbutton.x, pEvent->xbutton.y,
			pWin->x, pWin->y, pWin->width, pWin->height);
#endif
	    ;	/* no action */
	}
	else if (pEvent->xbutton.button == PPR_BTN_CLOSE) {
#if PPR_DEBUG_EVENTS
	    (void)printf("button3 event\n");
#endif
	    XCloseDisplay(pWin->pDisp);
	    pWin->pDisp = NULL;
	    pprWinWrapup(pWin);
	}
#if PPR_DEBUG_EVENTS
	else
	    (void)printf("other button event\n");
#endif
    }
#if PPR_DEBUG_EVENTS
    else
	(void)printf("some other event\n");
#endif
}
#endif

/*+/subr**********************************************************************
* NAME	pprWinInfo - get some information about the plot window
*
* DESCRIPTION
*	Get the size of the plot window and its position on the screen.
*
* RETURNS
*	void
*
* NOTES
* 1.	The information returned is window system dependent.  To avoid
*	portability problems, this information should be used only in
*	calls to pprWinXxx routines.
*
*-*/
void
pprWinInfo(pWin, pXpos, pYpos, pXwid, pYht)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
int	*pXpos; /* O pointer to place to store window x coord., in pixels */
int	*pYpos; /* O pointer to place to store window y coord., in pixels */
int	*pXwid;	/* O pointer to place to store window width, in pixels */
int	*pYht;	/* O pointer to place to store window height, in pixels */
{
    *pXpos = pWin->x;
    *pYpos = pWin->y;
    *pXwid = pWin->width;
    *pYht = pWin->height;
}

/*+/subr**********************************************************************
* NAME	pprWinIsMono - test to see if plot window is monochrome
*
* DESCRIPTION
*
* RETURNS
*	1	if plot window is monochrome or gray scale
*	0	if plot window is color
*
*-*/
int
pprWinIsMono(pWin)
PPR_WIN	*pWin;		/* I pointer to plot window structure */
{
#ifdef XWINDOWS
    int		screenNo;
    Visual	*pVisual;
#endif

#ifdef XWINDOWS
    if (pWin->winType != PPR_WIN_SCREEN)
	return 1;
    screenNo = DefaultScreen(pWin->pDisp);
    pVisual = DefaultVisual(pWin->pDisp, screenNo);
    if (pVisual->class != GrayScale && pVisual->class != StaticGray)
	return 0;
#endif
    return 1;		/* color not supported if not XWINDOWS */
}

/*+/subr**********************************************************************
* NAME	pprWinLoop - loop until "quit" event received in window
*
* DESCRIPTION
*	Handles the interactions with the windowing system.  The specific
*	actions depend on the plot window type:
*
*	PPR_WIN_SCREEN
*	o   creates a window on the screen
*	o   when the window actually appears, calls the caller's draw function
*	o   for all subsequent resize and expose events, calls the caller's
*	    draw function
*	o   when the right mouse button is clicked on the plot, closes the
*	    window and returns to the caller.  The current position and size
*	    of the window are stored (and can be retrieved with pprWinInfo).
*
*	PPR_WIN_POSTSCRIPT
*	o   calls the caller's draw function
*
*	PPR_WIN_EPS
*	o   calls the caller's draw function
*
*	The idea when using pprWinLoop is that a program
*	will do some preliminary setup for the data to be plotted.
*	Then the program must turn control over to the plot "window
*	manager" using pprWinLoop, which will call the caller's
*	actual plot routine.
*
*	When pprWinLoop exits back to the calling program, that program
*	can call pprWinInfo in order to "remember" the plot window
*	size and position.
*
* RETURNS
*	0, or
*	-1 if an error is encountered
*
* BUGS
* o	doesn't furnish information to the draw function which would allow
*	a partial redraw
* o	terminology is confusing and inconsistent: "draw" function, redraw,
*	replot, repaint, etc.
*
* SEE ALSO
*	pprWinOpen, pprWinInfo
*
* NOTES
* 1.	Even though there aren't any "events" associated with plotting on
*	a PostScript printer, this routine must be called even when using
*	PPR_WIN_POSTSCRIPT and PPR_WIN_EPS, since this routine invokes the
*	caller's "draw" function.
*
* EXAMPLE
*	See pprWinOpen for an example of a replot function.
jjj
*
*-*/
long
pprWinLoop(pWin, drawFun, pDrawArg)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
void	(*drawFun)();/* I pointer to function to draw the plot */
void	*pDrawArg;/* I pointer to pass to drawFun */
{
#ifdef XWINDOWS
    XEvent	anEvent;	/* a window event structure */
#endif

    pWin->drawFun = drawFun;
    pWin->pDrawArg = pDrawArg;
    if (pprWinMap(pWin) != 0)
	return -1;
    if (pWin->winType == PPR_WIN_SCREEN) {
#ifdef SUNVIEW
	window_main_loop(pWin->frame);
#elif defined XWINDOWS
	while (pWin->pDisp != NULL) {
	    XNextEvent(pWin->pDisp, &anEvent);
	    pprWinEvHandler(pWin, &anEvent);
	}
#endif
    }
    else if (pWin->winType == PPR_WIN_POSTSCRIPT ||
						pWin->winType == PPR_WIN_EPS) {
	(pWin->drawFun)(pWin, pWin->pDrawArg);
    }
    else
	PprAssertAlways(0);
    pprWinWrapup(pWin);
    return 0;
}

/*+/internal******************************************************************
* NAME	pprWinMap - create a plotting "window" and map it onto the display
*
* DESCRIPTION
*	This routine actually creates a window on a display device.  This
*	must be done prior to drawing; the PPR_WIN structure must have
*	been initialized by pprWinOpen .
*
*	If the display "device" is a file for PostScript printing, then
*	the "window" which is created isn't actually a window, but the
*	plotting operations of the user routine don't know the difference.
*
* RETURNS
*	0, or
*	-1 if an error is encountered
*
* SEE ALSO
*	pprWinOpen, pprWinLoop, pprWinWrapup
*
* NOTES
* 1.	This routine is called automatically by pprWinLoop, so it should
*	not be called by programs that use pprWinLoop.
*
*-*/
long
pprWinMap(pWin)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
{
#ifdef XWINDOWS
    int		screenNo, x, y, width, height;
    XSizeHints	sizeHints;
#endif
    if (pWin->winType == PPR_WIN_SCREEN) {
#ifdef SUNVIEW
	pWin->frame = window_create(NULL, FRAME,
		FRAME_LABEL, pWin->title,
		FRAME_NO_CONFIRM, 1,
		WIN_EVENT_PROC, pprWinEvHandler,
		WIN_CLIENT_DATA, pWin,
		WIN_X, pWin->x, WIN_Y, pWin->y,
		0);
	window_set(pWin->frame, WIN_CONSUME_PICK_EVENTS, WIN_NO_EVENTS,
		ACTION_OPEN, ACTION_CLOSE,
		ACTION_FRONT, ACTION_BACK,
		WIN_MOUSE_BUTTONS, WIN_UP_EVENTS, 0,
		0);
	pWin->canvas = window_create(pWin->frame, CANVAS,
		WIN_WIDTH, pWin->width,
		WIN_HEIGHT, pWin->height,
		WIN_EVENT_PROC, pprWinEvHandler,
		WIN_CLIENT_DATA, pWin,
                CANVAS_AUTO_SHRINK, TRUE,
                CANVAS_AUTO_EXPAND, TRUE,
                CANVAS_FIXED_IMAGE, FALSE,
                CANVAS_RETAINED, FALSE,
                CANVAS_AUTO_CLEAR, TRUE,
		0);
	pWin->pw = canvas_pixwin(pWin->canvas);
	window_set(pWin->canvas, WIN_CONSUME_PICK_EVENTS, WIN_NO_EVENTS,
		WIN_MOUSE_BUTTONS, WIN_UP_EVENTS, 0,
		0);
	window_fit(pWin->frame);
	window_set(pWin->frame, WIN_SHOW, 1, 0);
	window_set(pWin->canvas, WIN_SHOW, 1, 0);
	pArea->attr.myGC = 0;
	pArea->attr.bgGC = 0;
#elif defined XWINDOWS
	if (pWin->winDispName[0] == '\0')
	    pWin->pDisp = XOpenDisplay((char *)NULL);
	else
	    pWin->pDisp = XOpenDisplay(pWin->winDispName);
	if (pWin->pDisp == NULL) {
	    (void)printf("pprWinMap: XOpenDisplay to %s failed\n",
					XDisplayName(pWin->winDispName));
	    return -1;
	}
	screenNo = DefaultScreen(pWin->pDisp);
	pWin->attr.gc = DefaultGC(pWin->pDisp, screenNo);
	pWin->attr.myGC = 0;
	sizeHints.x = x = pWin->x;
	sizeHints.y = y = pWin->y;
	sizeHints.width = width = pWin->width;
	sizeHints.height = height = pWin->height;
	sizeHints.flags = PSize | PPosition;
	pWin->plotWindow = XCreateSimpleWindow(pWin->pDisp,
			DefaultRootWindow(pWin->pDisp),
			x, y, width, height, 1,
			BlackPixel(pWin->pDisp, screenNo),
			WhitePixel(pWin->pDisp, screenNo));

	XSetStandardProperties(pWin->pDisp, pWin->plotWindow,
			pWin->title, pWin->title,
			None, 0, NULL, &sizeHints);
	XSelectInput(pWin->pDisp, pWin->plotWindow,
		ExposureMask | ButtonPressMask | ButtonReleaseMask);
	XMapWindow(pWin->pDisp, pWin->plotWindow);
	pWin->attr.gcBG = XCreateGC(pWin->pDisp, pWin->plotWindow, 0, NULL);
	XCopyGC(pWin->pDisp, pWin->attr.gc,
				GCBackground|GCForeground, pWin->attr.gcBG);
	XSetFunction(pWin->pDisp, pWin->attr.gcBG, GXclear);
	pWin->attr.bgGC = 1;
#endif
    }
    else if (pWin->winType == PPR_WIN_POSTSCRIPT ||
						pWin->winType == PPR_WIN_EPS) {
	;	/* no action */
    }
    else
	PprAssertAlways(0);

    return 0;
}

/*+/subr**********************************************************************
* NAME	pprWinOpen - initialize a plotting "window" structure
*
* DESCRIPTION
*	Initialize a plot window structure.  This is the structure which
*	keeps track of the information needed to interact with the
*	device on which the "window" resides.  The possible types of
*	windows are:
*
*	o   PPR_WIN_SCREEN  selects a window on a visual display.  This
*	    will be a SunView or X window, depending on the version of
*	    the plotting used in linking the program.  The pprWinOpen
*	    call actually creates a window, with the root window as the
*	    parent.  The window thus created can be moved, resized, etc.
*	    according to the capabilities of the window manager of the
*	    windowing system being used.  (To initialize a plotting
*	    window structure in a window which already exists, use
*	    pprWinOpenUW.)
*
*	    for SunView,
*		`winDispName' is ignored and should be NULL
*		`winTitle' appears on the window's title bar and icon
*		`xPos' and `yPos' are pixel offsets from the upper left
*			corner of the root window.  If values of 0 are
*			supplied, then the window will be positioned at
*			100,100.
*		`xWid' and `yHt' are the width and height of the window,
*			in pixels.  If values of 0 are supplied, then
*			a size of 512,512 will be used.
*
*	    for X11,
*	    	`winDispName' specifies the X11 display name on which the
*			plot window is to appear.  If NULL, then the
*			DISPLAY environment variable will be used.
*		`winTitle' appears on the window's title bar and icon
*		`xPos' and `yPos' are pixel offsets from the upper left
*			corner of the root window.  If values of 0 are
*			supplied, then the window will be positioned at
*			100,100.
*		`xWid' and `yHt' are the width and height of the window,
*			in pixels.  If values of 0 are supplied, then
*			a size of 512,512 will be used.
*
*	o   PPR_WIN_POSTSCRIPT  selects a "window" on a PostScript
*	    printer.
*
*		`winDispName' is the name of a file to receive PostScript
*			output.  This is the file which will eventually
*			be sent to a PostScript printer to get a hard
*			copy of the plot.  If the file exists when
*			pprWinOpen is called, its contents are erased
*			before writing begins.
*		`winTitle' is ignored and should be NULL
*		`xPos' and `yPos' are ignored
*		`xWid' and `yHt' are the width and height of the window,
*			in pixels.  If values of 0 are supplied, then
*			a size of 512,512 will be used.  If the size is
*			larger than the page, then the plot will be scaled
*			to fit.  If necessary, the plot will be printed in
*			landscape mode, rather than the default of portrait
*			mode.
*
*	o   PPR_WIN_EPS  selects a "window" in an Encapsulated PostScript
*	    file.  EPS files are intended to be included into documents
*	    prepared by word processing programs such as Interleaf and
*	    TeX.  EPS files will not print directly on a PostScript printer.
*	    The description of the arguments for PPR_WIN_POSTSCRIPT applies,
*	    except that scaling and rotation aren't done.
*
*	The pprXxx routines can be used for several different styles of
*	usage:
*
*	o   a complete set of data is available prior to calling any of
*	    the plotting routines.  In this case, grids can be drawn and
*	    data can be plotted at the same time.
*
*	    For this style of usage, the typical usage will be:
*		pprWinOpen		to "open" a plot window
*		pprWinLoop		to map the plot window to the screen
*					and call the caller's replot function
*					when the window is exposed, resized,
*					etc.
*		pprWinClose		to "close" a plot window
*
*	o   no data (or only part of the data) is available prior to calling
*	    any of the plotting routines.  In this case, at least some of
*	    the data must be plotted at a later time than when the grids
*	    are drawn.  The pprXxx routines don't automatically support
*	    this style of usage fully, but they do provide some tools which
*	    make it relatively easy to implement.  See pprWinOpenUW for
*	    more details.
*
*	Under all circumstances, the calling program is expected to call
*	pprWinClose when plotting is complete for the plot window.
*
* RETURNS
*	pointer to window structure, or
*	NULL if an error is encountered
*
* SEE ALSO
*	pprWinOpenUW, pprWinClose, pprWinInfo, pprWinLoop
*
* EXAMPLES
* 1.	Plot an existing set of data, where the data is stored in `dataStruct'.
*
*	PPR_WIN	*pWindow;
*	  .
*	pWindow = pprWinOpen(PPR_WIN_SCREEN, NULL, "test", 0, 0, 0, 0);
*	if (pWindow == NULL)
*	    abort();
*	if (pprWinLoop(pWindow, replot, &dataStruct) != 0)
*	    abort();
*	pprWinClose(pWindow);
*
*-*/
PPR_WIN *
pprWinOpen(winType, winDispName, winTitle, xPos, yPos, xWid, yHt)
PPR_WIN_TY winType;	/* I type of plot window: PPR_WIN_xxx */
char	*winDispName;	/* I name of "display" or file for window */
char	*winTitle;	/* I title for window title bar and icon */
int	xPos;		/* I x position for window; 0 for default */
int	yPos;		/* I y position for window; 0 for default */
int	xWid;		/* I width of window; 0 for default */
int	yHt;		/* I height of window; 0 for default */
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */

    if ((pWin = (PPR_WIN *)malloc(sizeof(PPR_WIN))) == NULL) {
	(void)printf("pprWinOpen: couldn't malloc plot window struct\n");
	return NULL;
    }
    pWin->pAreaHead = NULL;
    pWin->pAreaTail = NULL;
    pWin->winType = winType;
    pWin->drawFun = NULL;
    pWin->pDrawArg = NULL;
    if (xPos != 0)
	pWin->x = xPos;
    else if (winType == PPR_WIN_POSTSCRIPT || winType == PPR_WIN_EPS)
	pWin->x = 0;
    else
	pWin->x = 100;
    if (yPos != 0)
	pWin->y = yPos;
    else if (winType == PPR_WIN_POSTSCRIPT || winType == PPR_WIN_EPS)
	pWin->y = 0;
    else
	pWin->y = 100;
    if (xWid != 0)
	pWin->width = xWid;
    else if (winType == PPR_WIN_POSTSCRIPT || winType == PPR_WIN_EPS)
	pWin->width = 0;
    else
	pWin->width = 512;
    if (yHt != 0)
	pWin->height = yHt;
    else if (winType == PPR_WIN_POSTSCRIPT || winType == PPR_WIN_EPS)
	pWin->height = 0;
    else
	pWin->height = 512;

    if (winDispName == NULL)
	pWin->winDispName[0] = '\0';
    else if (strlen(winDispName) >= 120) {
	(void)printf("pprWinOpen: plot file or display name too long\n");
	free((char *)pWin);
	return NULL;
    }
    else
	(void)strcpy(pWin->winDispName, winDispName);

    if (winTitle == NULL)
	pWin->title[0] = '\0';
    else if (strlen(winTitle) >= 80) {
	(void)printf("pprWinOpen: winTitle too long\n");
	free((char *)pWin);
	return NULL;
    }
    else
	(void)strcpy(pWin->title, winTitle);

    pWin->attr.lineThick = 1;
    pWin->attr.ltCurr = -1;
    pWin->attr.pPatt = NULL;
    pWin->attr.myGC = 0;
    pWin->attr.bgGC = 0;

/*-----------------------------------------------------------------------------
* PostScript printer initialization
*	o   for PostScript,
*	    -   send the %!PS line
*	o   for Encapsulated PostScript,
*	    -   send the %!PS,EPS line
*	    -   send the %%BoundingBox line
*	o   send the various PostScript programs needed
*	o   translate, rotate, and scale the PostScript axis as needed for
*	    the most recent size the window had.
*----------------------------------------------------------------------------*/
    if (winType == PPR_WIN_POSTSCRIPT || winType == PPR_WIN_EPS) {
	int		i;
	int		nLines;
	double		scale, xscale, yscale;

	if ((pWin->file = fopen(winDispName, "w")) == NULL) {
	    perror("opening PostScript or EPS file");
	    PprAssertAlways(0);
	}
	if (winType == PPR_WIN_POSTSCRIPT)
	    (void)fprintf(pWin->file, "%%!PS\n");
	else {
	    (void)fprintf(pWin->file, "%%!PS-Adobe-2.0 EPSF-1.2\n");
	    (void)fprintf(pWin->file, "%%%%BoundingBox: 0 0 %d %d\n",
					pWin->width - 1, pWin->height - 1);
	}

/*-----------------------------------------------------------------------------
*	write PostScript programs and defaults to file
*----------------------------------------------------------------------------*/
	nLines = sizeof(pprPSProg)/sizeof(char *);
	for (i=0; i<nLines; i++)
	    (void)fprintf(pWin->file, "%s\n", pprPSProg[i]);
	if (winType == PPR_WIN_POSTSCRIPT) {
	    if (pWin->width <= 560 && pWin->height <= 720) {
		(void)fprintf(pWin->file, "%d %d translate\n",
			(int)((612-pWin->width)/2), (760-pWin->height));
	    }
	    else if (pWin->width <= 720 && pWin->height <= 560) {
		(void)fprintf(pWin->file, "612 0 translate 90 rotate\n");
		(void)fprintf(pWin->file, "%d %d translate\n",
			(int)((792-pWin->width)/2), (560-pWin->height));
	    }
	    else if (pWin->width <= pWin->height) {
		xscale = 560. / (double)pWin->width;
		yscale = 720. / (double)pWin->height;
		if (xscale <= yscale)
		    scale = xscale;
		else
		    scale = yscale;
		(void)fprintf(pWin->file, "%.3f %.3f scale\n", scale, scale);
		(void)fprintf(pWin->file, "%d %d translate\n",
			(int)((612./scale-pWin->width)/2.),
			(int)(760./scale-pWin->height));
	    }
	    else {
		xscale = 560. / (double)pWin->width;
		yscale = 720. / (double)pWin->height;
		if (xscale <= yscale)
		    scale = xscale;
		else
		    scale = yscale;
		(void)fprintf(pWin->file, "612 0 translate 90 rotate\n");
		(void)fprintf(pWin->file, "%.3f %.3f scale\n", scale, scale);
		(void)fprintf(pWin->file, "%d %d translate\n",
			(int)((792./scale-pWin->width)/2.),
			(int)(560./scale-pWin->height));
	    }
	}
    }
    return pWin;
}

/*+/subr**********************************************************************
* NAME	pprWinOpenUW - open a plot "window" to an existing User Window
*
* DESCRIPTION
*	Initialize a plot window structure.  This is the structure which
*	keeps track of the information needed to interact with the
*	device on which the "window" resides.
*
*	This routine is for use when the caller already has a window in
*	the windowing system.  It is for use exclusively for PPR_WIN_SCREEN
*	plot window type.  The form of the call to this routine is heavily
*	dependent on the windowing system being used.  This routine provides
*	the basis for obtaining `asynchronous' plotting.  (It can also be
*	used for `batched' plotting, in which all data is available prior
*	to the first plotting call.)
*
*	Under all circumstances, the calling program is expected to call
*	pprWinClose when plotting is complete for the plot window.
*
* RETURNS
*	pointer to window structure, or
*	NULL if an error is encountered
*
* SEE ALSO
*	pprWinOpenUW, pprWinClose, pprWinInfo, pprWinLoop
*
* EXAMPLES
* 1.	for X11
*
*	PPR_WIN *pWin;
*	Display *pDisp;
*	Window	plotWindow;
*	GC	plotGC;
*
*	pWin = pprWinOpenUW(&pDisp, &plotWindow, &plotGC, NULL);
*	...
*	pprWinReplot(pWin, drawFn, drawArg);
*	...
*	pprWinClose(pWin);
*
*
* 2.	for SunView
*
*	PPR_WIN	*pWin;
*	Frame	plotFrame;
*	Canvas	plotCanvas;
*
*	pWin = pprWinOpenUW(&plotFrame, &plotCanvas, NULL, NULL);
*	...
*	pprWinReplot(pWin, drawFn, drawArg);
*	...
*	pprWinClose(pWin);
*
* 3.	for XView
*	(Since XView doesn't have any built-in line drawing capabilities,
*	some fussing is needed to get the X11 items needed for plotting.
*	In the following, pDisp, plotWindow, and plotGC are X11 items which
*	are obtained from XView items.)
*
*	PPR_WIN	*pWin;
*	Display	*pDisp;
*	Window	plotWindow;
*	GC	plotGC;
*
*	pDisp = (Display *)xv_get(frame, XV_DISPLAY);
*	plotWindow = (Window)xv_get(canvas_paint_window(canvas), XV_XID);
*	plotGC = DefaultGC(pDisp, DefaultScreen(pDisp));
*
*	pWin = pprWinOpenUW(&pDisp, &plotWindow, &plotGC, NULL);
*	...
*	pprWinReplot(pWin, drawFn, drawArg);
*	...
*	pprWinClose(pWin);
*
*-*/
PPR_WIN *
pprWinOpenUW(pArg1, pArg2, pArg3, pArg4)
void	*pArg1;
void	*pArg2;
void	*pArg3;
void	*pArg4;
{
    PPR_WIN	*pWin;		/* pointer to plot window structure */

    if ((pWin = (PPR_WIN *)malloc(sizeof(PPR_WIN))) == NULL) {
	(void)printf("pprWinOpenUW: couldn't malloc plot window struct\n");
	return NULL;
    }
    pWin->pAreaHead = NULL;
    pWin->pAreaTail = NULL;
    pWin->winType = PPR_WIN_SCREEN;
    pWin->drawFun = NULL;
    pWin->pDrawArg = NULL;
    pWin->winDispName[0] = '\0';
    pWin->title[0] = '\0';
#ifdef SUNVIEW
    pWin->frame = *(Frame *)pArg1;
    pWin->canvas = *(Canvas *)pArg2;
    pWin->pw = canvas_pixwin(pWin->canvas);
    pWin->attr.bgGC = 0;
#elif defined XWINDOWS
    pWin->pDisp = *(Display **)pArg1;
    pWin->plotWindow = *(Window *)pArg2;
    pWin->attr.gc = *(GC *)pArg3;
    pWin->attr.myGC = 0;
    pWin->attr.gcBG = XCreateGC(pWin->pDisp, pWin->plotWindow, 0, NULL);
    XCopyGC(pWin->pDisp, pWin->attr.gc,
				GCBackground|GCForeground, pWin->attr.gcBG);
    XSetFunction(pWin->pDisp, pWin->attr.gcBG, GXclear);
    pWin->attr.bgGC = 1;
#endif
    pprWinAttr(pWin);

    pWin->attr.lineThick = 1;
    pWin->attr.ltCurr = -1;
    pWin->attr.pPatt = NULL;

    return pWin;
}

/*+/subr**********************************************************************
* NAME	pprWinReplot - redraw a plot in a user owned window
*
* DESCRIPTION
*	Calls the "replot" function to repaint the plot window.  This
*	routine is intended to be used with "user owned" plot windows
*	which have been opened with pprWinOpenUW.
*
*	Prior to calling the replot function, this routine determines
*	the size of the actual window and rescales the existing plot
*	areas.
*
* RETURNS
*	void
*
* SEE ALSO
*	pprWinOpenUW
*
*-*/
void
pprWinReplot(pWin, pFunc, pArg)
PPR_WIN	*pWin;
void	(*pFunc)();
void	*pArg;
{
    int		width, height;
    PPR_AREA	*pArea;

    width = pWin->width;
    height = pWin->height;
    pprWinAttr(pWin);
    if (pWin->width != width || pWin->height != height) {
	pArea = pWin->pAreaHead;
	while (pArea != NULL) {
	    pprAreaRescale(pArea, pArea->xLeft, pArea->yBot,
						pArea->xRight, pArea->yTop);
	    pArea = pArea->pNext;
	}
    }
    pWin->drawFun = pFunc;
    pWin->pDrawArg = pArg;
    (pWin->drawFun)(pWin, pWin->pDrawArg);
    pprWinWrapup(pWin);
}

/*+/internal******************************************************************
* NAME	pprWinWrapup - wrapup plotting operations
*
* DESCRIPTION
*	Wrap up plotting operations.
*
* RETURNS
*	void
*
* SEE ALSO
*	pprWinMap, pprWinOpen, pprWinLoop, pprWinOpenUW, pprWinReplot
*
* NOTES
* 1.	This routine is called automatically by pprWinLoop.  It should be
*	used only by programs which don't use pprWinLoop.
*
*-*/
void
pprWinWrapup(pWin)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
{
    if (pWin->winType == PPR_WIN_POSTSCRIPT)
	(void)fprintf(pWin->file, "showpage");
    if (pWin->winType == PPR_WIN_POSTSCRIPT || pWin->winType == PPR_WIN_EPS) {
	(void)fclose(pWin->file);
	pWin->file = NULL;
    }
}

/*+/subr**********************************************************************
* NAME	pprYFracToXFrac - convert a Y fraction to an X fraction
*
* DESCRIPTION
*	Converts a value which is a fraction of window height into a value
*	which is a fraction of window width, so that the two values will
*	represent the same physical size, in pixels.
*
*	This routine is useful for laying out a plot area, especially when
*	a grid with annotations and labels is to be used.  The choice of
*	"data area" size (i.e., the size of the grid) depends on the size
*	of the characters which will be used for the annotations and
*	labels.
*
* RETURNS
*	X fraction
*
* SEE ALSO
*	pprText
*
* EXAMPLE
*	A plot area is use the full width and the vertical 1/3 of the window.
*	x values range from 0 to 100, while y is from -10 to 10.  The x
*	axis is to be divided into 10 divisions, the y into 4.  Use
*	the default character size for annotations and labels.  Also,
*	use the default window position and size.
*
*	PPR_WIN	*pWin;
*	PPR_AREA *pArea;
*	float	charHt;		height as a fraction of window height
*	float	charWid;	width as a fraction of window width
*
*	charHt = PprDfltCharHt(.33, .67);
*	charWid = pprYFracToXFrac(pWin, charHt);
*	pArea = pprAreaOpen(pWin, 12.*charWid, .33+6.*charHt, 1., .67,
*			0., -10., 100., 10, 4, charHt);
*
*-*/
double
pprYFracToXFrac(pWin, yFrac)
PPR_WIN	*pWin;	/* I pointer to plot window structure */
double	yFrac;	/* I fraction of window height */
{
    return (yFrac * pWin->height / pWin->width);
}

/*+/internal******************************************************************
* NAME	pprSleep - simple sleep routine to use with plots
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
static void
pprSleep(seconds, usec)
int	seconds;	/* I number of seconds (added to usec) to sleep */
int	usec;		/* I number of micro-sec (added to sec) to sleep */
{
#ifndef vxWorks
    usleep((unsigned)(seconds*1000000 + usec));
#else
    int		ticks;
    static int ticksPerSec=0;
    static int usecPerTick;

    if (ticksPerSec == 0) {
	ticksPerSec = sysClkRateGet();
	usecPerTick = 1000000 / ticksPerSec;
    }

    ticks = seconds*ticksPerSec + usec/usecPerTick + 1;
    taskDelay(ticks);
#endif
}
